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

"""     Initialized default values for the settings"""
configuration_file = "server.cfg"
debug = False
authorized_machines_file = "equips.dat"
machines_data = []
clients_timeout = []
IP = "127.0.0.1"
quit_command = False
lock_alives = threading.Lock()

"""   Created a function to read parameters from the program arguments"""
def read_parameters():
    global configuration_file
    global debug
    global authorized_machines_file
    for parameter in range(1, len(sys.argv)):
        if sys.argv[parameter] == "-c":
            if len(sys.argv) < parameter + 2:
                print("Parameter error, possible parameters:\n\t-c:\tAllows you to specify the file where configuration is stored, followed by the route of the configuration file.\n\t-d:\tActivates the debug mode.\n");
                sys.exit()
            else:
                configuration_file = sys.argv[parameter+1]
        elif sys.argv[parameter - 1] == "-c":
            pass
        elif sys.argv[parameter] == "-d":
            debug = True
        elif sys.argv[parameter] == "-u":
            if len(sys.argv) < parameter + 2:
                print("Parameter error, possible parameters:\n\t-c:\tAllows you to specify the file where configuration is stored, followed by the route of the configuration file.\n\t-d:\tActivates the debug mode.\n\t-u:\tAllows you to specify the file where authorized machines data is stored, followed by the route of the authorized machines file.");
                sys.exit()
            else:
                authorized_machines_file = sys.argv[parameter + 1]
        elif sys.argv[parameter - 1] == "-u":
            pass
        else:
            print("Parameter error, possible parameters:\n\t-c:\tAllows you to specify the file where configuration is stored, followed by the route of the configuration file.\n\t-d:\tActivates the debug mode.\n\t-u:\tAllows you to specify the file where authorized machines data is stored, followed by the route of the authorized machines file.");
            sys.exit()
read_parameters()
"""     This function reads the configuration file and stores the needed data of the server"""
def set_parameters(file):
    global configuration_file
    with open(file) as f:
        server_data = f.readlines()
    server_data = [x.strip() for x in server_data]
    for line in range(len(server_data)):
        if server_data[line] == "":
            del server_data[line]
    for line in range(len(server_data)):
            server_data[line] = server_data[line].split()
    return server_data[0][1], server_data[1][1], server_data[2][1], server_data[3][1]

"""     Setted the variables to store the server information"""
name, MAC, udp_port, tcp_port = set_parameters(configuration_file)

def print_debug(message_debug):
    if debug == True:
        print(datetime.datetime.now().time().strftime("%H:%M:%S") + ": " + message_debug)

"""     This function creates the data structure where the data of the clients will be stored, in order to query it later"""
def initialize_machines_data(file):
    global machines_data
    with open(file) as f:
        machines_data = f.readlines()
    machines_data = [x.strip() for x in machines_data]
    for line in range(len(machines_data)):
        if machines_data[line] == "":
            del machines_data[line]

    for line in range(len(machines_data)):
            machines_data[line] = machines_data[line].split()
    for x in range(len(machines_data)):
        machines_data[x].insert(0, "DISCONNECTED")
        while len(machines_data[x][1]) < 7:
                machines_data[x][1] = machines_data[x][1] + "\0"
        machines_data[x][2] = machines_data[x][2] + "\0"
        machines_data[x].append("000000\0")
        machines_data[x].append("")
initialize_machines_data(authorized_machines_file)

"""     This function return the actual number of seconds of the day as an int, we will use it to know the timeouts of the clients"""
def get_clock_seconds():
    return int(str(datetime.datetime.now().time())[0:2])*3600 + int(str(datetime.datetime.now().time())[3:5])*60 + int(str(datetime.datetime.now().time())[6:8])

"""     This function initialize the data structure used for clients timeout, being the first number an indicator of the seconds it has to wait before timeout, and the second, the time when the last alive was received"""
def initialize_clients_timeout():
    global clients_timeout
    for x in range(len(machines_data)):
        clients_timeout.append([0,0])
initialize_clients_timeout()

sock_udp = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock_udp.bind((IP, int(udp_port)))

def read_commands():
    command = input("")
    if command == "quit":
        print_debug("Exiting the server")
        sock_udp.close()
        os._exit(0)
    elif command == "list":
        print_debug("Showing the state of the authorized clients")
        print("STATE\t\tNAME\t\tMAC\t\tRANDOM_NUMBER\t\tIP\n")
        for client in machines_data:
            if client[0] == "ALIVE":
                print (client[0] + "\t\t" + client[1] + "\t\t" + client[2] + "\t" + client[3] + "\t\t\t" + client[4] + "\n")
            else:
                print (client[0] + "\t" + client[1] + "\t\t" + client[2] + "\t" + client[3] + "\t\t\t" + client[4] + "\n")
    else:
        print_debug("Invalid command")


"""         Function that creates the main packages of the server, using package_type, random number and data provided"""
def create_package(package_type, id, data):
    package = struct.pack('B', package_type)
    package += b"\0\0\0\0\0\0\0"  # Afegeix b davant de les cometes per indicar que és una seqüència de bytes
    package += "000000000000".encode('utf-8')  # Converteix la cadena en bytes amb el mètode 'encode'
    package += b"\0"
    package += "000000".encode('utf-8')
    package += b"\0"
    package += data.encode('utf-8')  # Converteix la cadena 'data' en bytes
    package += b"\0"
    package += struct.pack('78B', *([0] * 78))
    return package



"""     Funtion to return a string of six digits corresponding to a random number"""
def generate_random():
    random_number = ""
    for x in range(0,6):
        random_number = random_number + str(random.randint(0,9))
    return random_number

# def generate_random_number():
#     max_value = 2**(7*8) - 1
#     random_number = random.randint(0, max_value)
#     return random_number # ptsr: return "" + random_number

