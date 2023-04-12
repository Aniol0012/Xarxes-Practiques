#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <signal.h>
#include <sys/select.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <time.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <pthread.h>


// DEFINIM ELS POSSIBLES ESTATS DEL EQUIP
#define DISCONNECTED 0xA0 // Equip desconnectat
#define WAIT_REG_RESPONSE 0xA2 // Espera de resposta a la petició de registre
#define WAIT_DB_CHECK 0xA4 // Espera de consulta BB. DD. d’equips autoritzats
#define REGISTERED 0xA6 // Equip registrat, sense intercanvi SEND_ALIVE
#define SEND_ALIVE 0xA8 // Equip enviant i rebent paquets de SEND_ALIVE

// TIPUS DE PAQUET FASE DE REGISTRE
#define REGISTER_REQ 0x00  // Petició de resgistre
#define REGISTER_ACK 0x02  // Acceptació de registre
#define REGISTER_NACK 0x04 // Denegació de registre
#define REGISTER_REJ 0x06  // Rebuig de registre
#define ERROR 0x0F         // Error de protocol

// TIPUS DE PAQUET MANTENIMENT DE COMUNICACIÓ
#define ALIVE_INF 0x10
#define ALIVE_ACK 0x12
#define ALIVE_NACK 0x14
#define ALIVE_REJ 0x16

#define SEND_FILE 0x20
#define SEND_ACK 0x21
#define SEND_NACK 0x22
#define SEND_REJ 0x23
#define SEND_DATA 0x24
#define SEND_END 0x25

#define GET_FILE 0x30
#define GET_ACK 0x31
#define GET_NACK 0x32
#define GET_REJ 0x33
#define GET_DATA 0x34
#define GET_END 0x35

// TEMPORITZADORS (EN SEGONS)
#define T 1 // Temps màxim de resposta
#define P 2 // Número de paquets
#define Q 3
#define U 2
#define N 6
#define O 2   // Processos de registre
#define R 2   // Envia ALIVE_INF cada R segons
#define U_2 3 // Número máxim de paquets ALIVE_INF sense rebre ALIVE_ACK

// DEFINIM VARIABLES DE SORTIDA
#define EXIT_SUCCESS 0
#define EXIT_FAIL -1

/*Estructura per a guardar les dades del arxiu del client */
struct client_config{
  char name[7];
  char MAC[13];
  char server[20];
  char random[7];
  int UDP_port;
  int TCP_port;
};

/*Estructura per a guardar dades per a comprovar als SEND_ALIVE */
struct server_data{
  char name[7];
  char MAC[13];
  char random[7];
};

/* Estructura per a pasar les structs de config a treatPacket() */
struct parameters{
  struct client_config *config;
  struct sockaddr_in addr_client;
  struct sockaddr_in udp_addr_server;
  struct sockaddr_in tcp_addr_server;
  struct udp_PDU *data;
};

/*Estructura que fa de paquet UDP */
struct udp_PDU{
  unsigned char type;
  char name[7];
  char mac[13];
  char random[7];
  char data [50];
};

// CONFIGURACIÓ ADICIONAL (S'HA D'ACTIVAR MANUALMENT)
bool show_local_time = true; // Mostra l'hora actual en els missatges | DEFAULT = TRUE
bool show_client_info = false; // Mostra la informació rebuda per l'arxiu de configuració | DEFAULT = FALSE
bool print_buffer = false; // Mostra la informació rebuda pel buffer | DEFAULT = FALSE
bool show_exit_status = false; // Mostre en mode debug el codi de retorn del programa | DEFAULT = FALSE
bool debug_activated = false; // Estat inicial del mode debug (s'activa amb el paràmetre -d) | DEFAULT = FALSE

#define MAX_STATUS_LENGTH 18 // strlen('WAIT_REG_RESPONSE') = 17 + '\0'
#define MAX_FILENAME_LENGTH 12 // strlen('client2.cfg') = 11 + '\0'

// DEFINIM VARIABLES GLOBALS
int udp_socket;
char software_config_file[MAX_FILENAME_LENGTH] = "client.cfg";
char network_config_file[MAX_FILENAME_LENGTH] = "boot.cfg";
char current_state[MAX_STATUS_LENGTH] = "DISCONNECTED";
int tcp_sock = 0, counter;
int pthread_created = 0;
struct parameters parameters;
struct server_data server_data;
struct client_config config;
struct udp_PDU create_packet(char type[], char mac[], char random_num[], char data[]);

pthread_t alive_thread;

