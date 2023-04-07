#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <stdbool.h>
#include <signal.h> //Not used
#include <sys/select.h>
#include <pthread.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define BUFFER_SIZE 1024 // Tocar aixo

// TEMPORITZADORS (EN SEGONS)
#define T 1 // Temps màxim de resposta
#define P 2 // Número de paquets
#define Q 3
#define U 2
#define N 6
#define O 2 // Processos de registre
#define R 2 // Envia ALIVE_INF cada R segons
#define U_2 3 // Número máxim de paquets ALIVE_INF sense rebre ALIVE_ACK

// DEFINIM ELS POSSIBLES ESTATS DEL EQUIP
#define DISCONNECTED 0xA0 // Equip desconnectat
#define WAIT_REG_RESPONSE 0xA2 // Espera de resposta a la petició de registre
#define WAIT_DB_CHECK 0xA4 // Espera de consulta BB. DD. d’equips autoritzats
#define REGISTERED 0xA6 // Equip registrat, sense intercanvi ALIVE
#define SEND_ALIVE 0xA8 // Equip enviant i rebent paquets de ALIVE

// TIPUS DE PAQUET FASE DE REGISTRE
#define REGISTER_REQ 0x00 // Petició de resgistre
#define REGISTER_ACK 0x02 // Acceptació de registre
#define REGISTER_NACK 0x04 // Denegació de registre
#define REGISTER_REJ 0x06 // Rebuig de registre
#define ERROR 0x0F // Error de protocol

// TIPUS DE PAQUET MANTENIMENT DE COMUNICACIÓ
#define ALIVE_INF 0x10
#define ALIVE_ACK 0x12
#define ALIVE_NACK 0x14
#define ALIVE_REJ 0x16

// DEFINIM VARIABLES PER A STRINGS
#define MAX_INPUT 20

// ESTATS INICIALS
int current_state = DISCONNECTED;
bool debug = false;

// CONFIGURACIÓ ADICIONAL (S'HA D'ACTIVAR MANUALMENT)
bool show_local_time = true;
bool show_client_info = false; // Mostra la informació rebuda per l'arxiu de configuració
bool show_exit_status = false;

// DEFINIM VARIABLES DE SORTIDA
#define EXIT_SUCCESS 0
#define EXIT_FAIL -1

// ARXIU DE CONFIGURACIÓ
static char *Id = NULL;
char *MAC = NULL;
char *NMS_Id = NULL;
int NMS_UDP_Port = 0;

// ESTRUCTURA D'UN PAQUET (EN BYTES)
struct Package {
	unsigned char type;
    char id[7];
    char mac[13];
    char random_number[7];
    char data[50];
};

// VARIABLES PER A L'ENVIAMENT DE PAQUETS DEL CLIENT
int register_attempts_left = O;
int consecutive_inf_without_ack = 0;

int udp_socket;
struct sockaddr_in server_addr;
char buffer[BUFFER_SIZE];

char *strdup(const char *); // Inicialitzem strdup per a poder usarla
void change_state(int new_state);
int open_socket(int protocol);
void send_register_request();
void proccess_register(int udp_socket);
void wait_for_ack();
// FASE DE COMUNICACIÓ RECURRENT
void concurrent_comunication();
void send_alive_inf();
void process_alive();
// FI DE FASE DE COMUNICACIÓ RECURRENT
char *get_local_address(char *str);
char *random_number();

// DEFINIM LES VARIABLES AUXILIARS
void *wait_quit(void *arg);
int get_type_from_str(char *str);
char *get_pdu_type(int type);
void read_client_config(char *config_file); // read_config_file ptsr millor nom
void print_client_info();

// Funcions decoratives TODO
void println(char *str); 
void print_time(); // Printar l'hora actual
void print_state(char *str, int current_state); // Printar canvis d'estat
void print_error(char *str_given); // Printar errors
void printd(char *str_given); // Printar debugs
void exit_program(int EXIT_STATUS);
void print_bar();

