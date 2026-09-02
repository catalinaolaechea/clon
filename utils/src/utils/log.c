#include "so_log.h"
#include <stdio.h>
#include <stdlib.h>
#include <commons/log.h>

t_log* iniciar_logger(char* path_log, char* nombre_modulo, t_log_level nivel_inicial){
    t_log* logger = log_create(path_log,nombre_modulo,true,nivel_inicial);

    if(logger == NULL){
        fprintf(stderr, "No se pudo crear el archivo log: %s\n",path_log);
        exit(EXIT_FAILURE);
    }

    return logger;
}

void so_log_set_level(t_log* logger, char* log_level_str){
    log_set_level(logger,log_level_from_string(log_level_str));
}