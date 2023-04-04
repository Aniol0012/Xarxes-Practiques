#!/bin/bash

# Clonació dels arxius de configuració i el codi al github:
cp boot.cfg boot1.cfg boot2.cfg boot3.cfg client client.c client*.cfg equips.dat Makefile server server.cfg server.py Xarxes-Practica-1/src

# Clonació de arxius utils pero no necessaris per a realitzar la pràctica:
cp clonar.sh Xarxes-Practica-1/utils

# Després ens posicionem en la carpeta del github(Xarxes-Practica-1) per a fer el commit
cd Xarxes-Practica-1

# Si pasem el argument -p fem el commit


# Comprobar si el archivo existe
if [ ! -e version ]; then
    # Si no existe, crearlo con valor inicial 0
    echo "0" > version
fi

# Leer el valor actual del archivo
current_version=$(cat version)

# Incrementar el valor en una unidad
new_version=$((current_version+1))

# Escribir el nuevo valor en el archivo
echo "${new_version}" > version


if [[ $1 == "-p" ]]; then
    git add .
    git commit -m "$new_version"
    git push
fi