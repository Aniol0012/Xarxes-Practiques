#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <stdbool.h>
#include <signal.h> //Not used
#include <pthread.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define BUFFER_SIZE 1024 // Tocar aixo
#define T 1
#define P 2
#define Q 3
#define U 2
#define N 6
#define O 2

// DEFINIM ELS POSSIBLES ESTATS DEL EQUIP
#define DISCONNECTED 0xA0
#define WAIT_REG_RESPONSE 0xA2
#define WAIT_DB_CHECK 0xA4
#define REGISTERED 0xA6
#define SEND_ALIVE 0xA8

// DEFINIM VARIABLES PER A STRINGS
#define MAX_INPUT 20

int current_state = DISCONNECTED;
bool show_local_time = true;
bool debug = false;

// DEFINIM VARIABLES DE SORTIDA
#define EXIT_SUCCESS 0
#define EXIT_FAIL -1

// CONFIGURACIÓ DEL SERVIDOR
char *NMS_Id = NULL;
char *NMS_MAC = NULL;
int NMS_UDP_Port = 0;
int NMS_TCP_Port = 0;

char* client_ip = "127.0.0.1";

int socketfd;
struct sockaddr_in server_addr;
char buffer[BUFFER_SIZE];

char *strdup(const char *); // Inicialitzem strdup per a poder usarla
void change_state(int new_state);
void send_register_request();

// DEFINIM LES VARIABLES AUXILIARS
void *wait_quit(void *arg);
void read_config_file(const char *filename);
char *get_pdu_type(int type);
void read_server_config();
void print_server_info();

// Funcions decoratives TODO
void println(char *str);
void print_time();
void print_state(char *str, int current_state);
void print_bar();

/*
void send_message(int socketfd, const char *message) {
	ssize_t sent = send(socketfd, message, strlen(message), 0);
	if (sent < 0){
		if (debug) {
			perror("Error sending message");
		//exit(EXIT_FAIL);
		}
	}
}
*/

ssize_t receive_message(int socketfd, char *buffer, size_t size) {
	ssize_t received = recv(socketfd, buffer, size - 1, 0);
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

void send_register_request() {
	// Obrim un socket UDP
	socketfd = socket(AF_INET, SOCK_DGRAM, 0); // SOCK_DGRAM (UDP) || SOCK_STREAM (TCP)

	if (socketfd < 0){
		if (debug) {
			println("Ha sorgit un error al crear el socket");
		}
		exit(EXIT_FAIL);
	}

	// Fer el bind del servidor per a configurar el port IDK LA VD
	struct sockaddr_in local_addr;
    memset(&local_addr, 0, sizeof(local_addr));
	// Crec que el bind s'ha de fer amb el del client MODIFICAR!!!!!!!!!!!!!!!!!
    local_addr.sin_family = AF_INET;
    local_addr.sin_port = htons(0); // 0 permite que el sistema elija un puerto disponible
    //local_addr.sin_addr.s_addr = inet_addr(client_ip); // htonl(INADDR_ANY);
    local_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(socketfd, (struct sockaddr *)&local_addr, sizeof(local_addr)) < 0) {        
		if (debug) {
            perror("Error en bind");
        }
        exit(EXIT_FAIL);
    }

    char register_request[64];
    sprintf(register_request, "0x00 %s %s", NMS_Id, NMS_MAC);

	printf("%i\n", NMS_UDP_Port);
	printf("%s\n",NMS_Id);

    struct sockaddr_in server_addr_udp;
	memset(&server_addr_udp, 0, sizeof(server_addr_udp));// Ptsr no fa falta
    server_addr_udp.sin_family = AF_INET;
    server_addr_udp.sin_port = htons(NMS_UDP_Port);
    server_addr_udp.sin_addr.s_addr = inet_addr(NMS_Id);

    ssize_t sent = sendto(socketfd, register_request, strlen(register_request), 0, (struct sockaddr *) &server_addr_udp, sizeof(server_addr_udp));
	
	//sleep(5);
	printf("\nCodi de retorn del sendto: %li\n", sent); // Esta retornant -1
    
	if (sent < 0) {
        if (debug) {
        	println("Error a l'enviar la petició de registre"); // Esta fallant aqui
        }
		exit(EXIT_FAIL);
    }
    change_state(WAIT_REG_RESPONSE);

	// GESTIÓ DEL ACK O NACK
	
	if (strncmp(buffer, "0x02", 4) == 0) { // REGISTER_ACK
		change_state(REGISTERED);
	} else if (strncmp(buffer, "0x04", 4) == 0 || strncmp(buffer, "0x06", 4) == 0) { // REGISTER_NACK || REGISTER_REJ
		change_state(DISCONNECTED);
	} else {
		change_state(DISCONNECTED); // Millorar
		// Gestiona altres casos o errors
	}
	
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
			fprintf(stderr, "Ús: %s [-d] [-c <config_file>]\n", argv[0]);
			exit(EXIT_FAIL);
		}
	}

	if (debug){
		print_bar();
		printf("\t\t\tMode debug activat\n");
		print_bar();
	}

	if (config_file){ // Printem el fitxer de configuració proporcionat en el paràmetre -c
		if (debug) {
			printf("Arxiu de configuració (-c): %s\n", config_file);
		}
	}

	// Estat inicial
	print_state("Equip passa a l'estat", current_state);

	//sleep(1);
	read_server_config();
	// printf("Estat 0x09: %s\n", get_pdu_type(9)); // Te retorna ERROR
	// Fer-ho servir per al debug

	send_register_request();
    ssize_t received = recv(socketfd, buffer, BUFFER_SIZE - 1, 0);
    if (received < 0) {
        if (debug) {
            perror("Error rebent la resposta de registre");
        }
    } else {
        buffer[received] = '\0';
        if (debug) {
            printf("Resposta del servidor: %s\n", buffer);
        }
    }

	// while(sleep(WAIT_rEG_RESPONSE_SEGONS)) // S'hauria d'afegir per anar intentant-ho

	// Connexio TCP
	/*
	if (connect(socketfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0){
		change_state(WAIT_REG_RESPONSE);
		if (debug) {
			perror("No s'ha pogut conectar amb el servidor");
		}
		change_state(DISCONNECTED);
		close(socketfd);
		//exit(EXIT_FAIL);
	} else {
		change_state(REGISTERED);
	}
	*/

	// Comunicació amb el servidor basada en les especificacions.
	/*
	send_message(socketfd, "START");
	receive_message(socketfd, buffer, BUFFER_SIZE);
	if (debug) {
		printf("Server response: %s\n", buffer);
	}
	*/

	// Afegim la petició de registre
	/*
    char register_request[64];
    sprintf(register_request, "0x00 %s %s", NMS_Id, NMS_MAC);
    send_message(socketfd, register_request);
    receive_message(socketfd, buffer, BUFFER_SIZE);
    if (debug) {
        printf("Server response: %s\n", buffer);
    }
	*/

	// Aquí es podrien afegir més interaccions amb el servidor si fos necessari.

	close(socketfd);
	
	/* S'hauria d'usar quan ja no se vulguin més
	free(NMS_Id);
    free(NMS_MAC);
	*/
	pthread_join(wait_quit_thread, NULL); // Esperem que acabi el fil
	return 0;
}

