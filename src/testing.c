#include <stdio.h>

#include <stdlib.h>

#include <time.h>

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
bool show_local_time = true;

// DEFINIM VARIABLES DE SORTIDA
#define EXIT_SUCESS 0
#define EXIT_FAIL -1

char *NMS_Id = "127.0.0.1";
char *NMS_UDP_Port = "2023";
int socketfd;
struct sockaddr_in server_addr;

void send_register_req(int socketfd)
{
    char pdu[78];
    pdu[0] = 0x00;                        // REGISTER_REQ
    strcpy(pdu + 1, "ID_EQUIP");          // id equipo
    strcpy(pdu + 8, "01:23:45:67:89:AB"); // MAC
    strcpy(pdu + 21, "1234567");          // Numero aleatorio
    memset(pdu + 28, 0, 50);              // Dades
    ssize_t sent = send(socketfd, pdu, sizeof(pdu), 0);
    if (sent < 0)
    {
        perror("Error enviando REGISTER_REQ");
        exit(EXIT_FAIL);
    }
}

void handle_received_pdu(int socketfd, char *pdu)
{
    switch (pdu[0]){
    case 0x02: // REGISTER_ACK
        current_state = REGISTERED;
        print_msg("Equip registrado", current_state);
        // Guardar número aleatorio y puerto TCP
        // pdu[21] a pdu[27] contiene el número aleatorio
        // pdu[28] a pdu[77] contiene el puerto TCP
        char random_num[8];
        strncpy(random_num, pdu + 21, 7);
        random_num[7] = '\0';
        char tcp_port[51];
        strncpy(tcp_port, pdu + 28, 50);
        tcp_port[50] = '\0';
        printf("Numero aleatorio: %s\n", random_num);
        printf("Puerto TCP: %s\n", tcp_port);
        break;
    case 0x04: // REGISTER_NACK
        current_state = DISCONNECTED;
        print_msg("Denegacion de registro", current_state);
        break;
    case 0x06: // REGISTER_REJ
        current_state = DISCONNECTED;
        print_msg("Rechazo de registro", current_state);
        break;
    case 0x0F: // ERROR
        current_state = DISCONNECTED;
        print_msg("Error de protocolo", current_state);
        break;
    default:
        current_state = DISCONNECTED;
        print_msg("PDU desconocida", current_state);
        break;
    }
}

void registration_procedure(int socketfd)
{
    int attempts = 0;
    int register_cycles = 0;
    int t = T;
    while (current_state != REGISTERED && register_cycles < O)
    {
        attempts = 0;
        while (current_state != REGISTERED && attempts < N)
        {
            send_register_req(socketfd);
            current_state = WAIT_REG_RESPONSE;
            print_msg("Enviado REGISTER_REQ", current_state);

            // Establecer temporizador
            struct timeval tv;
            tv.tv_sec = t;
            tv.tv_usec = 0;
            setsockopt(socketfd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof tv);

            // Recibir respuesta
            char pdu[78];
            ssize_t received = recv(socketfd, pdu, sizeof(pdu), 0);
            if (received < 0)
            {
                perror("No se ha recibido respuesta");
                if (attempts < P)
                {
                    t = T;
                }
                else if (t < Q * T)
                {
                    t += T;
                }
            }
            else
            {
                handle_received_pdu(socketfd, pdu);
            }
            attempts++;
        }

        if (current_state != REGISTERED)
        {
            register_cycles++;
            sleep(U);
        }
    }

    if (current_state != REGISTERED)
    {
        print_msg("No se pudo contactar con el servidor", current_state);
        exit(EXIT_FAIL);
    }
}

int main(int argc, char *argv[])
{
    // El resto de tu código main aquí
    // ...
    // Comunicación con el servidor basada en las especificaciones.
    send_message(socketfd, "HELLO");
    receive_message(socketfd, buffer, BUFFER_SIZE);
    printf("Server response: %s\n", buffer);

    send_message(socketfd, "START");
    receive_message(socketfd, buffer, BUFFER_SIZE);
    printf("Server response: %s\n", buffer);

    // Realizar el proceso de registro
    registration_procedure(socketfd);

    // Aquí es podrían añadir más interacciones con el servidor si fuese necesario.

    close(socketfd);
    return 0;
}

// Resto de tus funciones aquí
// ...


// Register client (Ns que fa)
void register_client() {
  int attempts = 0;
  int total_attempts = 0;
  int register_processes = 0;
  
  while (register_processes < O) {
    attempts = 0;
    bool success = false;

    while (attempts < N) {
      if (attempts < P) {
        sleep(T);
      } else if (attempts >= P) {
        int delta_t = (attempts - P + 1) * T;
        if (delta_t >= Q * T) {
          delta_t = Q * T;
        }
        sleep(delta_t);
      }

      send_message(socketfd, "REGISTER_REQ");
      current_state = WAIT_REG_RESPONSE;

      fd_set read_fds;
      struct timeval tv;
      FD_ZERO(&read_fds);
      FD_SET(socketfd, &read_fds);
      tv.tv_sec = T;
      tv.tv_usec = 0;

      int select_result = select(socketfd + 1, &read_fds, NULL, NULL, &tv);
      if (select_result > 0 && FD_ISSET(socketfd, &read_fds)) {
        char buffer[BUFFER_SIZE];
        ssize_t received = receive_message(socketfd, buffer, BUFFER_SIZE);

        if (strncmp(buffer, "REGISTER_ACK", 12) == 0) {
          current_state = REGISTERED;
          success = true;
          break;
        } else if (strncmp(buffer, "REGISTER_REJ", 12) == 0) {
          printf("El registre ha estat rebutjat pel servidor. Motiu: %s\n", buffer + 13);
          current_state = DISCONNECTED;
          break;
        } else if (strncmp(buffer, "REGISTER_NACK", 13) == 0) {
          break;
        }
      }

      attempts++;
      total_attempts++;
    }

    if (success) {
      break;
    }

    register_processes++;
    sleep(U);
  }

  if (current_state != REGISTERED) {
    printf("No s'ha pogut contactar amb el servidor després de %d intents.\n", total_attempts);
    exit(EXIT_FAIL);
  }
}

int main(int argc, char *argv[]) {
  // El codi existent ja proporcionat abans va aquí
  // ...
  // Aquí es podrien afegir més interaccions amb el servidor si fos necessari.

  register_client();

  // Implementa la resta de funcions aquí

  close(socketfd);
  return 0;
}
