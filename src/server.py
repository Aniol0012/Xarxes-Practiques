import socket
import threading

PORT = 12345
BUFFER_SIZE = 1024
ADDRESS = "127.0.0.1"

def handle_client(client_socket, address):
    print(f"Client connected: {address}")
    while True:
        message = client_socket.recv(BUFFER_SIZE).decode("utf-8")
        if not message:
            break

        print(f"Received message from {address}: {message}")

        if message == "HELLO":
            response = "HELLO CLIENT"
        elif message == "START":
            response = "STARTING"
        else:
            response = "UNKNOWN COMMAND"

        client_socket.send(response.encode("utf-8"))

    print(f"Client disconnected: {address}")
    client_socket.close()

def main():
    server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server_socket.bind((ADDRESS, PORT))
    server_socket.listen(5)

    print(f"Server listening on {ADDRESS}:{PORT}")

    while True:
        client_socket, address = server_socket.accept()
        client_thread = threading.Thread(target=handle_client, args=(client_socket, address))
        client_thread.start()

if __name__ == "__main__":
    main()
