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

config_file = "server.cfg" # -c
equips_file = "equips.dat" # -u
localhost_ip = "127.0.0.1"

debug = False
show_exit_status = True

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

def load_authorized_clients(): # -u
    try:
        clients = {}
        with open(equips_file, "r") as f:
            for line in f:
                name, mac = line.strip().split(" ")
                clients[mac] = ClientInfo(name, mac)
        return clients
    except FileNotFoundError:
        printd("No s'ha pogut trobar l'arxiu de configuració '{}'".format(equips_file))
        exit_program(1)

def load_server_config(): # -c
    try:
        with open(config_file, "r") as f:
            config_data = {}
            for line in f:
                key, value = line.strip().split(maxsplit=1)
                config_data[key.lower()] = value
        return Config(config_data["id"], config_data["mac"], config_data["udp-port"], config_data["tcp-port"])
    except FileNotFoundError:
        printd("No s'ha pogut trobar l'arxiu de configuració '{}'".format(config_file))
        exit_program(1)

def generate_random_number():
    random_number = random.randint(100000, 999999)
    return str(random_number)

def correct_paquet(data, equip, addr):
    paq_type, id, mac, random, dades = struct.unpack("B7s13s7s50s", data)
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

def handle_udp(config, authorized_clients):
    sock_udp = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock_udp.bind((localhost_ip, int(config.UDP_port)))

    while True:
        message, (ip, port) = sock_udp.recvfrom(1024)
        paq_type, id, mac, random, dades = struct.unpack("B7s13s7s50s", message)
        id = id.decode().rstrip("\0")
        mac = mac.decode().rstrip("\0")
        random = random.decode().rstrip("\0")

        if mac in authorized_clients:
            equip = authorized_clients[mac]

            if paq_type == REGISTER_REQ:
                printd("S'ha rebut una petició de registre")
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

def tractar_parametres():
    global debug
    global equips_file
    global config_file

    i = 1
    while i < len(sys.argv):
        if sys.argv[i] == "-c": # server.cfg
            i += 1
            if i < len(sys.argv):
                config_file = sys.argv[i]
            else:
                print_usage()
                exit_program()
        elif sys.argv[i] == "-u": # equips.dat
            i += 1
            if i < len(sys.argv):
                equips_file = sys.argv[i]
            else:
                print_usage()
                exit_program()
        elif sys.argv[i] == "-d":
            print_debug_activated()
            debug = True
        else:
            print_usage()
            exit_program()
        i += 1

    printd("L'arxiu de configuració de l'equip és: " + config_file)
    printd("L'arxiu dels equips autoritzats és: " + equips_file)


def print_client_list(authorized_clients):
    print("Llistat dels clients autoritzats:")
    print_bar()
    print("{:<20} {:<20} {:<20} {:<20}".format("Nom", "MAC", "Nombre Aleatori", "Estat"))
    print_bar()
    
    for client in authorized_clients.values():
        print("{:<20} {:<20} {:<20} {:<20}".format(client.name, client.mac, client.random_number or "N/A", client.state))


def println(str):
    print(time.strftime("%H:%H:%S") + ": MSG.  =>  " + str)

def printt(str):
    print(time.strftime("%H:%H:%S") + ": TEST MSG.   =>  " + str)

def printd(str, exit_status = None):
    if (debug and exit_status == None):
        print(time.strftime("%H:%H:%S") + ": DEBUG MSG.  =>  " + str)
    elif (debug and exit_status != None):
        print(time.strftime("%H:%H:%S") + ": DEBUG MSG.  =>  " + str + ": " + exit_status)

def comandes(authorized_clients): # list i quit
    command = input('')

    if command == 'quit':
        exit_program()
    elif command == 'list':
        print_client_list(authorized_clients)
    else:
        println("Comanda incorrecta")

def print_usage():
    println("Uso: server.py [-c config_file] [-d] [-u equips_file]")
    println("  -c config_file: especifica un arxiu de configuració del servidor, default: server.cfg")
    println("  -d: habilita el mode de depuració (debug)")
    println("  -u equips_file: especifica un arxiu de clients autoritzats, default: equips.dat")

def print_debug_activated():
    print_bar()
    print("\t\t\tMode debug activat")
    print_bar()

def print_bar(length=75):
    print("─" * length)

def exit_program(exit_status = 0, print_line = False):
    # Es retorna amb 0 si ha estat una sortida controlada
    # Es retorna amb 1 o més si ha estat una sortida forçosa
    if print_line:
        print()
    printd("El programa s'ha aturat")
    if show_exit_status:
        printd("El codi d'acabament ha estat: " + str(exit_status))
    exit(exit_status)


def main():
    try:
        tractar_parametres()
        config = load_server_config()
        global server_id, server_mac
        server_id = config.name
        server_mac = config.mac
        authorized_clients = load_authorized_clients()

        udp_thread = threading.Thread(target=handle_udp, args=(config, authorized_clients)) # Ptsr passar una tupla buida
        udp_thread.daemon = True # Es tanca el fil automaticament
        udp_thread.start()

        while True: # Fem un bucle infinit per a l'espera de més comandes
            comandes(authorized_clients)
        
    except(KeyboardInterrupt):
        exit_program(1, True)

if __name__ == "__main__":
    main()