/*
void send_message(int udp_socket, const char *message) {
	ssize_t sent = send(udp_socket, message, strlen(message), 0);
	if (sent < 0){
		if (debug) {
			perror("Error sending message");
		//exit(EXIT_FAIL);
		}
	}
}
*/


ssize_t receive_message(int udp_socket, char *buffer, size_t size) {
	ssize_t received = recv(udp_socket, buffer, size - 1, 0);
	if (received < 0){
		if (debug) {
			perror("Error receiving message");
		//exit(EXIT_FAIL);
		}
	}
	else{
		print_state("S'ha rebut el missatge amb éxit", REGISTERED); // TESTING
	}
	buffer[received] = '\0';
	return received;
}

int main(int argc, char *argv[]) {
	char *config_file = NULL;

	pthread_t wait_quit_thread; // Creem el fil per a l'espera de la comanda
	pthread_create(&wait_quit_thread, NULL, wait_quit, NULL); // Iniciem el fil concurrent

	for (int i = 1; i < argc; i++){
		if (strcmp(argv[i], "-d") == 0){
			debug = true;
		}
		else if (strcmp(argv[i], "-c") == 0 && i + 1 < argc){ // Mirem si s'ha proporcionat el parametre -c seguit del nom del fitxer
			i++;
			config_file = argv[i];
		}
		else if (strcmp(argv[i], "-f") == 0 && i + 1 < argc){
			i++;
			config_file = argv[i];
		}
		else{
			fprintf(stderr, "Ús: %s [-d] [-c <config_file.cfg>]\n", argv[0]);
			exit_program(EXIT_SUCCESS); // És una sortida controlada del programa
		}
	}

	if (debug) {
		print_bar();
		printf("\t\t\tMode debug activat\n");
		print_bar();
	}

	// Estat inicial
	print_state("Equip passa a l'estat", current_state);

	read_client_config(config_file);

	send_register_request();

	close(udp_socket);
	
	/* S'hauria d'usar quan ja no se vulguin més
	free(NMS_Id);
    free(MAC);
	*/
	pthread_join(wait_quit_thread, NULL); // Esperem que acabi el fil
	return 0;
}

int open_socket(int protocol) {
    int socket_type;

    if (protocol == IPPROTO_UDP) {
        socket_type = SOCK_DGRAM; // Protocol UDP
    } else {
        socket_type = SOCK_STREAM; // Protocol TCP
    }

    int udp_socket = socket(AF_INET, socket_type, protocol);

    if (udp_socket < 0) {
		printd("Ha sorgit un error al crear el socket");
        exit_program(EXIT_FAIL);
    }

    struct sockaddr_in local_addr;
    memset(&local_addr, 0, sizeof(local_addr));
    local_addr.sin_family = AF_INET;
    local_addr.sin_port = htons(0); // 0 permet que el sistema escolleixi un port disponible
    local_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(udp_socket, (struct sockaddr *)&local_addr, sizeof(local_addr)) < 0) {
        printd("Error al fer el binding");
        exit_program(EXIT_FAIL);
    }

    return udp_socket;
}

void send_register_request() {
	// Obrim un socket UDP
	udp_socket = open_socket(IPPROTO_UDP); // Especifiquem que volem crear el socket en UDP

	struct Package register_request;
	register_request.type = get_type_from_str("REGISTER_REQ");

	strcpy(register_request.id, Id);
    strcpy(register_request.mac, MAC);
    strcpy(register_request.random_number, random_number());
    strcpy(register_request.data, "");

    struct sockaddr_in server_addr_udp;
	memset(&server_addr_udp, 0, sizeof(server_addr_udp));
    server_addr_udp.sin_family = AF_INET;
    server_addr_udp.sin_port = htons(NMS_UDP_Port); // Cuidao amb el htons
    server_addr_udp.sin_addr.s_addr = inet_addr(get_local_address(NMS_Id));

    ssize_t sent = sendto(udp_socket, &register_request, sizeof(register_request), 0, (struct sockaddr *) &server_addr_udp, sizeof(server_addr_udp));
	
	//sleep(5);

	if (sent < 0) {
        printd("Error a l'enviar la petició de registre");
		exit_program(EXIT_FAIL);
    }
    change_state(WAIT_REG_RESPONSE);

	proccess_register(udp_socket);

}