// DECLARACIÓ DE FUNCIONS
int open_socket(int protocol);
void read_software_config_file(struct client_config *config);
void send_register_request(struct client_config *config, struct sockaddr_in udp_addr_server, struct sockaddr_in addr_client);
void create_UDP(struct udp_PDU *pdu, struct client_config *config, unsigned char petition);
void set_current_state(char _current_state[]);
void set_periodic_comunication();
int treat_UDP_packet();
void send_alive();
void get_network_file_size(char *size);
void *wait_quit(void *arg);
bool is_state_equal(char *str); // Comparem el nou estat amb l'actual

// FUNCIONS PER A PRINTAR
void println(char *str);
void exit_program(int EXIT_STATUS);
void printd(char *str_given); // Printar debugs
void print_bar();
void print_state(int current_state);
void print_time(); // Printar l'hora actual
void print_info();
void get_time(char *time_str);
void print_n();

// CODI PRINCIPAL
int main(int argc, char *argv[])
{
    struct sockaddr_in udp_addr_server, addr_client;

    pthread_t wait_quit_thread; // Creem el fil per a l'espera de la comanda
	pthread_create(&wait_quit_thread, NULL, wait_quit, NULL); // Iniciem el fil concurrent

    for (int i = 1; i < argc; i++){
		if (strcmp(argv[i], "-d") == 0){
			debug_activated = true;
            print_bar();
		    printf("\t\t\tMode debug activat\n");
		    print_bar();
		}
		else if (strcmp(argv[i], "-c") == 0 && i + 1 < argc){ 
			i++;
			strcpy(software_config_file, argv[i]);
		}
		else if (strcmp(argv[i], "-f") == 0 && i + 1 < argc){
			i++;
			strcpy(network_config_file, argv[i]);
		}
		else{
			fprintf(stderr, "Ús: %s [-d] [-c <config_file.cfg>]\n", argv[0]);
			exit_program(EXIT_SUCCESS); // És una sortida controlada del programa
		}
	}

    print_state(DISCONNECTED); // Partim de l'estat disconnected

    read_software_config_file(&config);
    strcpy(config.random, "000000");
    config.random[6] = '\0';

    printd("S'ha llegit l'arxiu de configuració");

    //printf("La configuració llegida és la següent: \n \t Name: %s \n \t MAC: %s \n \t Server: %s \n \t Port: %i \n",
           //config.name, config.MAC, config.server, config.UDP_port);
    print_info();

    /* Adreça del bind del client */
    memset(&addr_client, 0, sizeof(struct sockaddr_in));
    addr_client.sin_family = AF_INET;
    addr_client.sin_addr.s_addr = htonl(INADDR_ANY);
    addr_client.sin_port = htons(config.UDP_port);

    /* Adreça del servidor */
    memset(&udp_addr_server, 0, sizeof(udp_addr_server));
    udp_addr_server.sin_family = AF_INET;
    udp_addr_server.sin_addr.s_addr = inet_addr(config.server);
    udp_addr_server.sin_port = htons(config.UDP_port);

    /* Per a poder tractar els paquets més facilment més endavant */
    parameters.config = &config;
    parameters.addr_client = addr_client;
    parameters.udp_addr_server = udp_addr_server;

    // Obrim un socket UDP
	udp_socket = open_socket(IPPROTO_UDP); // Especifiquem que volem crear el socket en UDP

    send_register_request(&config, addr_client, udp_addr_server);
    pthread_join(wait_quit_thread, NULL); // Esperem que acabi el fil
    return 1;
}

void read_software_config_file(struct client_config *config)
{
    FILE *conf;
    char word[256];
    conf = fopen(software_config_file, "r");
    if (conf == NULL)
    {
        fprintf(stderr, "Error obrir arxiu");
        exit_program(EXIT_FAIL);
    }

    fscanf(conf, "%s", word);
    fscanf(conf, "%s", word);   /* No es la millor manera de fer-ho... pero ja que suposem que el fitxer es correcte*/
    strcpy(config->name, word); /*  Ens saltem les comprovacions */

    fscanf(conf, "%s", word);
    fscanf(conf, "%s", word);
    strcpy(config->MAC, word);

    fscanf(conf, "%s", word);
    fscanf(conf, "%s", word);
    // Tractar en una funció
    if (strcmp(word, "localhost") == 0)
    {
        strcpy(config->server, "127.0.0.1");
    }
    else
    {
        strcpy(config->server, word);
    }

    fscanf(conf, "%s", word);
    fscanf(conf, "%s", word);
    config->UDP_port = atoi(word);
    fclose(conf);
}