"""     Function to treat the package received"""
def process_package(package, addr):
    global machines_data
    if package[0] == REGISTER_REQ:
        for machine in range(len(machines_data)):
            if package[1:8] == machines_data[machine][1]:
                if package[8:21] == machines_data[machine][2]:
                    if package[21:28] == machines_data[machine][3]:
                        if machines_data[machine][0] == "DISCONNECTED":
                            machines_data[machine][0] = "REGISTERED"
                            print("Client numero " + str(machine+1) + ": State changed from DISCONNECTED to REGISTERED")
                            random_number = generate_random()
                            sock_udp.sendto(create_package(REGISTER_ACK, random_number, str(tcp_port) + "\0"), addr)
                            print_debug("Received REGISTER_ACK message correctly\n")
                            print_debug("REGISTER_ACK sent\n")
                            machines_data[machine][3] = random_number + "\0"
                            clients_timeout[machine][0] = get_clock_seconds()
                            if clients_timeout[machine][1] == 0:
                                clients_timeout[machine][1] = 1
                            machines_data[machine][4] = addr[0]
                            return
                        elif machines_data[machine][0] == "REGISTERED" or machines_data[machine][0] == "ALIVE":
                            sock_udp.sendto(create_package(REGISTER_ACK, machines_data[machine][3], str(tcp_port) + "\0"), addr)
                            print_debug("Received REGISTER_ACK message correctly\n")
                            print_debug("REGISTER_ACK sent\n")
                            return
                        else:
                            print_debug("Received REGISTER_REQ out of sequence\n")
                            print_debug("REGISTER_NACK sent\n")
                            sock_udp.sendto(create_package(REGISTER_NACK, "000000", "Error, REGISTER_REQ out of sequence"), addr)
                            return
                    else:
                        print_debug("Received REGISTER_REQ with bad random number\n")
                        print_debug("REGISTER_NACK sent\n")
                        sock_udp.sendto(create_package(REGISTER_NACK, "000000", "Error, bad random number"), addr)
                        return
        print_debug("Received REGISTER_REQ from non-authorized client, bad MAC adress or name")
        print_debug("REGISTER_REJ sent")
        sock_udp.sendto(create_package(REGISTER_REJ, "000000", "Error, non-authorized client: bad name or MAC address"), addr)
        return
    elif package[0] == ALIVE_INF:
        for machine in range(len(machines_data)):
            if package[1:8] == machines_data[machine][1]:
                if package[8:21] == machines_data[machine][2]:
                    if package[21:28] == machines_data[machine][3]:
                        if machines_data[machine][0] == "ALIVE" or machines_data[machine][0] == "REGISTERED":
                            if machines_data[machine][4] == addr[0]:
                                if machines_data[machine][0] == "REGISTERED":
                                    print ("Client numero " + str(machine+1) + ": State changed from REGISTERED to ALIVE")
                                    machines_data[machine][0] = "ALIVE"
                                sock_udp.sendto(create_package(ALIVE_ACK, machines_data[machine][3], ""), addr)
                                print_debug("Received ALIVE_INF message correctly")
                                print_debug("ALIVE_ACK sent")
                                clients_timeout[machine][0] = get_clock_seconds()
                                if clients_timeout[machine][1] == 1:
                                    clients_timeout[machine][1] = 2
                                return
                            else:
                                print_debug("Received ALIVE_INF from bad ip")
                                print_debug("ALIVE_NACK sent")
                                sock_udp.sendto(create_package(ALIVE_NACK, machines_data[machine][3], "Error: bad ip"), addr)
                                return
                        else:
                            print_debug("Received ALIVE_INF out of sequence")
                            print_debug("ALIVE_NACK sent")
                            sock_udp.sendto(create_package(ALIVE_NACK, machines_data[machine][3], "Error: package out of sequence"), addr)
                            return
                    else:
                        print_debug("Received ALIVE_INF with bad random number")
                        print_debug("ALIVE_NACK sent")
                        sock_udp.sendto(create_package(ALIVE_NACK, machines_data[machine][3], "Error: bad random number"), addr)
                        return
        print_debug("Received ALIVE_INF from non-authorized client: bad name or MAC address")
        print_debug("ALIVE_REJ sent")
        sock_udp.sendto(create_package(ALIVE_REJ, machines_data[machine][3], "Error, non-authorized client: bad name or MAC address"), addr)
        return
def timeout_alives():
    global clients_timeout
    global machines_data
    while True:
        lock_alives.acquire(True)
        for x in range(len(clients_timeout)):
            if clients_timeout[x][1] == 1:
                if get_clock_seconds() > clients_timeout[x][0] + 6:
                    print_debug("Client " + machines_data[x][1] + " DISCONNECTED: not received the first alive in 6 seconds")
                    clients_timeout[x][1] = 0
                    machines_data[x][0] = "DISCONNECTED"
                    machines_data[x][3] = "000000\0"
                    machines_data[x][4] = ""
            elif clients_timeout[x][1] == 2:
                if get_clock_seconds() > clients_timeout[x][0] + 9:
                    print_debug("Client " + machines_data[x][1] + " DISCONNECTED: not received alives in 9 seconds")
                    clients_timeout[x][1] = 0
                    machines_data[x][0] = "DISCONNECTED"
                    machines_data[x][3] = "000000\0"
                    machines_data[x][4] = ""
        lock_alives.release()
alives_timeout_thread = threading.Thread(target = timeout_alives).start()
while True:
    readable, writable, exceptional = select.select([sock_udp, sys.stdin], [], [])
    for s in readable:
        if s is sock_udp:
            message, addr = sock_udp.recvfrom(78)
            process_package(message, addr);
        else:
            read_commands();
