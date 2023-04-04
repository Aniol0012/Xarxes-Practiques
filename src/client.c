#include <stdio.h>

#include <stdlib.h>

#include <string.h>

#include <stdbool.h> //Not used

#include <signal.h> //Not used

#include <unistd.h>

#include <arpa/inet.h>

#include <sys/socket.h>

#include <netinet/in.h>

#define PORT 12345
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

int current_state = DISCONNECTED;

// DEFINIM VARIABLES DE SORTIDA
#define EXIT_SUCESS 0
#define EXIT_FAIL - 1

char * NMS_Id = "127.0.0.1";
char * NMS_UDP_Port = "2023";
int socketfd;
struct sockaddr_in server_addr;

void send_message(int socketfd,
  const char * message) {
  ssize_t sent = send(socketfd, message, strlen(message), 0);
  if (sent < 0) {
    perror("Error sending message");
    exit(EXIT_FAIL);
  }
}

ssize_t receive_message(int socketfd, char * buffer, size_t size) {
  ssize_t received = recv(socketfd, buffer, size - 1, 0);
  if (received < 0) {
    perror("Error receiving message");
    exit(EXIT_FAIL);
  }
  buffer[received] = '\0';
  return received;
}

char * strdup(const char * ); // Inicialitzem strdup per a poder usarla

// DEFINIM LES VARIABLES AUXILIARS
void obrir_socket();
void read_config_file(const char * filename);
void print_msg(char * str, int current_state);
void print_bar();

int main(int argc, char * argv[]) {
  char buffer[BUFFER_SIZE];
  int debug = 0;
  char * config_file = NULL;

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-d") == 0) {
      debug = 1;
    } else if (strcmp(argv[i], "-c") == 0 && i + 1 < argc) { // Mirem si s'ha proporcionat el parametre -c seguit del nom del fitxer
      i++;
      config_file = argv[i];
    } else if (strcmp(argv[i], "-f") == 0 && i + 1 < argc) {
      i++;
      config_file = argv[i];
    } else {
      fprintf(stderr, "Usage: %s [-d] [-c <config_file>]\n", argv[0]);
      exit(EXIT_FAIL);
    }
  }

  if (debug) {
    print_bar();
    printf("Mode debug activat\n");
    print_bar();
  }

  if (config_file) { // Printem el fitxer de configuració proporcionat en el paràmetre -c
    printf("Config file: %s\n", config_file);
  }

  print_msg("Equip passa a l'estat", current_state);

  obrir_socket();

  if (connect(socketfd, (struct sockaddr * ) & server_addr, sizeof(server_addr)) < 0) {
    perror("Error connecting to server");
    close(socketfd);
    exit(EXIT_FAIL);
  }

  if (debug) {
    printf("Connected to server.\n");
  }

  // Comunicació amb el servidor basada en les especificacions.
  send_message(socketfd, "HELLO");
  receive_message(socketfd, buffer, BUFFER_SIZE);
  printf("Server response: %s\n", buffer);

  send_message(socketfd, "START");
  receive_message(socketfd, buffer, BUFFER_SIZE);
  printf("Server response: %s\n", buffer);

  // Aquí es podrien afegir més interaccions amb el servidor si fos necessari.

  close(socketfd);
  return 0;
}

void obrir_socket() {
  // Obrim el socket
  socketfd = socket(AF_INET, SOCK_DGRAM, 0); // Potser SOCK_DGRAM => SOCK_STREAM pero no crec

  if (socketfd < 0) {
    perror("Ha sorgit un error al crear el socket");
    exit(EXIT_FAIL);
  }

  // Binding amb el servidor
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(atoi(NMS_UDP_Port));
  server_addr.sin_addr.s_addr = inet_addr(NMS_Id);
}

void read_config_file(const char * filename) {
  FILE * file = fopen(filename, "r");
  if (!file) {
    perror("Error al abrir el archivo de configuración");
    exit(EXIT_FAIL);
  }

  char line[256];
  while (fgets(line, sizeof(line), file)) {
    char key[64], value[64];
    if (sscanf(line, "%63s = %63s", key, value) == 2) {
      if (strcmp(key, "NMS_Id") == 0) {
        NMS_Id = strdup(value);
      } else if (strcmp(key, "NMS_UDP_Port") == 0) {
        NMS_UDP_Port = strdup(value);
      }
    }
  }

  fclose(file);
}

void print_msg(char * str, int current_state) {
  char current_state_str[strlen("WAIT_REG_RESPONSE") + 1];

  // Creem un diccionari per a cada estat
  switch (current_state) {
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
  printf("MSG.  =>  %s: %s\n", str, current_state_str);
} 

void print_bar() {
  printf("───────────────────────────────────────────────────────────────────────────\n");
}