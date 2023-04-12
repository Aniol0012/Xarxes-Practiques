/*
void read_software_config_file(struct client_config *config)
{
    FILE *file;
    char label[50];
    file = fopen(software_config_file, "r");
    if (file == NULL)
    {
        fprintf(stderr, "Ha sorgit un error a l'obrir l'arxiu");
        exit_program(EXIT_FAIL);
    }

    fscanf(file, "%s", label);
    fscanf(file, "%s", label);   // No es la millor manera de fer-ho... pero ja que suposem que el fitxer es correcte
    strcpy(config->name, label); // Ens saltem les comprovacions 

    fscanf(file, "%s", label);
    fscanf(file, "%s", label);
    strcpy(config->MAC, label);

    fscanf(file, "%s", label);
    fscanf(file, "%s", label);
    // Tractar en una funció
    if (strcmp(label, "localhost") == 0)
    {
        strcpy(config->server, "127.0.0.1");
    }
    else
    {
        strcpy(config->server, label);
    }

    fscanf(file, "%s", label);
    fscanf(file, "%s", label);
    config->UDP_port = atoi(label);
    fclose(file);
}
*/