// GESTIÓ DEL ACK, NACK o REJECT
void proccess_register(int udp_socket) {
	struct sockaddr_in sender_addr;
	socklen_t sender_addr_len = sizeof(sender_addr);
	struct Package received_package;

	ssize_t received = recvfrom(udp_socket, &received_package, sizeof(received_package), 0, (struct sockaddr *) &sender_addr, &sender_addr_len);

	if (received < 0) {
		if (debug) {
			print_error("Error al rebre el paquet");
		}

	}
	sleep(2);

	//printf("%i\n",received_package.type);
	// TESTING CONCURRENT
	change_state(REGISTERED);
	concurrent_comunication();

	switch (received_package.type) {
		case REGISTER_ACK: // REGISTER_ACK (0x02)
			printd("S'ha rebut un REGISTER_ACK");
			change_state(REGISTERED);
			concurrent_comunication();
		case REGISTER_NACK: // REGISTER_NACK (0x04)
			printd("S'ha rebut un REGISTER_NACK");
			if (register_attempts_left > 0) {
				printd("Fem un altre request"); // Eliminar
				send_register_request();
				register_attempts_left--;
			} else {
				printd("No queden més intents de registre");
			}
		case REGISTER_REJ:
			printd("S'ha rebut un REGISTER_REJ");
			change_state(DISCONNECTED);
			exit_program(EXIT_SUCCESS);
		default:
			printd("Ha hagut un error de protocol");
			change_state(DISCONNECTED);
			exit_program(EXIT_SUCCESS);
			
	}
}

// TRACTAMENT DE COMUNICACIÓ PERIÒDICA AMB EL SERVIDOR
void concurrent_comunication() {
	if (current_state == REGISTERED) {
		while (true) { // while (current_state == REGISTERED) ??
			send_alive_inf();
			change_state(SEND_ALIVE);

			fd_set read_fds;
			struct timeval timeout;
			timeout.tv_sec = R;
			timeout.tv_usec = 0;

			FD_ZERO(&read_fds);
			FD_SET(udp_socket, &read_fds);

			int activity = select(udp_socket + 1, &read_fds, NULL, NULL, &timeout);
			if (activity > 0) {
				process_alive();
			} else if (activity == 0) {
				consecutive_inf_without_ack++;
				if (consecutive_inf_without_ack >= U_2) {
					change_state(DISCONNECTED);
					send_register_request();
				}
			}

			if (current_state == DISCONNECTED) {
				break;
			}
		}
	}
}

void send_alive_inf() {
    struct Package alive_inf;
    alive_inf.type = ALIVE_INF;
    strcpy(alive_inf.id, Id);
    strcpy(alive_inf.mac, MAC);
    strcpy(alive_inf.random_number, random_number());
    strcpy(alive_inf.data, "");

    ssize_t alive_sent = sendto(udp_socket, &alive_inf, sizeof(alive_inf), 0, (struct sockaddr *) &server_addr, sizeof(server_addr));
    if (alive_sent < 0) {
        printd("Error l'enviar ALIVE_INF");
    }
}

void process_alive() {
    struct sockaddr_in sender_addr;
    socklen_t sender_addr_len = sizeof(sender_addr);
    struct Package received_package;

    ssize_t received = recvfrom(udp_socket, &received_package, sizeof(received_package), 0, (struct sockaddr *) &sender_addr, &sender_addr_len);
    if (received < 0) {
        if (debug) {
            print_error("Error al rebre el paquet");
        }
    }

    switch (received_package.type) {
        case ALIVE_ACK:
            if (strcmp(received_package.id, Id) == 0 && strcmp(received_package.mac, MAC) == 0) {
                consecutive_inf_without_ack = 0; // Reset counter
            }
            break;
        case ALIVE_NACK:
            // No es fa res ja que es considera com no haver rebut resposta del servidor
            break;
        case ALIVE_REJ:
            change_state(DISCONNECTED);
            send_register_request();
            break;
    }
}

