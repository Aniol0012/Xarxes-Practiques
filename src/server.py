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

CHECK_ALIVE_PERIOD = 2  # Periode en segons per verificar si els clients estan vius

server_id = None
server_mac = None

config_file = "server.cfg" # -c
equips_file = "equips.dat" # -u
localhost_ip = "127.0.0.1"

debug = False
show_exit_status = False

class ClientInfo:
    def __init__(self, name, mac, random_number=None, addr=None, state="DISCONNECTED"):
        self.name = name
        self.mac = mac
        self.random_number = random_number
        self.addr = addr
        self.state = state
        self.first_packet_recieved = False
        self.alive_recieved = 0
        self.last_alive_time = None

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

def handle_udp(config, authorized_clients):
    sock_udp = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock_udp.bind((localhost_ip, int(config.UDP_port)))

    while True:
        message, addr = sock_udp.recvfrom(1024)
        client_thread = threading.Thread(target=handle_client_udp, args=(sock_udp, config, authorized_clients, message, addr))
        client_thread.daemon = True
        client_thread.start()

        udp_thread = threading.Thread(target=wait_timeout, args=(authorized_clients))
        udp_thread.daemon = True # Es tanca el fil automaticament
        udp_thread.start()

# Espera als j segons (4 per les proves de protocol) a que l'estat del client sigui ALIVE
def wait_timeout(authorized_clients):
    while True:
        time.sleep(CHECK_ALIVE_PERIOD)
        for client in authorized_clients.values():
            if client.state == "ALIVE":
                if (time.time() - client.last_alive_time) > CHECK_ALIVE_PERIOD * 2:
                    client.state = "DISCONNECTED"
                    print_state(client.name, client.state)

def correct_paquet(data, equip, addr):
    paq_type, id, mac, random, dades = struct.unpack("B7s13s7s50s", data)
    id = id.decode().rstrip("\0")
    mac = mac.decode().rstrip("\0")
    random = random.decode().rstrip("\0")

    if equip.mac != mac or equip.name != id:
        return "wrong_mac_or_id"
    
    if equip.addr != addr:
        return False
    
    if equip.first_packet_recieved == False:
        if equip.random_number != "0000000" and equip.random_number is not None:
            return False
    return True

def print_state(equip_name, equip_state):
    println("L'equip " + equip_name + " passa a l'estat " + equip_state)


def handle_client_udp(sock_udp, config, authorized_clients, message, addr):
    paq_type, id, mac, random, dades = struct.unpack("B7s13s7s50s", message)
    id = id.decode().rstrip("\0")
    mac = mac.decode().rstrip("\0")
    random = random.decode().rstrip("\0")

    if mac in authorized_clients:
        equip = authorized_clients[mac]
        equip.addr = addr

        if equip.first_packet_recieved:
            if equip.random_number == "0000000":
                equip.random_number = generate_random_number()
        else:
            equip.random_number = "0000000"
            equip.first_packet_recieved = True
        val_correct_paquet = correct_paquet(message, equip, addr)

        if paq_type == REGISTER_REQ:
            printd("S'ha rebut un REGISTER_REQ de " + equip.name)
            equip.state = "WAIT_DB_CHECK"
            print_state(equip.name, equip.state)
            if val_correct_paquet:
                equip.state = "REGISTERED"
                print_state(equip.name, equip.state)

                # Enviar un paquet de registre ACK
                ack_message = ack_pack(equip.random_number, config.TCP_port, equip.name)
                sock_udp.sendto(ack_message, addr)
            elif (val_correct_paquet == "wrong_mac_or_id"):
                # Enviar un paquet de registre NACK
                printd("La MAC o la id del equip no és correcte")
                equip.state = "WAIT_REG_RESPONSE"
                print_state(equip.name, equip.state)
                nack_message = nack_pack("La mac o la id del equip no és correcte", equip.name)
                sock_udp.sendto(nack_message, addr)
            else:
                printd("La mac o la id del equip no és correcte")
                equip.state = "WAIT_REG_RESPONSE"
                print_state(equip.name, equip.state)
                nack_message = nack_pack("La mac o la id del equip no és correcte", equip.name)
                sock_udp.sendto(nack_message, addr)
        elif paq_type == ALIVE_INF:
            printd("S'ha rebut un ALIVE_INF de " + equip.name)

            if ((equip.state != "WAIT_DB_CHECK") or (equip.state != "WAIT_REG_RESPONSE") or (equip.state != "DISCONNECTED")):
                if equip.state == "REGISTERED":
                    equip.state = "ALIVE"
                    print_state(equip.name, equip.state)
                    
                if correct_paquet(message, equip, addr):
                    ack_alive_message = ack_alive_pack(equip.random_number, equip.name)
                    sock_udp.sendto(ack_alive_message, addr)
                    printd("S'ha enviat un ALIVE_ACK")
                else:
                    printd("El paquet no és correcte")
                    nack_alive_message = nack_alive_pack("Dades incorrectes", equip.name)
                    sock_udp.sendto(nack_alive_message, addr)
            else:
                printd("El client encara no està registrat: " + equip.name)
        else:
            printd("S'ha rebut un paquet desconegut: " + hex(paq_type))
            err_message = err_pack("Error en el protocol", equip.name)
            sock_udp.sendto(err_message, addr)
    else:
        # REGISTER_REJ el client no està autoritzat
        rej_message = rej_pack("Client no autoritzat", equip.name)
        sock_udp.sendto(rej_message, addr)


