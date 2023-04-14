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

def create_package(package_type, src_id, mac, data):
    header_format = "!BBH6s6s"
    header = struct.pack(header_format, package_type, 0, len(data), src_id.encode(), mac.encode())

    # Add padding to the data field to make it a multiple of 4 bytes
    padded_data = data.encode() + b'\0' * ((4 - len(data) % 4) % 4)

    package = header + padded_data
    return package


def generate_random_number():
    max_value = 2**(7*8) - 1
    random_number = random.randint(0, max_value)
    return random_number

def parse_package(package):
    header_format = "!BBH6s6s"
    package_type, _, data_length, src_id, mac = struct.unpack(header_format, package[:12])
    data = package[12:12 + data_length].decode().rstrip('\0')

    return {"package_type": package_type, "src_id": src_id.decode(), "mac": mac.decode(), "data": data}

def handle_udp(sock_udp, authorized_clients):
    while True:
        message, addr = sock_udp.recvfrom(1024)
        package_type = message[0]

        if package_type == REGISTER_REQ:
            process_register_request(message, addr, authorized_clients)
        else:
            # Handle other types of packages (e.g., ALIVE_INF) here
            pass

def process_register_request(message, addr, authorized_clients, config, sock_udp):
    # Unpack message and check if client is authorized
    msg_data = parse_package(message)
    client_mac = msg_data["mac"]
    
    if client_mac not in authorized_clients:
        # Send REGISTER_REJ if client is not authorized
        response = create_package(REGISTER_REJ, config["Id"], client_mac, "Error, non-authorized client: bad name or MAC address")
        sock_udp.sendto(response, addr)
        return

    client = authorized_clients[client_mac]

    # Check if received data is correct
    if msg_data["name"] == client.name:
        if client.state == "DISCONNECTED":
            # Assign a new random number and change the state
            client.random_number = generate_random_number()
            client.addr = addr
            client.state = "WAIT_DB_CHECK"
            
        # Send REGISTER_ACK
        data = "{}\0{}".format(config["TCP-port"], client.random_number)
        response = create_package(REGISTER_ACK, config["Id"], client.mac, data)
        sock_udp.sendto(response, addr)

    else:
        # Send REGISTER_NACK if received data is incorrect
        response = create_package(REGISTER_NACK, config["Id"], client.mac, "Error, bad name or MAC address")
        sock_udp.sendto(response, addr)

def handle_udp(sock_udp, authorized_clients, config):
    while True:
        message, addr = sock_udp.recvfrom(1024)
        package_type = message[0]

        if package_type == REGISTER_REQ:
            process_register_request(message, addr, authorized_clients, config)
        else:
            # Handle other types of packages (e.g., ALIVE_INF) here
            pass

def main():
    config = load_server_config()
    authorized_clients = load_authorized_clients()

    # Initialize UDP socket
    sock_udp = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock_udp.bind((socket.gethostname(), int(config["UDP-port"])))

    # Initialize TCP socket
    sock_tcp = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock_tcp.bind((socket.gethostname(), int(config["TCP-port"])))
    sock_tcp.listen(5)

    # Start handling UDP requests
    udp_thread = threading.Thread(target=handle_udp, args=(sock_udp, authorized_clients, config))
    udp_thread.start()

if __name__ == "__main__":
    main()
