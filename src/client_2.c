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
#define ALIVE_INF 0x10 // Enviament d’informació d’alive
#define ALIVE_ACK 0x12 // Confirmació de recepció d’informació d’alive
#define ALIVE_NACK 0x14 // Denegació de recepció d’informació d’alive
#define ALIVE_REJ 0x16 // Rebuig de recepció d’informació d’alive

// TIPUS DE PAQUET PER L'ENVIAMENT DE L'ARXIU DE CONFIGURACIÓ (NO USAT)
#define SEND_FILE 0x20 // Petició d’enviament d’arxiu de configuració
#define SEND_DATA 0x22 // Bloc de dades de l’arxiu de configuració
#define SEND_ACK 0x24 // Acceptació de la petició d’enviament d’arxiu de configuració
#define SEND_NACK 0x26 // Denegació de la petició d’enviament d’arxiu de configuració
#define SEND_REJ 0x28 // Rebuig de la petició d’enviament d’arxiu de configuració
#define SEND_END 0x2A // Fi de l’enviament de dades de l’arxiu de configuració

// TIPUS DE PAQUET PER L'OBTENCIÓ DE L'ARXIU DE CONFIGURACIÓ (NO USAT)
#define GET_FILE 0x30 // Petició d’obtenció d’arxiu de configuració
#define GET_DATA 0x32 // Bloc de dades de l’arxiu de configuració
#define GET_ACK 0x34 // Acceptació d’obtenció d’arxiu de configuració
#define GET_NACK 0x36 // Denegació d’obtenció d’arxiu de configuració
#define GET_REJ 0x38 // Rebuig d’obtenció d’arxiu de configuració
#define GET_END 0x3A // Fi de l’obtenció de l’arxiu de configuració

// TEMPORITZADORS (EN SEGONS)

// FASE DE REGISTRE
#define T 1 // Temps en segons entre els primers p paquets REGISTER_REQ
#define P 2 // Número de paquets REGISTER_REQ abans d'incrementar l'interval entre paquets
#define Q 3 // Factor que multiplica T per obtenir l'interval màxim entre paquets REGISTER_REQ
#define U 2 // Temps en segons abans de reiniciar el procés de registre si no s'ha completat
#define N 6 // Nombre màxim de paquets REGISTER_REQ per a cada intent de registre
#define O 2 // Nombre màxim d'intents de registre abans de finalitzar el programa

//  FASEDE DE COMUNICACIÓ PERIÒDICA
#define R 2   // Envia ALIVE_INF cada R segons
#define U_2 3 // Número máxim de paquets ALIVE_INF sense rebre ALIVE_ACK

// DEFINIM CONSTANTS
#define EXIT_SUCCESS 0 // Codi de sortida en cas d'exit
#define EXIT_FAIL -1 // Codi de sortida en cas d'error
#define MAX_STATUS_LENGTH 18 // strlen('WAIT_REG_RESPONSE') = 17 + '\0'
#define MAX_FILENAME_LENGTH 12 // strlen('client2.cfg') = 11 + '\0'
#define BUFFER_SIZE 250 // Mida del buffer

// DADES DE L'ARXIU DE CONFIGURACIÓ DEL CLIENT
struct client_config {
    char name[7];
    char MAC[13];
    char server[20];
    char random[7];
    int UDP_port;
    int TCP_port;
};

// DADES PER A COMPROVAR ELS SEND_ALIVE
struct server_data {
    char name[7];
    char MAC[13];
    char random[7];
};

// PARAMETRES PER A TRACTAR ELS PAQUETS
struct parameters {
    struct client_config *client_data;
    struct sockaddr_in addr_client;
    struct sockaddr_in udp_addr_server;
    struct sockaddr_in tcp_addr_server;
    struct udp_PDU *data;
};

// LA PDU DEL PACKET UDP
struct udp_PDU {
    unsigned char type;
    char name[7];
    char mac[13];
    char random[7];
    char data[50];
};

