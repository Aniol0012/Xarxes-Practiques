#!/bin/bash

#clear

### FUTURES IMPLEMENTACIONS ###
    # git pull
    # git clone
    # git rev-list --count HEAD # Retorna el numero actual de commit (s'hauria d'incrementar)

common_help_panel() {
    echo -e "On: \n\t-p és per a fer el commit al github, sinó nomes se copia."
    echo -e "\tMISSATGE: Missatge per a incloure al commit, si no s'inclou un missatge 
        es printarà en numero de versió actual"
    echo -e "\t-r per a començar amb la versió igual a 1"
    echo -e "\t-r <versió> per a posar la versió desitjada (es recomanable
        posar-ho en el nombre de commit que ens trobem actualment)"
    echo "───────────────────────────────────────────────────────────────────────────"
}

help_panel() {
    echo "───────────────────────────────────────────────────────────────────────────"
    echo "Recorda que també pots fer servir: ./clonar.sh <-p <"MISSATGE">>"
    common_help_panel
}

help_panel2() {
    echo "───────────────────────────────────────────────────────────────────────────"
    echo "Recorda que l'us és: ./clonar.sh <-p <"MISSATGE">>"
    common_help_panel
}

help_panel3() {
    echo "───────────────────────────────────────────────────────────────────────────"
    echo "L'us és: ./clonar.sh <-p <"MISSATGE">>"
    common_help_panel
}

if [[ $1 == "--help" ]]; then
    help_panel3
    exit 0
fi

if [[ $# == 0 ]]; then
    help_panel # Si no es passen arguments
else
    help_panel2 # En cas que es passin arguments
fi

# Nom de la carpeta on es guarda la pràctica del github,
# si es vol en una altra carpeta s'ha de posar el pwd sencer.
github_folder_name=Xarxes-Practica-1

# Clonació dels arxius de configuració i el codi al github:
cp boot.cfg boot1.cfg boot2.cfg boot3.cfg client *.c client*.cfg equips.dat Makefile server server.cfg server.py $github_folder_name/src

# Clonació d'arxius útils però no necessaris per a realitzar la pràctica:
cp clonar.sh $github_folder_name/utils

echo "Tots els arxius s'han copiat correctament"

# Després ens posicionem en la carpeta del github (Xarxes-Practica-1) per a fer el commit
cd $github_folder_name

# Copiem el git per a que el VSCode pugui fer un seguiment
cp -r .git .. > /dev/null

# Si passem l'argument -p fem el commit
file_name="version.txt"
path="utils/" # Si es vol en la ruta del mateix github, deixar en blanc
file_def=$path$file_name

# Si passem l'argument -r
if [[ $1 == "-r" ]]; then
    if [[ $2 == "" ]]; then
        rm -rf $file_def
        echo "S'ha borrat l'archiu de versions"
    else
        echo "$2" > $file_def
        echo "S'ha posat l'arxiu de versions igual a $2"
    fi
    exit
fi

# Comprova si l'arxiu existeix
if [ ! -e "$file_def" ]; then
    # Si no existeix, crearlo amb valor inicial a 1
    echo "1" > $file_def
fi

# Llexeix el valor actual del fitxer
#current_version=$(cat $file_def)
current_version=$(echo $(git rev-list --count HEAD))

# Incrementa la versió en una unitat si es fa push, sinó es maté igual
if [[ $1 == "-p" ]]; then
    new_version=$((current_version+1))
else 
    new_version=$((current_version))
fi

# Escriu en nou valor a l'axriu
#echo "${new_version}" > $file_def

# Ens guardem tots els arguments a partir del 2n (inclós)
for arg in "${@:2}"; do
    commit=$commit" "$arg # Posem un espai entre arguments
done

if [[ $1 == "-p" ]]; then
    git add .
    if [[ $2 == "" ]];then
       git commit -m "v$new_version"
    else
       git commit -m "v$new_version - $commit"
    fi
    git push
fi