#include "log.h"
#include <stdio.h>
#include <stdlib.h>
#include <commons/log.h>

t_log* iniciar_logger(char* path_log, char* nombre_modulo, t_log_level nivel){
    if (nivel == LOG_LEVEL_INVALID) {
        fprintf(stderr, "ERROR - Nivel de log inválido: %d\n", nivel);
        exit(EXIT_FAILURE);
    }

    t_log* logger = log_create(path_log,nombre_modulo,true,nivel);

    if(logger == NULL){
        fprintf(stderr, "No se pudo crear el archivo log: %s\n",path_log);
        exit(EXIT_FAILURE);
    }

    return logger;
}