// CONFIGURACIÓ ADICIONAL (S'HA D'ACTIVAR MANUALMENT)
bool show_local_time = true; // Mostra l'hora actual en els missatges | DEFAULT = TRUE
bool show_client_info = true; // Mostra la informació rebuda per l'arxiu de configuració | DEFAULT = TRUE
bool print_buffer = false; // Mostra la informació rebuda pel buffer | DEFAULT = FALSE
bool show_exit_status = false; // Mostre en mode debug el codi de retorn del programa | DEFAULT = FALSE
bool debug = false; // Estat inicial del mode debug (s'activa amb el paràmetre -d) | DEFAULT = FALSE


// DEFINIM VARIABLES GLOBALS
int udp_socket;
char client_config_file[MAX_FILENAME_LENGTH] = "client.cfg";
char current_state[MAX_STATUS_LENGTH] = "DISCONNECTED";
int NACK_counter = 0; // Número de REGISTER_NACK que s'han rebut
bool already_sent_alive = false; // Només printem en el cas que no s'hagi canviat el current_state a SEND_ALIVE

// DEFINIM ESTRUCTURES GLOBALS
struct sockaddr_in udp_addr_server, addr_client;
struct parameters parameters;
struct server_data server_data;
struct client_config client_data;
struct udp_PDU create_packet(char type[], char mac[], char random_num[], char data[]);

// DECLARACIÓ DE FUNCIONS
void bind_client(struct sockaddr_in *addr_client, struct client_config *client_data); // Fa el bind del client
void bind_server(struct sockaddr_in *udp_addr_server, struct client_config *client_data); // Fa el bind del servidor
void initialize_parameters(struct parameters *params, struct client_config *client_data, struct sockaddr_in *addr_client, struct sockaddr_in *udp_addr_server); // Configura i inicialitza els paràmetres utilitzats en el programa
void read_client_config(struct client_config *client_data); // Llegeix la configuració del arxiu de configuració del client
void send_register_request(struct client_config *client_data, struct sockaddr_in udp_addr_server, struct sockaddr_in addr_client); // Fa la petició de registre al servidor
void process_UDP_packet(); // Fa el tractament del packet UDP
void setup_UDP_packet(struct client_config *configuracio, struct udp_PDU *pdu, unsigned char request); // Preparem un paquet UDP
void send_alives(); // Envia comunicació periòdica al servidor
int open_socket(int protocol); // Obrim un socket en funció del protocol proporcionat
void *wait_quit(void *arg); // Una funció en un altre thread que espera comandes com el quit
bool is_state_equal(char *str); // Comparem el nou estat amb l'actual

// FUNCIONS PER A PRINTAR
void print_client_info(); // Printa la configuració llegida de l'arxiu de configuració del client
void get_time(char *time_str); // Enregistra l'hora actual
void print_time(); // Printar l'hora actual
void println(char *str); // Printa un missatge en mode no debug
void printd(char *str_given); // Printa els debugs en cas que estigui activat
void print_state(int current_state); // Printa i canvia els nous estats
void exit_program(int EXIT_STATUS); // Surt del programa segons el codi d'acabament
void print_n(); // Printa un \n
void print_bar(); // Printa una barra horitzontal decorativa

///////////////////////////////// CODI PRINCIPAL /////////////////////////////////

int main(int argc, char *argv[]) {

    pthread_t wait_quit_thread; // Creem el fil per a l'espera de la comanda
    pthread_create(&wait_quit_thread, NULL, wait_quit, NULL); // Iniciem el fil concurrent

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-d") == 0) {
            debug = true;
            print_bar();
            printf("\t\t\tMode debug activat\n");
            print_bar();
        } else if (strcmp(argv[i], "-c") == 0 && i + 1 < argc) {
            i++;
            strcpy(client_config_file, argv[i]);
        } else {
            fprintf(stderr, "Ús: %s [-d] [-c <config_file.cfg>]\n", argv[0]);
            exit_program(EXIT_SUCCESS); // És una sortida controlada del programa
        }
    }

    print_state(DISCONNECTED); // Partim de l'estat inicial disconnected

    read_client_config(&client_data);
    strcpy(client_data.random, "000000");
    client_data.random[6] = '\0';

    printd("S'ha llegit l'arxiu de configuració");

    print_client_info();

    // Obrim un socket UDP
    udp_socket = open_socket(IPPROTO_UDP); // Especifiquem que volem crear el socket en UDP

    // Fem el binding del client
    struct sockaddr_in addr_client;
    bind_client(&addr_client, &client_data);

    // Fem el binding del servidor
    struct sockaddr_in udp_addr_server;
    bind_server(&udp_addr_server, &client_data);

    // Inicialitzem l'estructura de paràmetres
    initialize_parameters(&parameters, &client_data, &addr_client, &udp_addr_server);

    send_register_request(&client_data, addr_client, udp_addr_server);
    pthread_join(wait_quit_thread, NULL); // Esperem que acabi el fil
    return EXIT_SUCCESS;
}