void send_register_request(struct client_config *config, struct sockaddr_in udp_addr_server, struct sockaddr_in addr_client) {
    int tries, max_tries = 2, n_bytes;
    int correct = 0; /*variable per saber si s'ha aconseguit correctament el registre */
    struct udp_PDU data;
    struct udp_PDU reg_pdu;
    socklen_t fromlen;
    char buff[300];
    fromlen = sizeof(udp_addr_server);

    /* Creació paquet registre */
    create_UDP(&reg_pdu, config, REGISTER_REQ);

    /* Inici proces subscripció */
    for (tries = 0; tries < max_tries && strcmp("REGISTERED", current_state) != 0 && strcmp("SEND_ALIVE", current_state); tries++) {
        int packet_counter = 0, interval = T, t = T;
        int p = P, q = Q, n = N, u = U;
        
        while (packet_counter < n && strcmp("REGISTERED", current_state) != 0 && strcmp("SEND_ALIVE", current_state)) {
            sendto(udp_socket, &reg_pdu, sizeof(reg_pdu), 0, (struct sockaddr *)&udp_addr_server, sizeof(udp_addr_server));
            packet_counter++;
            printd("Enviat paquet REGISTER_REQ");

            if (strcmp(current_state, "DISCONNECTED") == 0) {
                set_current_state("WAIT_REG_RESPONSE");
                print_state(WAIT_REG_RESPONSE);
                
                printd("Passat a l'estat WAIT_REG_RESPONSE");
            }

            n_bytes = recvfrom(udp_socket, &data, sizeof(data), MSG_DONTWAIT, (struct sockaddr *)&udp_addr_server, &fromlen);
            if (n_bytes > 0) {
                correct = 1;
                break;
            }

            if (packet_counter <= p) {
                t = interval;
            } else if (packet_counter > p && t < (q * T)) {
                t += interval;
            } else {
                t = q * T;
            }

            sleep(t);
        }

        if (correct == 1)
            break;

        if (tries < max_tries - 1) {
            printd("Reiniciant procès subscripció");
            sleep(u);
        }
    } /* Fi while d'enviar paquets */

    if (tries == max_tries && correct == 0) { /* Comprova si s'ha sortit del bucle per màxim d'intents */
        println("Ha fallat el procès de registre. No s'ha pogut contactar amb el servidor.");
        exit_program(EXIT_FAIL);
    }

    if (show_client_info && debug_activated) {
        sprintf(buff, "Rebut: bytes= %lu, type:%i, mac=%s, random=%s, dades=%s", sizeof(struct udp_PDU), data.type, data.mac, data.random, data.data);
        //print_info(data.type, data.name, data.mac, data.random, data.data);
        if (print_buffer) {
            printd(buff);
        }
    }
    parameters.data = &data;
    treat_UDP_packet();
}


void send_alive()
{
    int u = 3, i = 0, r = 3, temp, n_bytes, packet_current_state = 0, incorrecte = 0;
    char buff[300];
    socklen_t fromlen;
    struct udp_PDU alive_pdu;
    struct udp_PDU data;
    fromlen = sizeof(&parameters.udp_addr_server);

    create_UDP(&alive_pdu, parameters.config, ALIVE_INF);
    while (1)
    {
        while (is_state_equal("SEND_ALIVE"))
        {
            temp = sendto(udp_socket, &alive_pdu, sizeof(alive_pdu), 0, (struct sockaddr *)&parameters.udp_addr_server, sizeof(parameters.udp_addr_server));
            printd("Enviat paquet ALIVE_INF");
            if (temp == -1)
            {
                printf("Error sendTo \n");
            }

            n_bytes = recvfrom(udp_socket, &data, sizeof(data), MSG_DONTWAIT, (struct sockaddr *)&parameters.udp_addr_server, &fromlen);
            if (n_bytes > 0)
            {
                i = 0;
                parameters.data = &data;
                if (show_client_info && debug_activated) {
                    sprintf(buff, "Rebut: bytes= %lu, type:%i, nom=%s, mac=%s, random=%s, dades=%s", sizeof(struct udp_PDU), data.type, data.name, data.mac, data.random, data.data);
                    if (print_buffer) {
                        printd(buff);
                    }
                }
                packet_current_state = treat_UDP_packet();
                if (packet_current_state == -1)
                { /*Significa que es un paquet incorrecte */
                    incorrecte += 1;
                    if (incorrecte == u)
                    {
                        set_current_state("DISCONNECTED");
                        println("ESTAT: DISCONNECTED");
                        printd("No s'ha rebut tres paquets de confirmació d'SEND_ALIVE consecutius correctes.");
                        printd("Client passa a l'estat DISCONNECTED i reinicia el proces de subscripció");
                        break;
                    }
                }
                else
                {
                    incorrecte = 0;
                }
            }
            else
            {
                i++;
                if (i == u)
                { /*Comprova que no s'ha rebut 3 paquets seguits de confirmació d'ALive*/
                    set_current_state("DISCONNECTED");
                    println("ESTAT: DISCONNECTED");
                    printd("No s'ha rebut confirmació de tres paquets de rebuda de paquets SEND_ALIVE consecutius.");
                    printd("Client passa a l'estat DISCONNECTED i reinicia el proces de subscripció");
                    break;
                }
            }
            n_bytes = 0;
            sleep(r);
        }
    }
    send_register_request(parameters.config, parameters.udp_addr_server, parameters.addr_client);
}

