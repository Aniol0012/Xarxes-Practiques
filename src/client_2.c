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

/*Estructura per a guardar les dades del arxiu del client */
struct client_config{
  char name[7];
  char MAC[13];
  char server[20];
  char random[7];
  int UDPport;
  int TCPport;
};

/*Estructura per a guardar dades per a comprovar als ALIVE */
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

// CONFIGURACIÓ ADICIONAL (S'HA D'ACTIVAR MANUALMENT)
bool show_local_time = true;
bool show_client_info = false; // Mostra la informació rebuda per l'arxiu de configuració
bool show_exit_status = false;
bool debug_activated = false;

/* Variables globals */
int debug_flag = 0;
int udp_socket;
char software_config_file[20] = "client.cfg";
char network_config_file[20] = "boot.cfg"; // El -f crec q no sha de fer
char current_state[30] = "DISCONNECTED";
int tcp_sock = 0, counter;
int pthread_created = 0;
struct parameters params;
struct server_data server_data;
pthread_t alive_thread;

/* Funcions */
void parse_parameters(int argc, char **argv);
int open_socket(int protocol);
void read_software_config_file(struct client_config *config);
void debug(char msg[]);
struct udp_PDU create_packet(char type[], char mac[], char random_num[], char data[]);
void subscribe(struct client_config *config, struct sockaddr_in udp_addr_server, struct sockaddr_in addr_client);
void crea_UDP(struct udp_PDU *pdu, struct client_config *config, unsigned char petition);
void set_current_state(char _current_state[]);
void print_msg(char msg[]);
void set_periodic_comunication();
int treat_UDP_packet();
void send_alive();
void read_commands();
void treat_command(char command[]);
void send_conf();
void send_file();
void get_network_file_size(char *size);

void println(char *str);
void exit_program(int EXIT_STATUS);
void printd(char *str_given); // Printar debugs
void print_bar();
void print_time(); // Printar l'hora actual
void get_time(char *time_str);

int main(int argc, char **argv)
{
    struct sockaddr_in udp_addr_server, addr_client;
    struct client_config config;

    parse_parameters(argc, argv);
    read_software_config_file(&config);
    strcpy(config.random, "000000");
    config.random[6] = '\0';

    debug("S'ha llegit l'arxiu de configuració");

    printf("La configuració llegida és la següent: \n \t Name: %s \n \t MAC: %s \n \t Server: %s \n \t Port: %i \n",
           config.name, config.MAC, config.server, config.UDPport);

    /* Adreça del bind del client */
    memset(&addr_client, 0, sizeof(struct sockaddr_in));
    addr_client.sin_family = AF_INET;
    addr_client.sin_addr.s_addr = htonl(INADDR_ANY);
    addr_client.sin_port = htons(config.UDPport);

    /* Adreça del servidor */
    memset(&udp_addr_server, 0, sizeof(udp_addr_server));
    udp_addr_server.sin_family = AF_INET;
    udp_addr_server.sin_addr.s_addr = inet_addr(config.server);
    udp_addr_server.sin_port = htons(config.UDPport);

    /* Per a poder tractar els paquets més facilment més endavant */
    params.config = &config;
    params.addr_client = addr_client;
    params.udp_addr_server = udp_addr_server;

    // Obrim un socket UDP
	udp_socket = open_socket(IPPROTO_UDP); // Especifiquem que volem crear el socket en UDP

    subscribe(&config, addr_client, udp_addr_server);
    read_commands();
    pthread_join(alive_thread, NULL);
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
        exit(-1);
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
    config->UDPport = atoi(word);
    fclose(conf);
}