///////////////////////////////// FUNCIONS AUXILIARS /////////////////////////////////

// Fa el bind del client
void bind_client(struct sockaddr_in *addr_client, struct client_config *client_data) {
    memset(addr_client, 0, sizeof(struct sockaddr_in));
    addr_client->sin_family = AF_INET;
    addr_client->sin_addr.s_addr = htonl(INADDR_ANY);
    addr_client->sin_port = htons(client_data->UDP_port);
}

// Fa el bind del servidor
void bind_server(struct sockaddr_in *udp_addr_server, struct client_config *client_data) {
    memset(udp_addr_server, 0, sizeof(*udp_addr_server));
    udp_addr_server->sin_family = AF_INET;
    udp_addr_server->sin_addr.s_addr = inet_addr(client_data->server);
    udp_addr_server->sin_port = htons(client_data->UDP_port);
}

// Configura i inicialitza els paràmetres utilitzats en el programa
void initialize_parameters(struct parameters *params, struct client_config *client_data, struct sockaddr_in *addr_client, struct sockaddr_in *udp_addr_server) {
    params->client_data = client_data;
    params->addr_client = *addr_client;
    params->udp_addr_server = *udp_addr_server;
}

// Llegeix la configuració del arxiu de configuració del client
void read_client_config(struct client_config *client_data) {
    FILE *file;
    char label[50];
    file = fopen(client_config_file, "r");

    if (file == NULL) {
        fprintf(stderr, "Ha sorgit un error a l'obrir l'arxiu");
        exit_program(EXIT_FAIL);
    }

    fscanf(file, "%s", label);
    fscanf(file, "%s", label);
    strcpy(client_data->name, label);

    fscanf(file, "%s", label);
    fscanf(file, "%s", label);
    strcpy(client_data->MAC, label);

    fscanf(file, "%s", label);
    fscanf(file, "%s", label);

    if (strcmp(label, "localhost") == 0) {
        strcpy(client_data->server, "127.0.0.1");
    } else {
        strcpy(client_data->server, label);
    }

    fscanf(file, "%s", label);
    fscanf(file, "%s", label);
    client_data->UDP_port = atoi(label);
    fclose(file);
}

// Fa la petició de registre al servidor
void send_register_request(struct client_config *client_data, struct sockaddr_in udp_addr_server,
                           struct sockaddr_in addr_client) {
    int tries, max_tries = 2, n_bytes;
    bool is_registered = false;
    struct udp_PDU data;
    struct udp_PDU reg_pdu;
    socklen_t fromlen;
    char buffer[BUFFER_SIZE];
    fromlen = sizeof(udp_addr_server);

    // Creem un paquet de registre
    setup_UDP_packet(client_data, &reg_pdu, REGISTER_REQ);

    for (tries = 0; tries < max_tries && !is_state_equal("REGISTERED") && !is_state_equal("SEND_ALIVE"); tries++) {
        int packet_counter = 0;
        int t = T, p = P, q = Q, n = N, u = U;

        while (packet_counter < n && !is_state_equal("SEND_ALIVE") && !is_state_equal("REGISTERED")) {
            sendto(udp_socket, &reg_pdu, sizeof(reg_pdu), 0, (struct sockaddr *) &udp_addr_server, sizeof(udp_addr_server));
            packet_counter++;
            printd("S'ha enviat paquet REGISTER_REQ");

            if (is_state_equal("DISCONNECTED")) {
                print_state(WAIT_REG_RESPONSE); 
                sleep(T);
            }

            n_bytes = recvfrom(udp_socket, &data, sizeof(data), MSG_DONTWAIT, (struct sockaddr *) &udp_addr_server, &fromlen);
            if (n_bytes > 0) {
                is_registered = true;
                break;
            }

            if (packet_counter <= p) {
                t = T;
            } else if (packet_counter > p && packet_counter <= (p + (q - 1))) {
                t += T;
            } else {
                t = q * T;
            }

            sleep(t);
        }

        if (is_registered)
            break;

        if (tries < max_tries - 1) {
            printd("Reiniciem el procès de registre");
            sleep(u);
        }
    }

    if (tries == max_tries && !is_registered) { // Comprova si s'ha sortit del bucle per màxim d'intents
        println("Ha fallat el procès de registre. No s'ha pogut contactar amb el servidor.");
        exit_program(EXIT_FAIL);
    }

    if (print_buffer && debug) {
        sprintf(buffer, "Dades rebudes: bytes= %lu, type:%i, mac=%s, random=%s, dades=%s", sizeof(struct udp_PDU), data.type, 
                data.mac, data.random, data.data);
        printd(buffer);
    }
    parameters.data = &data;
    process_UDP_packet();
}