void *wait_quit(void *arg) {
	char input[MAX_INPUT];
	while (1) {
        fgets(input, MAX_INPUT, stdin);
        input[strcspn(input, "\n")] = 0; // Eliminem el salt de linea
        if (strcmp(input, "quit") == 0) {
            if (debug) {
				printf("El programa s'ha aturat.\n");
			}
    		exit(EXIT_SUCCESS);
        }
    }
    return NULL;
}

void read_server_config() {
    FILE *file = fopen("server.cfg", "r");
    if (file == NULL) {
        perror("Error a l'obrir l'arxiu server.cfg");
        exit(EXIT_FAIL);
    }

    char line[64];
    while (fgets(line, sizeof(line), file) != NULL) {
        if (strncmp(line, "Id", 2) == 0) {
            NMS_Id = malloc(16 * sizeof(char));
            sscanf(line, "Id %15s", NMS_Id);
        } else if (strncmp(line, "MAC", 3) == 0) {
            NMS_MAC = malloc(13 * sizeof(char));
            sscanf(line, "MAC %12s", NMS_MAC);
        } else if (strncmp(line, "UDP-port", 8) == 0) {
            sscanf(line, "UDP-port %d", &NMS_UDP_Port);
        } else if (strncmp(line, "TCP-port", 8) == 0) {
            sscanf(line, "TCP-port %d", &NMS_TCP_Port);
        }
    }

    fclose(file);

	if (debug) {;
		print_server_info();
	}
}

void print_server_info() {
	println("La informació obtinguda de l'arxiu de configuració server.cfg ha estat:");
	print_time();
	printf("NMS_Id: %s\n", NMS_Id);
	print_time();
	printf("NMS_MAC: %s\n", NMS_MAC);
	print_time();
	printf("NMS_UDP_Port: %i\n", NMS_UDP_Port);
	print_time();
	printf("NMS_TCP_Port: %i\n", NMS_TCP_Port);
}

void read_config_file(const char *filename){
	FILE *file = fopen(filename, "r");
	if (!file){
		perror("Error al obrir l'axiu de configuració");
		exit(EXIT_FAIL);
	}

	char line[256];
	while (fgets(line, sizeof(line), file)){
		char key[64], value[64];
		if (sscanf(line, "%63s = %63s", key, value) == 2){
			if (strcmp(key, "NMS_Id") == 0){
				NMS_Id = strdup(value); // Maybe un atoi()
			} else if (strcmp(key, "NMS_UDP_Port") == 0){
				NMS_UDP_Port = atoi(strdup(value));
			}
		}
	}

	fclose(file);
}

void change_state(int new_state) {
    if (current_state != new_state) {
        current_state = new_state;
        print_state("Equip passa a l'estat", current_state);
    }
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

void println(char *str) {
	print_time();
	printf("%s\n", str);
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

	if (show_local_time){
		char time_str[10];
		get_time(time_str);
		printf("%s MSG.  =>  %s: %s\n", time_str, str_given, current_state_str);
	}
	else{
		printf("MSG.  =>  %s: %s\n", str_given, current_state_str);
	}
}

void print_bar(){
	printf("───────────────────────────────────────────────────────────────────────────\n");
}