void subscribe(struct client_config *config, struct sockaddr_in udp_addr_server, struct sockaddr_in addr_client) {
    int tries, max_tries = 2, n_bytes;
    int correct = 0; /*variable per saber si s'ha aconseguit correctament el registre */
    struct udp_PDU data;
    struct udp_PDU reg_pdu;
    socklen_t fromlen;
    char buff[300];
    fromlen = sizeof(udp_addr_server);

    /* Creació paquet registre */
    crea_UDP(&reg_pdu, config, REGISTER_REQ);

    /* Inici proces subscripció */
    for (tries = 0; tries < max_tries && strcmp("REGISTERED", current_state) != 0 && strcmp("ALIVE", current_state); tries++) {
        int packet_counter = 0, interval = T, t = T;
        int p = P, q = Q, n = N, u = U;
        
        while (packet_counter < n && strcmp("REGISTERED", current_state) != 0 && strcmp("ALIVE", current_state)) {
            sendto(udp_socket, &reg_pdu, sizeof(reg_pdu), 0, (struct sockaddr *)&udp_addr_server, sizeof(udp_addr_server));
            packet_counter++;
            debug("Enviat paquet REGISTER_REQ");

            if (strcmp(current_state, "DISCONNECTED") == 0) {
                set_current_state("WAIT_REG_RESPONSE");
                if (debug_flag == 0)
                    print_msg("ESTAT: WAIT_REG_RESPONSE");
                debug("Passat a l'estat WAIT_REG_RESPONSE");
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
            debug("Reiniciant procès subscripció");
            sleep(u);
        }
    } /* Fi while d'enviar paquets */

    if (tries == max_tries && correct == 0) { /* Comprova si s'ha sortit del bucle per màxim d'intents */
        print_msg("Ha fallat el procès de registre. No s'ha pogut contactar amb el servidor.");
        exit(-1);
    }

    sprintf(buff, "Rebut: bytes= %lu, type:%i, mac=%s, random=%s, dades=%s", sizeof(struct udp_PDU), data.type, data.mac, data.random, data.data);
    debug(buff);
    params.data = &data;
    treat_UDP_packet();
}


void send_alive()
{
    int u = 3, i = 0, r = 3, temp, n_bytes, packet_current_state = 0, incorrecte = 0;
    char buff[300];
    socklen_t fromlen;
    struct udp_PDU alive_pdu;
    struct udp_PDU data;
    fromlen = sizeof(&params.udp_addr_server);

    crea_UDP(&alive_pdu, params.config, ALIVE_INF);
    while (1)
    {
        while (strcmp(current_state, "ALIVE") == 0)
        {
            temp = sendto(udp_socket, &alive_pdu, sizeof(alive_pdu), 0, (struct sockaddr *)&params.udp_addr_server, sizeof(params.udp_addr_server));
            debug("Enviat paquet ALIVE_INF");
            if (temp == -1)
            {
                printf("Error sendTo \n");
            }

            n_bytes = recvfrom(udp_socket, &data, sizeof(data), MSG_DONTWAIT, (struct sockaddr *)&params.udp_addr_server, &fromlen);
            if (n_bytes > 0)
            {
                i = 0;
                params.data = &data;
                sprintf(buff, "Rebut: bytes= %lu, type:%i, nom=%s, mac=%s, random=%s, dades=%s", sizeof(struct udp_PDU), data.type, data.name, data.mac, data.random, data.data);
                debug(buff);
                packet_current_state = treat_UDP_packet();
                if (packet_current_state == -1)
                { /*Significa que es un paquet incorrecte */
                    incorrecte += 1;
                    if (incorrecte == u)
                    {
                        set_current_state("DISCONNECTED");
                        print_msg("ESTAT: DISCONNECTED");
                        debug("No s'ha rebut tres paquets de confirmació d'ALIVE consecutius correctes.");
                        debug("Client passa a l'estat DISCONNECTED i reinicia el proces de subscripció");
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
                    print_msg("ESTAT: DISCONNECTED");
                    debug("No s'ha rebut confirmació de tres paquets de rebuda de paquets ALIVE consecutius.");
                    debug("Client passa a l'estat DISCONNECTED i reinicia el proces de subscripció");
                    break;
                }
            }
            n_bytes = 0;
            sleep(r);
        }
    }
    subscribe(params.config, params.udp_addr_server, params.addr_client);
}

void set_periodic_comunication()
{
    int r = 3, u = 0, temp, n_bytes;
    char buff[300];
    socklen_t fromlen;
    struct udp_PDU alive_pdu;
    struct udp_PDU data;
    crea_UDP(&alive_pdu, params.config, ALIVE_INF);
    fromlen = sizeof(&params.udp_addr_server);

    while (1 && u != 3)
    {
        temp = sendto(udp_socket, &alive_pdu, sizeof(alive_pdu), 0, (struct sockaddr *)&params.udp_addr_server, sizeof(params.udp_addr_server));
        debug("Enviat paquet ALIVE_INF");
        if (temp == -1)
        {
            printf("Error sendTo \n");
        }
        sleep(r);
        n_bytes = recvfrom(udp_socket, &data, sizeof(data), MSG_DONTWAIT, (struct sockaddr *)&params.udp_addr_server, &fromlen);
        if (n_bytes > 0)
        {
            params.data = &data;
            u = 0;
            sprintf(buff, "Rebut: bytes= %lu, type:%i, nom=%s, mac=%s, random=%s, dades=%s", sizeof(struct udp_PDU), data.type, data.name, data.mac, data.random, data.data);
            debug(buff);
            memset(buff, '\0', sizeof(buff)); /*Per evitar stack smashing */
            treat_UDP_packet();
            if (strcmp(current_state, "ALIVE") == 0 || u == 3)
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
        if (debug_flag == 0)
            print_msg("ESTAT: DISCONNECTED");
        debug("NO s'ha rebut resposta");
        debug("Passat a l'estat DISCONNECTED i reinici del procès de subscripció");
        subscribe(params.config, params.udp_addr_server, params.addr_client);
    }
    else if (strcmp(current_state, "ALIVE") == 0 && pthread_created == 0)
    {
        pthread_created = 1;
        debug("Creat procés per mantenir comunicació periodica amb el servidor");
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
    int equals;
    int correct = 0;
    switch (params.data->type)
    {
    case REGISTER_REJ:
        sprintf(buff, "El client ha estat rebutjat. Motiu: %s", params.data->data);
        print_msg(buff);
        set_current_state("DISCONNECTED");
        if (debug_flag == 0)
            print_msg("ESTAT: DISCONNECTED");
        debug("Client passa a l'estat : DISCONNECTED.");
        exit(-1);
        return 0;
    case REGISTER_NACK:
        if (strcmp("ALIVE", current_state) == 0)
        { /* El desestimem perque ja estem registrats */
            break;
        }
        if (counter < 3)
        {
            debug("Rebut REGISTER_NACK, reiniciant procès subscripció");
            subscribe(params.config, params.udp_addr_server, params.addr_client);
            counter++;
            return 0;
        }
        debug("Superat màxim d'intents. Tancant client.");
        exit(-1);
    case REGISTER_ACK:
        equals = strcmp(current_state, "REGISTERED");
        if (equals != 0)
        {
            debug("Rebut REGISTER_ACK, client passa a l'estat REGISTERED");
            set_current_state("REGISTERED");
            if (debug_flag == 0)
                print_msg("ESTAT: REGISTERED");
            params.config->TCPport = atoi(params.data->data);
            strcpy(params.config->random, params.data->random);
            strcpy(server_data.random, params.data->random);
            strcpy(server_data.name, params.data->name);
            strcpy(server_data.MAC, params.data->mac);
            set_periodic_comunication();
        }
        else
        {
            debug("Rebut REGISTER_ACK");
        }
        return 0;
    case ALIVE_ACK:
        equals = strcmp(current_state, "ALIVE");
        if (strcmp(params.data->random, server_data.random) == 0 && strcmp(params.data->name, server_data.name) == 0 && strcmp(params.data->mac, server_data.MAC) == 0)
        {
            correct = 1;
        }
        if (equals != 0 && correct == 1)
        { /* Primer ack rebut */
            set_current_state("ALIVE");
            if (debug_flag == 0)
                print_msg("ESTAT: ALIVE");
            debug("Rebut ALIVE_ACK correcte, client passa a l'estat ALIVE");
        }
        else if (equals == 0 && correct == 1)
        { /* Ja tenim estat ALIVE*/
            debug("Rebut ALIVE_ACK");
        }
        else
        {
            debug("Rebut ALIVE_ACK incorrecte");
            return -1;
        }
        return 0;
    case ALIVE_NACK: /*No els tenim en compte, no caldria ficar-los */
        return 0;
    case ALIVE_REJ:
        equals = strcmp(current_state, "ALIVE");
        if (equals == 0)
        {
            set_current_state("DISCONNECTED");
            if (debug_flag == 0)
                print_msg("ESTAT: DISCONNECTED");
            debug("Rebut ALIVE_REJ, possible suplantació d'identitat. Client pasa a estat DISCONNECTED");
            debug("Reiniciant proces subscripció");
            subscribe(params.config, params.udp_addr_server, params.addr_client);
        }
        return 0;
    }
    return 0;
}

void crea_UDP(struct udp_PDU *pdu, struct client_config *configuracio, unsigned char peticio)
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
        exit(-1);
    }
    fseek(f, 0L, SEEK_END);
    size = ftell(f);
    sprintf(str_size, "%d", size);
    fclose(f);
}

void read_commands()
{
    char command[9]; /* les comandes son màxim 9 caracters */
    while (1)
    {
        if (debug_flag == 0)
            printf("=> "); /* Per evitar barrejes amb els misatges debug */
        scanf("%9s", command);
        treat_command(command);
    }
}

void treat_command(char command[])
{
    if (strcmp(command, "quit") == 0)
    {
        pthread_cancel(alive_thread);
        close(udp_socket);
        debug("Finalitzats sockets");
        exit(1);
    }
    else
    {
        print_msg("Wrong command");
    }
}


void parse_parameters(int argc, char **argv)
{
    int i;
    if (argc > 1)
    {
        for (i = 0; i < argc; i++)
        { /* PARSING PARAMETERS */
            char const *option = argv[i];
            if (option[0] == '-')
            {
                switch (option[1])
                {
                case 'd':
                    debug_flag = 1;
                    break;
                case 'c':
                    strcpy(software_config_file, argv[i + 1]);
                    break;
                case 'f':
                    strcpy(network_config_file, argv[i + 1]);
                    break;
                default:
                    fprintf(stderr, "Wrong parameters Input \n");
                    exit(-1);
                }
            }
        }
    }
    debug("S'ha seleccionat l'opció debug");
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
    debug("S'ha obert el socket");
    return socket_created;
}

///////////////////////////////// FUNCIONS PER PRINTAR /////////////////////////////////

void debug(char msg[])
{
    if (debug_flag == 1)
    {
        time_t _time = time(0);
        struct tm *tlocal = localtime(&_time);
        char output[128];
        strftime(output, 128, "%H:%M:%S", tlocal);
        printf("%s: DEBUG -> %s \n", output, msg);
    }
}

void print_msg(char msg[])
{
    time_t _time = time(0);
    struct tm *tlocal = localtime(&_time);
    char output[128];
    strftime(output, 128, "%H:%M:%S", tlocal);
    printf("%s: MSG -> %s \n", output, msg);
}

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

void printd(char *str_given) {
	if (debug_activated) {
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
