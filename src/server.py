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

class Config:
    def __init__(self, name, mac, UDP_port, TCP_port):
        self.name = name
        self.mac = mac
        self.UDP_port = UDP_port
        self.TCP_port = TCP_port

def load_authorized_clients():
    clients = {}
    with open(client_file, "r") as f:
        for line in f:
            name, mac = line.strip().split(" ")
            clients[mac] = ClientInfo(name, mac)
    return clients

def load_server_config():
    with open(config_file, "r") as f:
        config_data = {}
        for line in f:
            key, value = line.strip().split(maxsplit=1)
            config_data[key.lower()] = value
    return Config(config_data["id"], config_data["mac"], config_data["udp-port"], config_data["tcp-port"])

def generate_random_number():
    random_number = random.randint(100000, 999999)
    return str(random_number)

def correct_paquet(data, equip, addr):
    type, id, mac, random, dades = struct.unpack("B7s13s7s50s", data)
    id = id.decode().rstrip("\0")
    mac = mac.decode().rstrip("\0")
    random = random.decode().rstrip("\0")
    if equip.mac != mac or equip.name != id:
        return False
    if equip.state == "REGISTERED" and equip.addr != addr:
        return False
    if equip.random_number != random and random != "0000000":
        return False
    return True

def handle_udp(authorized_clients):
    sock_udp = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock_udp.bind(("127.0.0.1", 2023)) # S'ha dobtenir de l'arxiu

    while True:
        message, (ip, port) = sock_udp.recvfrom(1024)
        type, id, mac, random, dades = struct.unpack("B7s13s7s50s", message)
        id = id.decode().rstrip("\0")
        mac = mac.decode().rstrip("\0")
        random = random.decode().rstrip("\0")

        if mac in authorized_clients:
            equip = authorized_clients[mac]
            if type == REGISTER_REQ:
                if correct_paquet(message, equip, (ip, port)):
                    printt("L'equip està registrat")
                    # Enviar un paquet de registre
                else:
                    printt("L'equip no està registrat")

                # Realizar las acciones necesarias, como enviar el paquete de registro ACK
            # Agregar otras acciones para los demás tipos de paquetes

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

# Modificar
def print_client_list(authorized_clients):
    print("Llistat dels clients autoritzats:")
    print_bar()
    print("{:<20} {:<20} {:<20} {:<20}".format("Nombre", "MAC", "Número Aleatorio", "Estado"))
    print_bar()
    
    for client in authorized_clients.values():
        print("{:<20} {:<20} {:<20} {:<20}".format(client.name, client.mac, client.random_number or "N/A", client.state))


def println(str):
    print(time.strftime("%H:%H:%S") + ": MSG.  =>  " + str)

def printt(str):
    print(time.strftime("%H:%H:%S") + ": TEST MSG.  =>  " + str)

def printd(str):
    if (debug):
        print(time.strftime("%H:%H:%S") + ": DEBUG MSG.  =>  " + str)

def comandes(authorized_clients): # list i quit
    command = input('')

    if command == 'quit':
        sys.exit(0)
    elif command == 'list':
        print_client_list(authorized_clients)
    else:
        println("Comanda incorrecta")


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
            else:
                print_usage()
                sys.exit(1)
        else:
            print_usage()
            sys.exit(1)
        i += 1

    printd("L'arxiu de configuració del client és: " + client_file)
    printd("L'arxiu de configuració de l'equip és: " + config_file)
    return client_file, config_file

def print_usage():
    print("Uso: server.py [-c client_file] [-d] [-f config_file]")
    print("  -c client_file: especifica un archivo de clientes autorizados diferente al predeterminado (equips.dat)")
    print("  -d: habilita el modo de depuración (debug)")
    print("  -f config_file: especifica un archivo de configuración diferente al predeterminado (server.cfg)")

def print_debug_activated():
    print_bar()
    print("\t\t\tMode debug activat")
    print_bar()

def print_bar(length=75):
    print("─" * length)

def main():
    try:
        tractar_parametres()
        config = load_server_config()
        global server_id, server_mac
        server_id = config.name
        server_mac = config.mac
        authorized_clients = load_authorized_clients()

        udp_thread = threading.Thread(target=handle_udp, args=(authorized_clients,)) # Ptsr passar una tupla buida
        udp_thread.daemon = True # Es tanca el fil automaticament
        udp_thread.start()

        comandes(authorized_clients)
        
    except(KeyboardInterrupt, SystemExit):
        print()
        printd("El programa s'ha aturat")
        exit(1)

if __name__ == "__main__":
    main()