// FI DE TRACTAMENT DE COMUNCIACIÓ PERIÒDICA ABM EL SERVIDOR




void *wait_quit(void *arg) {
	char input[MAX_INPUT];
	while (1) {
        fgets(input, MAX_INPUT, stdin);
        input[strcspn(input, "\n")] = 0; // Eliminem el salt de linea
        if (strcmp(input, "quit") == 0) {
			exit_program(EXIT_SUCCESS);
        }
    }
    return NULL;
}

// Retorna una cadena de caràcters de tamany maxim 7 bytes
char *random_number() {
    srand(time(NULL));

    int length = rand() % 7 + 1;

    // Reservem memòria per a la cadena i el caràcter nul final
    char *number = (char *)malloc(length + 1);

    // Generem cada dígit aleatoriament i l'afegim a la cadena
    for (int i = 0; i < length; i++) {
        number[i] = '0' + rand() % 10;
    }

    // Afegim el caràcter nul al final de la cadena
    number[length] = '\0';

    return number;
}

void read_client_config(char *config_file) { // Si se passa per paràmetre un altre arxiu s'agafa aquell
    FILE *file;
    if (config_file) {
        file = fopen(config_file, "r");
		printd("S'ha carregat l'arxiu de configuració");
    } else {
        file = fopen("client.cfg", "r");
		printd("S'ha carregat l'arxiu de configuració per defecte");
    }

	if (file == NULL) {
        println("Error al carregar l'arxiu de configuració");
		exit_program(EXIT_FAIL);
    }

    char line[64];
	while (fgets(line, sizeof(line), file) != NULL) {
		if (strncmp(line, "Id", 2) == 0) {
			Id = malloc(16 * sizeof(char));
			sscanf(line, "Id %15s", Id);
		} else if (strncmp(line, "MAC", 3) == 0) {
			MAC = malloc(13 * sizeof(char));
			sscanf(line, "MAC %12s", MAC);
		} else if (strncmp(line, "NMS-Id", 6) == 0) {
			NMS_Id = malloc(64 * sizeof(char));
			sscanf(line, "NMS-Id %63s", NMS_Id);
		} else if (strncmp(line, "NMS-UDP-port", 12) == 0) {
			sscanf(line, "NMS-UDP-port %d", &NMS_UDP_Port);
		}
	}

    fclose(file);

	if (show_client_info) {;
		print_client_info();
	}
}

// FUNCIÓ QUE RETORNA LA IP DEL LOCALHOST EN CAS QUE SIGUI LA QUE S'HA LLEGIT, SINÓ ES RETORNA LA ORGINAL
char *get_local_address(char *str) {
    if (NMS_Id && strcmp(NMS_Id, "localhost") == 0) {
        return "127.0.0.1";
    } else {
        return str;
    }
}

void print_client_info() {
	printd("La informació obtinguda de l'arxiu de configuració ha estat:");
	print_time();
	printf("Id: %s\n", Id);
	print_time();
	printf("MAC: %s\n", MAC);
	print_time();
	printf("NMS-Id: %s\n", NMS_Id);
	print_time();
	printf("NMS_UDP_Port: %i\n", NMS_UDP_Port);
}

void change_state(int new_state) {
    if (current_state != new_state) {
        current_state = new_state;
        print_state("Equip passa a l'estat", current_state);
    }
}

int get_type_from_str(char *str) {
    for (int i = 0; i != -1; i++) {
        char *type_str = get_pdu_type(i);
        if (strcmp(type_str, "DESCONEGUT") == 0) {
            break;
        }
        if (strcmp(str, type_str) == 0) {
            return i;
        }
    }
    return -1; // Si no se encuentra el tipo en la lista, retorna -1
}