void set_periodic_comunication()
{
    int r = 3, u = 0, temp, n_bytes;
    char buff[300];
    socklen_t fromlen;
    struct udp_PDU alive_pdu;
    struct udp_PDU data;
    create_UDP(&alive_pdu, parameters.config, ALIVE_INF);
    fromlen = sizeof(&parameters.udp_addr_server);

    while (1 && u != 3)
    {
        temp = sendto(udp_socket, &alive_pdu, sizeof(alive_pdu), 0, (struct sockaddr *)&parameters.udp_addr_server, sizeof(parameters.udp_addr_server));
        printd("Enviat paquet ALIVE_INF");
        if (temp == -1)
        {
            printf("Error sendTo \n");
        }
        sleep(r);
        n_bytes = recvfrom(udp_socket, &data, sizeof(data), MSG_DONTWAIT, (struct sockaddr *)&parameters.udp_addr_server, &fromlen);
        if (n_bytes > 0)
        {
            parameters.data = &data;
            u = 0;
            printd(buff);
            memset(buff, '\0', sizeof(buff));
            treat_UDP_packet();
            if (strcmp(current_state, "SEND_ALIVE") == 0 || u == 3)
            {
                break;
            }
        }
        else
        {
            u++;
        }
    }

    if (strcmp(current_state, "REGISTERED") == 0)
    {
        set_current_state("DISCONNECTED");
        print_state(DISCONNECTED);
        printd("NO s'ha rebut resposta");
        printd("Passat a l'estat DISCONNECTED i reinici del procès de subscripció");
        send_register_request(parameters.config, parameters.udp_addr_server, parameters.addr_client);
    }
    else if (strcmp(current_state, "SEND_ALIVE") == 0 && pthread_created == 0) // Aixo revisar ja que no fa falta
    {
        pthread_created = 1;
        printd("Creat procés per mantenir comunicació periodica amb el servidor");
        pthread_create(&alive_thread, NULL, (void *(*)(void *))send_alive, NULL);
    }
}

void set_current_state(char _current_state[])
{
    strcpy(current_state, _current_state);
}

int treat_UDP_packet()
{
    char buff[300];
    int correct = 0;
    switch (parameters.data->type)
    {
    case REGISTER_REJ:
        sprintf(buff, "S'ha rebutjat el client, motiu: %s", parameters.data->data);
        println(buff);
        set_current_state("DISCONNECTED");
        print_state(DISCONNECTED);
        exit_program(EXIT_FAIL);
    case REGISTER_NACK:
        if (strcmp("SEND_ALIVE", current_state) == 0)
        { // El client ja està registrat
            break;
        }
        if (counter < 3)
        {
            printd("Rebut REGISTER_NACK, reiniciant el procès de registre");
            send_register_request(parameters.config, parameters.udp_addr_server, parameters.addr_client);
            counter++;
            return 0;
        }
        printd("S'ha superat el número màxim d'intents");
        exit_program(EXIT_FAIL);
    case REGISTER_ACK:
        if (!is_state_equal("REGISTERED"))
        {
            printd("S'ha rebut un REGISTER_ACK");
            set_current_state("REGISTERED");
            print_state(REGISTERED);
            parameters.config->TCP_port = atoi(parameters.data->data);
            strcpy(parameters.config->random, parameters.data->random);
            strcpy(server_data.random, parameters.data->random);
            strcpy(server_data.name, parameters.data->name);
            strcpy(server_data.MAC, parameters.data->mac);
            set_periodic_comunication();
        }
        else
        {
            printd("S'ha rebut un REGISTER_ACK");
        }
        return 0;
    case ALIVE_ACK:
        if (strcmp(parameters.data->random, server_data.random) == 0 && strcmp(parameters.data->name, server_data.name) == 0 && strcmp(parameters.data->mac, server_data.MAC) == 0)
        {
            correct = 1;
        }
        if (!is_state_equal("SEND_ALIVE") && correct == 1)
        { /* Primer ack rebut */
            set_current_state("SEND_ALIVE");
            print_state(SEND_ALIVE);
            printd("Rebut ALIVE_ACK correcte, client passa a l'estat SEND_ALIVE");
        }
        else if (is_state_equal("SEND_ALIVE") && correct == 1)
        { /* Ja tenim estat SEND_ALIVE*/
            printd("Rebut ALIVE_ACK");
        }
        else
        {
            printd("Rebut ALIVE_ACK incorrecte");
            return -1;
        }
        return 0;
    case ALIVE_NACK: /*No els tenim en compte, no caldria ficar-los */
        return 0;
    case ALIVE_REJ:
        if (is_state_equal("SEND_ALIVE"))
        {
            set_current_state("DISCONNECTED");
            printd("S'ha rebut un ALIVE_REJ");
            print_state(DISCONNECTED);
            printd("Reinici del procés de registre");
            send_register_request(parameters.config, parameters.udp_addr_server, parameters.addr_client);
        }
        return 0;
    }
    return 0;
}

