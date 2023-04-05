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

// #define PORT 12345
#define BUFFER_SIZE 1024
#define T 1
#define P 2
#define Q 3
#define U 2
#define N 6
#define O 2

// DEFINIM ELS POSSIBLES ESTATS
#define DISCONNECTED 0
#define WAIT_REG_RESPONSE 1
#define REGISTERED 2
#define SEND_ALIVE 3

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

// char* ip_client = "127.0.0.1" // ???

int socketfd;
struct sockaddr_in server_addr;

char *strdup(const char *); // Inicialitzem strdup per a poder usarla

// DEFINIM LES VARIABLES AUXILIARS
void *wait_quit(void *arg);
void obrir_socket();
void read_config_file(const char *filename);
char *get_pdu_type(int type);
void read_server_config();
void print_server_info();

// Funcions decoratives TODO
void println(char *str);
void print_time();
void print_msg(char *str, int current_state);
void print_bar();

void send_message(int socketfd, const char *message) {
	ssize_t sent = send(socketfd, message, strlen(message), 0);
	if (sent < 0){
		if (debug) {
			perror("Error sending message");
		//exit(EXIT_FAIL);
		}
	}
}

ssize_t receive_message(int socketfd, char *buffer, size_t size){
	ssize_t received = recv(socketfd, buffer, size - 1, 0);
	if (received < 0){
		if (debug) {
			perror("Error receiving message");
		//exit(EXIT_FAIL);
		}
	}
	else{
		print_msg("S'ha rebut el missatge amb éxit", REGISTERED); // TESTING
	}
	buffer[received] = '\0';
	return received;
}

int main(int argc, char *argv[]){
	char buffer[BUFFER_SIZE];
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
		printf("Config file: %s\n", config_file);
	}

	print_msg("Equip passa a l'estat", current_state);

	//sleep(1);
	read_server_config();
	// printf("Estat 0x09: %s\n", get_pdu_type(9)); // Te retorna ERROR
	// Fer-ho servir per al debug

	obrir_socket();

	if (connect(socketfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0){
		if (debug) {
			perror("Error connecting to server");
		}
		close(socketfd);
		//exit(EXIT_FAIL);
	}

	if (debug){
		printf("Connected to server.\n");
	}

	// Comunicació amb el servidor basada en les especificacions.
	send_message(socketfd, "START");
	receive_message(socketfd, buffer, BUFFER_SIZE);
	if (debug) {
		printf("Server response: %s\n", buffer);
	}

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


void obrir_socket(){
	// Obrim el socket
	socketfd = socket(AF_INET, SOCK_STREAM, 0); // SOCK_DGRAM (UDP) || SOCK_STREAM (TCP)

	if (socketfd < 0){
		perror("Ha sorgit un error al crear el socket");
		exit(EXIT_FAIL);
	}

	// Binding del el servidor
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(NMS_UDP_Port); // Maybe TCP
	server_addr.sin_addr.s_addr = inet_addr(NMS_Id);
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

char *get_pdu_type(int type) {
    switch (type) {
        case 0x00: // Petició de registre
            return "REGISTER_REQ";
        case 0x01: // Acceptació de registre
            return "REGISTER_ACK";
        case 0x02: // Denegació de registre
            return "REGISTER_NACK";
        case 0x03: // Rebuig de registre
            return "REGISTER_REJ";
        case 0x10: // Enviament d’informació d’alive
            return "ALIVE_INF";
        case 0x11: // Confirmació de recepció d’informació d’alive
            return "ALIVE_ACK";
        case 0x12: // Denegació de recepció d’informació d’alive
            return "ALIVE_NACK";
        case 0x13: // Rebuig de recepció d’informació d’alive
            return "ALIVE_REJ";
        case 0x20: // Petició d’enviament d’arxiu de configuració
            return "SEND_FILE";
        case 0x21: // Acceptació de la petició d’enviament d’arxiu de configuració
            return "SEND_ACK";
        case 0x22: // Denegació de la petició d’enviament d’arxiu de configuració
            return "SEND_NACK";
        case 0x23: // Rebuig de la petició d’enviament d’arxiu de configuració
            return "SEND_REJ";
        case 0x24: // Bloc de dades de l’arxiu de configuració
            return "SEND_DATA";
        case 0x25: // Fi de l’enviament de dades de l’arxiu de configuració
            return "SEND_END";
        case 0x30: // Petició d’obtenció d’arxiu de configuració
            return "GET_FILE";
        case 0x31: // Acceptació d’obtenció d’arxiu de configuració
            return "GET_ACK";
        case 0x32: // Denegació d’obtenció d’arxiu de configuració
            return "GET_NACK";
        case 0x33: // Rebuig d’obtenció d’arxiu de configuració
            return "GET_REJ";
        case 0x34: // Bloc de dades de l’arxiu de configuració
            return "GET_DATA";
        case 0x35: // Fi de l’obtenció de l’arxiu de configuració
            return "GET_END";
        default:
            return "ERROR"; // Error de protocol (0x09)
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

void print_msg(char *str_given, int current_state){
	char current_state_str[strlen("WAIT_REG_RESPONSE") + 1];

	// Creem un diccionari per a cada estat
	switch (current_state){
	case WAIT_REG_RESPONSE:
		strcpy(current_state_str, "WAIT_REG_RESPONSE");
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