char *get_pdu_type(int type) {
    switch (type) {
		// FASE DE REGISTRE
        case 0x00: // Petició de registre
            return "REGISTER_REQ";
        case 0x02: // Acceptació de registre
            return "REGISTER_ACK";
        case 0x04: // Denegació de registre
            return "REGISTER_NACK";
        case 0x06: // Rebuig de registre
            return "REGISTER_REJ";
		case 0x0F: // Error de protocol
			return "ERROR";

		// ESTATS D'UN EQUIP
        case 0xA0: // Equip desconnectat
            return "DISCONNECTED";
        case 0xA2: // Espera de resposta a la petició de registre
            return "WAIT_REG_RESPONSE";
        case 0xA4: // Espera de consulta BB. DD. d’equips autoritzats
            return "WAIT_DB_CHECK";
        case 0xA6: // Equip registrat, sense intercanvi ALIVE
            return "REGISTERED";
        case 0xA8: // Equip enviant i rebent paquets de ALIVE
            return "SEND_ALIVE";

		// TIPUS DE PAQUET MANTENIMENT DE COMUNICACIÓ
        case 0x10: // Enviament d’informació d’alive
            return "ALIVE_INF";
        case 0x12: // Confirmació de recepció d’informació d’alive
            return "ALIVE_ACK";
        case 0x14: // Denegació de recepció d’informació d’alive
            return "ALIVE_NACK";
        case 0x16: // Rebuig de recepció d’informació d’alive
            return "ALIVE_REJ";

		// TIPUS PAQUET ENVIAMENT ARXIU
        case 0x20: // Petició d’enviament d’arxiu de configuració
            return "SEND_FILE";
        case 0x22: // Bloc de dades de l’arxiu de configuració
            return "SEND_DATA";
        case 0x24: // Acceptació de la petició d’enviament d’arxiu de configuració
            return "SEND_ACK";
        case 0x26: // Denegació de la petició d’enviament d’arxiu de configuració
            return "SEND_NACK";
        case 0x28: // Rebuig de la petició d’enviament d’arxiu de configuració
            return "SEND_REJ";
        case 0x2A: // Fi de l’enviament de dades de l’arxiu de configuració
            return "SEND_END";
        default: // Cas desconegut
            return "DESCONEGUT";
    }
}

///////////////////////////////// FUNCIONS PER PRINTAR /////////////////////////////////

void get_time(char *time_str){
	time_t t = time(NULL);
	struct tm tm = *localtime(&t);

	sprintf(time_str, "%02d:%02d:%02d:", tm.tm_hour, tm.tm_min, tm.tm_sec);
}

void print_time() {
	if (show_local_time) {
		char time_str[10];
		get_time(time_str);
		printf("%s ", time_str);
	}
}


void print_state(char *str_given, int current_state){
	char current_state_str[strlen("WAIT_REG_RESPONSE") + 1];

	// Creem un diccionari per a cada estat
	switch (current_state){
		case WAIT_REG_RESPONSE:
			strcpy(current_state_str, "WAIT_REG_RESPONSE");
			break;

		case WAIT_DB_CHECK:
			strcpy(current_state_str, "WAIT_DB_CHECK");
			break;

		case REGISTERED:
			strcpy(current_state_str, "REGISTERED");
			break;

		case SEND_ALIVE:
			strcpy(current_state_str, "SEND_ALIVE");
			break;

		default:
			strcpy(current_state_str, "DISCONNECTED");
			break;
	}

	print_time();
	printf("MSG.  =>  %s: %s\n", str_given, current_state_str);
}

void println(char *str) {
	print_time();
	printf("%s\n", str);
}

void print_error(char *str_given) {
	print_time();
	if (debug) {
		printf("(DEBUG): ERROR.  =>  %s", str_given);
	}
	printf("ERROR.  =>  %s", str_given);
}

void printd(char *str_given) {
	if (debug) {
		print_time();
		printf("DEBUG MSG.  =>  %s\n", str_given);
	}
}

void exit_program(int EXIT_STATUS) {
	printd("El programa s'ha aturat");
	if (show_exit_status) {
		print_time();
		printf("DEBUG MSG.  =>  El codi d'acabament ha estat: %i\n", EXIT_STATUS);
	}
    exit(EXIT_STATUS);
}

void print_bar(){
	printf("───────────────────────────────────────────────────────────────────────────\n");
}