# PAQUETS FASE REGISTRE
def ack_pack(random, tcp_port, equip_name):
    printd("Preparem un paquet REGISTER_ACK  per a " + equip_name)
    return struct.pack("B7s13s7s50s", REGISTER_ACK, server_id.encode(), server_mac.encode(), random.encode(), tcp_port.encode())

def nack_pack(motiu, equip_name):
    printd("Preparem un paquet REGISTER_NACK per a " + equip_name)
    return struct.pack("B7s13s7s50s", REGISTER_NACK, server_id.encode(), server_mac.encode(), "0000000".encode(), motiu.encode())

def rej_pack(motiu, equip_name):
    printd("Preparem un paquet REGISTER_REJ per a " + equip_name)
    return struct.pack("B7s13s7s50s", REGISTER_REJ, server_id.encode(), server_mac.encode(), "0000000".encode(), motiu.encode())

def err_pack(motiu, equip_name):
    printd("Preparem un paquet ERROR per a " + equip_name)
    return struct.pack("B7s13s7s50s", ERROR, server_id.encode(), server_mac.encode(), "0000000".encode(), motiu.encode())


# PAQUETS FASE MANTENIMENT
def ack_alive_pack(random, equip_name):
    printd("Preparem un paquet ALIVE_ACK per a " + equip_name)
    return struct.pack("B7s13s7s50s", ALIVE_ACK, server_id.encode(), server_mac.encode(), random.encode(), b'')

def nack_alive_pack(motiu, equip_name):
    printd("Preparem un paquet ALIVE_NACK per a " + equip_name)
    return struct.pack("B7s13s7s50s", ALIVE_NACK, server_id.encode(), server_mac.encode(), "0000000".encode(), motiu.encode())

def rej_alive_pack(motiu, equip_name):
    printd("Preparem un paquet ALIVE_REJ per a " + equip_name)
    return struct.pack("B7s13s7s50s", ALIVE_REJ, server_id.encode(), server_mac.encode(), "0000000".encode(), motiu.encode())

# Tractem els diferents paràmetres passats en l'execució del programa
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

# Es printa el llistat dels clients autoritzats i si estan registrats la seva adreça ip
def print_client_list(authorized_clients):
    print("Llistat dels clients autoritzats:")
    print_bar(95)
    print("{:<20} {:<20} {:<20} {:<20} {:<20}".format("Nom", "MAC", "Nombre Aleatori", "Estat", "Adreça IP"))
    print_bar(95)
    
    for client in authorized_clients.values():
        ip_addr = client.addr[0] if client.addr else "-"
        print("{:<20} {:<20} {:<20} {:<20} {:<20}".format(client.name, client.mac, client.random_number or "-", client.state, ip_addr))


def println(str):
    print(time.strftime("%H:%H:%S") + ": MSG.  =>  " + str)

def printd(str, exit_status = None):
    if (debug and exit_status == None):
        print(time.strftime("%H:%H:%S") + ": DEBUG MSG.  =>  " + str)
    elif (debug and exit_status != None):
        print(time.strftime("%H:%H:%S") + ": DEBUG MSG.  =>  " + str + ": " + exit_status)

def comandes(authorized_clients):
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

        # Fem un thread per a tractar els paquets udp
        udp_thread = threading.Thread(target=handle_udp, args=(config, authorized_clients))
        udp_thread.daemon = True # Es tanca el fil automaticament
        udp_thread.start()

        while True: # Fem un bucle infinit per a l'espera de més comandes
            comandes(authorized_clients)
        
    except(KeyboardInterrupt):
        exit_program(1, True)

if __name__ == "__main__":
    main()