// Fa el tractament del packet UDP
void process_UDP_packet() {
    char buffer[BUFFER_SIZE]; // Buffer per emmagatzemar missatges temporals
    bool is_ALIVE_ACK_Valid = false;

    switch (parameters.data->type) {
        case REGISTER_REJ: // El registre ha estat rebutjat
            sprintf(buffer, "S'ha rebutjat el client, motiu: %s", parameters.data->data);
            println(buffer);
            print_state(DISCONNECTED);
            exit_program(EXIT_FAIL);
        case REGISTER_NACK: // El registre no s'ha realitzat correctament
            if (NACK_counter < O) {
                printd("S'ha rebut REGISTER_NACK, reiniciant el procès de registre");
                NACK_counter++;
                send_register_request(parameters.client_data, parameters.udp_addr_server, parameters.addr_client);
                break;
            }
            printd("S'ha superat el número màxim d'intents");
            exit_program(EXIT_FAIL);
        case REGISTER_ACK: // El registre s'ha realitzat correctament
            printd("S'ha rebut un REGISTER_ACK");
            if (!is_state_equal("REGISTERED")) {
                print_state(REGISTERED);
                parameters.client_data->TCP_port = atoi(parameters.data->data);
                strcpy(parameters.client_data->random, parameters.data->random);
                strcpy(server_data.random, parameters.data->random);
                strcpy(server_data.name, parameters.data->name);
                strcpy(server_data.MAC, parameters.data->mac);
                send_alives();
            }
            break;
        case ALIVE_ACK: // Confirmació de recepció d’informació d’alive
            // Comprova si les dades del paquet ALIVE_ACK són correctes
            if (strcmp(parameters.data->random, server_data.random) == 0 &&
                strcmp(parameters.data->name, server_data.name) == 0 &&
                strcmp(parameters.data->mac, server_data.MAC) == 0) {
                is_ALIVE_ACK_Valid = true;
            }
            if (!is_ALIVE_ACK_Valid) {
                printd("El ALIVE_ACK no és valid");
                break;
            }
            // Si ja hem rebut un ALIVE_ACK només el printem en mode debug
            if (already_sent_alive) {
                printd("S'ha rebut un ALIVE_ACK");
            } else {
                already_sent_alive = true;
                print_state(SEND_ALIVE);
                printd("S'ha rebut un ALIVE_ACK");
            }
            break;
        case ALIVE_REJ: // La comprovació periòdica ha estat rebutjada
            if (is_state_equal("SEND_ALIVE")) {
                printd("S'ha rebut un ALIVE_REJ");
                print_state(DISCONNECTED);
                printd("Reinici del procés de registre");
                send_register_request(parameters.client_data, parameters.udp_addr_server, parameters.addr_client);
            }
            break;
        default: // Tipus de paquet desconegut o no gestionat
            break;
    }
}

// Preparem un paquet UDP
void setup_UDP_packet(struct client_config *configuracio, struct udp_PDU *pdu, unsigned char request) {
    pdu->type = request;
    strcpy(pdu->name, configuracio->name);
    strcpy(pdu->mac, configuracio->MAC);
    strcpy(pdu->random, configuracio->random);
    memset(pdu->data, '\0', sizeof(char) * 49);

    if (request == REGISTER_REQ) {
        pdu->data[49] = '\0';
    }
}