void create_UDP(struct udp_PDU *pdu, struct client_config *configuracio, unsigned char peticio)
{
    pdu->type = peticio;
    strcpy(pdu->name, configuracio->name);
    strcpy(pdu->mac, configuracio->MAC);
    strcpy(pdu->random, configuracio->random);
    memset(pdu->data, '\0', sizeof(char) * 49);

    if (peticio == REGISTER_REQ)
    {
        pdu->data[49] = '\0';
    }
}

void get_network_file_size(char *str_size)
{
    FILE *f;
    int size;

    f = fopen(network_config_file, "r");
    if (f == NULL)
    {
        fprintf(stderr, "Error obrir arxiu");
        exit_program(EXIT_FAIL);
    }
    fseek(f, 0L, SEEK_END);
    size = ftell(f);
    sprintf(str_size, "%d", size);
    fclose(f);
}

void print_info() {
    if (show_client_info && debug_activated) {
        printd("La informació obtinguda de l'arxiu de configuració ha estat:");
        printf("\t\t\t\tId equip: %s\n", config.name);
        printf("\t\t\t\tAdreça MAC: %s\n", config.MAC);
        printf("\t\t\t\tNMS-Id: %s\n", config.server);
        printf("\t\t\t\tNMS-UDP-Port: %d\n", config.UDP_port);
    }
}

int open_socket(int protocol) {

    int socket_type;

    if (protocol == IPPROTO_UDP) {
        socket_type = SOCK_DGRAM; // Protocol UDP
    } else {
        socket_type = SOCK_STREAM; // Protocol TCP
    }

    int socket_created = socket(AF_INET, socket_type, protocol);

    if (socket_created < 0) {
		printd("Ha sorgit un error al crear el socket");
        exit_program(EXIT_FAIL);
    }
    return socket_created;
}

void *wait_quit(void *arg) {
	char input[MAX_FILENAME_LENGTH];
	while (true) {
        fgets(input, MAX_FILENAME_LENGTH, stdin);
        input[strcspn(input, "\n")] = 0; // Eliminem el salt de linea
        if (strcmp(input, "quit") == 0) {
            print_n();
			exit_program(EXIT_SUCCESS);
        } else {
            println("Comanda incorrecta");
        }
    }
    return NULL;
}

bool is_state_equal(char *str) {
    return strcmp(current_state, str) == 0;
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
	printf("MSG.  =>  %s\n", str);
}

void printd(char *str_given) {
	if (debug_activated) {
		print_time();
		printf("DEBUG MSG.  =>  %s\n", str_given);
	}
}

void print_state(int current_state) {
	char current_state_str[MAX_STATUS_LENGTH];

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
	printf("MSG.  =>  Equip passa a l'estat: %s\n", current_state_str);
}

void exit_program(int EXIT_STATUS) {
	printd("El programa s'ha aturat");
	if (show_exit_status) {
		print_time();
		printf("DEBUG MSG.  =>  El codi d'acabament ha estat: %i\n", EXIT_STATUS);
	}
    exit(EXIT_STATUS);
}

void print_n() {
	printf("\n");
}

void print_bar(){
	printf("───────────────────────────────────────────────────────────────────────────\n");
}
