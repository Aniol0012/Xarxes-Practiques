#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import sys
import select
import socket
import os
import datetime
import struct
import time
import random
import threading

# TIPUS DE PAQUETS
REGISTER_REQ = 0x00
REGISTER_ACK = 0x02
REGISTER_NACK = 0x04
REGISTER_REJ = 0x06
ERROR = 0x0F

ALIVE_INF = 0x10
ALIVE_ACK = 0x12
ALIVE_NACK = 0x14
ALIVE_REJ = 0x16

server_id = None
server_mac = None
client_file = "equips.dat"
config_file = "server.cfg"

debug = False

class ClientInfo:
    def __init__(self, name, mac, random_number=None, addr=None, state="DISCONNECTED"):
        self.name = name
        self.mac = mac
        self.random_number = random_number
        self.addr = addr
        self.state = state


def load_authorized_clients(file_name="equips.dat"):
    clients = {}
    with open(file_name, "r") as f:
        for line in f:
            name, mac = line.strip().split(" ")
            clients[mac] = ClientInfo(name, mac)
    return clients

def load_server_config():
    config = {}
    with open("server.cfg", "r") as f:
        for line in f:
            key, value = line.strip().split()
            config[key] = value
    return config

def generate_random_number():
    random_number = random.randint(100000, 999999)
    return str(random_number)

def handle_udp(authorized_clients):
    sock_udp = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock_udp.bind(("127.0.0.1", 2023)) # S'ha dobtenir de l'arxiu

    while True:
        message, (ip, port) = sock_udp.recvfrom(1024)
        type, id, mac, random, dades = struct.unpack("B7s13s7s50s", message)
        id = id.decode().rstrip("\0")
        printt(id)
        mac = mac.decode().rstrip("\0")
        random = random.decode().rstrip("\0")

        if type == REGISTER_REQ: # i primer random 0000000 id mac valid:
            pack = ack_pack(generate_random_number, 2024)
            # sendto
            # Mirar les comprovacions necessaries i segons si les cumpleix o no enviar ack


# PAQUETS FASE REGISTRE
def ack_pack(random, tcp_port):
    printt("Estem aqui")
    return struct.pack("B7s13s7s50s", REGISTER_ACK, server_id.encode(), server_mac.encode(), random.encode(), tcp_port.encode())

def nack_pack(motiu):
    pass
def rej_pack(motiu):
    pass

# PAQUETS FASE MANTENIMENT
def ack_alive_pack(random):
    pass
def nack_alive_pack(motiu):
    pass
def rej_alive_pack(motiu):
    pass

def print_client_list():
    pass

def println(str):
    print(time.strftime("%H:%H:%S") + ": MSG.  =>  " + str)

def printt(str):
    print(time.strftime("%H:%H:%S") + ": TEST MSG.  =>  " + str)

def printd(str):
    if (debug):
        print(time.strftime("%H:%H:%S") + ": DEBUG MSG.  =>  " + str)

def comandes(): # list i quit
    command = input('')

    if command == 'quit':
        sys.exit(0)
    elif command == 'list':
        println("Estem a list")
        # print_client_list()
    else:
        println("Comanda incorrecta")

def print_usage():
    print("Uso: server.py [-c client_file] [-d] [-f config_file]")
    print("  -c client_file: especifica un archivo de clientes autorizados diferente al predeterminado (equips.dat)")
    print("  -d: habilita el modo de depuración (debug)")
    print("  -f config_file: especifica un archivo de configuración diferente al predeterminado (server.cfg)")

def tractar_parametres():
    global debug
    global client_file
    global config_file

    i = 1
    while i < len(sys.argv):
        if sys.argv[i] == "-c":
            i += 1
            if i < len(sys.argv):
                client_file = sys.argv[i]
            else:
                print_usage()
                sys.exit(1)
        elif sys.argv[i] == "-d":
            print_debug_activated()
            debug = True
        elif sys.argv[i] == "-f":
            i += 1
            if i < len(sys.argv):
                config_file = sys.argv[i]
                printd("L'arxiu de configuració de l'equip és: " + config_file)
            else:
                print_usage()
                sys.exit(1)
        else:
            print_usage()
            sys.exit(1)
        i += 1

    return client_file, config_file

def print_debug_activated():
    print("───────────────────────────────────────────────────────────────────────────")
    print("\t\t\tMode debug activat")
    print("───────────────────────────────────────────────────────────────────────────")

def main():
    try:
        client_file, config_file = tractar_parametres()
        config = load_server_config()
        authorized_clients = load_authorized_clients()

        udp_thread = threading.Thread(target=handle_udp, args=(authorized_clients,)) # Ptsr passar una tupla buida
        udp_thread.daemon = True # Es tanca el fil automaticament
        udp_thread.start()

        comandes()
        
    except(KeyboardInterrupt, SystemExit):
        printd("El programa s'ha aturat")
        exit(1)

if __name__ == "__main__":
    main()
