// V1 (WORKING)
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
    fscanf(file, "%s", label);
    strcpy(config->name, label);

    fscanf(file, "%s", label);
    fscanf(file, "%s", label);
    strcpy(config->MAC, label);

    fscanf(file, "%s", label);
    fscanf(file, "%s", label);

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

// V2 (NOT SURE IF WORKING)
/*
void read_software_config_file(struct client_config *config) {
    FILE *file;
    char label[50], value[50];

    file = fopen(software_config_file, "r");
    if (file == NULL) {
        fprintf(stderr, "Ha sorgit un error a l'obrir l'arxiu");
        exit_program(EXIT_FAIL);
    }

    while (fscanf(file, "%s %s", label, value) == 2) {
        if (strcmp(label, "Id") == 0) {
            strncpy(config->name, value, sizeof(config->name) - 1);
            config->name[sizeof(config->name) - 1] = '\0';
        } else if (strcmp(label, "MAC") == 0) {
            strncpy(config->MAC, value, sizeof(config->MAC) - 1);
            config->MAC[sizeof(config->MAC) - 1] = '\0';
        } else if (strcmp(label, "NMS-Id") == 0) {
            if (strcmp(value, "localhost") == 0) {
                strncpy(config->server, "127.0.0.1", sizeof(config->server) - 1);
            } else {
                strncpy(config->server, value, sizeof(config->server) - 1);
            }
            config->server[sizeof(config->server) - 1] = '\0';
        } else if (strcmp(label, "NMS-UDP-port") == 0) {
            config->UDP_port = atoi(value);
        } else {
            fprintf(stderr, "Etiqueta desconeguda: %s\n", label);
        }
    }

    fclose(file);
}
*/