// Envia comunicació periòdica al servidor
void send_alives() {
    int r = R, u = 0, temp, n_bytes;
    char buffer[BUFFER_SIZE];
    socklen_t fromlen;
    struct udp_PDU alive_pdu;
    struct udp_PDU data;
    setup_UDP_packet(parameters.client_data, &alive_pdu, ALIVE_INF);
    fromlen = sizeof(&parameters.udp_addr_server);

    while (true && u != U_2) {
        temp = sendto(udp_socket, &alive_pdu, sizeof(alive_pdu), 0, (struct sockaddr *) &parameters.udp_addr_server,
                      sizeof(parameters.udp_addr_server));
        if (temp == -1) {
            printd("Ha sorgit un error en el sendto");
        }
        sleep(r);
        n_bytes = recvfrom(udp_socket, &data, sizeof(data), MSG_DONTWAIT,
                           (struct sockaddr *) &parameters.udp_addr_server, &fromlen);
        if (n_bytes > 0) {
            parameters.data = &data;
            u = 0;
            if (print_buffer) {
                printd(buffer);
            }
            memset(buffer, '\0', sizeof(buffer));
            process_UDP_packet();
            if (is_state_equal("SEND_ALIVE") || u == U_2) {
                break;
            }
        } else {
            u++;
        }
    }

    if (is_state_equal("REGISTERED")) {
        printd("No s'ha rebut resposta del servidor, reiniciem el procés de registre");
        print_state(DISCONNECTED);
        send_register_request(parameters.client_data, parameters.udp_addr_server, parameters.addr_client);
    }
}

// Obrim un socket en funció del protocol proporcionat
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

// Una funció en un altre thread que espera comandes com el quit
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

// Comparem el nou estat amb l'actual
bool is_state_equal(char *str) {
    return strcmp(current_state, str) == 0;
}

///////////////////////////////// FUNCIONS PER PRINTAR /////////////////////////////////

// Printa la configuració llegida de l'arxiu de configuració del client
void print_client_info() {
    if (show_client_info && debug) {
        printd("La informació obtinguda de l'arxiu de configuració ha estat:");
        printf("\t\t\t┌───────────────────────────────────┐\n");
        printf("\t\t\t│\tId equip: %s\t    │\n", client_data.name);
        printf("\t\t\t│\tAdreça MAC: %s    │\n", client_data.MAC);
        printf("\t\t\t│\tNMS-Id: %s\t    │\n", client_data.server);
        printf("\t\t\t│\tNMS-UDP-Port: %d\t    │\n", client_data.UDP_port);
        printf("\t\t\t└───────────────────────────────────┘\n");
    }
}

// Enregistra l'hora actual
void get_time(char *time_str) {
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);

    sprintf(time_str, "%02d:%02d:%02d:", tm.tm_hour, tm.tm_min, tm.tm_sec);
}

// Printar l'hora actual
void print_time() {
    if (show_local_time) {
        char time_str[10];
        get_time(time_str);
        printf("%s ", time_str);
    }
}

// Printa un missatge en mode no debug
void println(char *str) {
    print_time();
    printf("MSG.  =>  %s\n", str);
}

// Printa els debugs en cas que estigui activat
void printd(char *str_given) {
    if (debug) {
        print_time();
        printf("DEBUG MSG.  =>  %s\n", str_given);
    }
}

// Printa i canvia els nous estats
void print_state(int current_state) {
    char current_state_str[MAX_STATUS_LENGTH];

    // Creem un diccionari per a cada estat
    switch (current_state) {
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

// Surt del programa segons el codi d'acabament
void exit_program(int EXIT_STATUS) {
    printd("El programa s'ha aturat");
    if (show_exit_status) {
        print_time();
        printf("DEBUG MSG.  =>  El codi d'acabament ha estat: %i\n", EXIT_STATUS);
    }
    exit(EXIT_STATUS);
}

// Printa un \n
void print_n() {
    printf("\n");
}

// Printa una barra horitzontal decorativa
void print_bar() {
    printf("───────────────────────────────────────────────────────────────────────────\n");
}