#include "so_config.h"
#include <stdio.h>
#include <stdlib.h>

t_config* iniciar_config(char* path_config){
    t_config* config = config_create(path_config);

    if(config ==NULL){
        fprintf(stderr, "No se pudo abrir el archivo de configuración: %s\n",path_config);
        exit(EXIT_FAILURE);
    }

    return config;
}

static void abortar_si_falta(t_config* config, char* nombre_modulo, char* clave){
    if (!config_has_property(config,clave)){
        fprintf(stderr, "ERROR - Falta clave obligatoria en el config %s\n",nombre_modulo,clave);
        config_destroy(config);
        exit(EXIT_FAILURE);
    }

}

char* so_config_get_string(t_config* config, char* nombre_modulo, char* clave){
    abortar_si_falta(config,nombre_modulo,clave);
    return config_get_string_value(config,clave);
}
int so_config_get_int(t_config* config, char* nombre_modulo, char*clave){
    abortar_si_falta(config,nombre_modulo,clave);
    return config_get_int_value(config,clave);
}

double so_config_get_double(t_config* config, char*nombre_modulo, char* clave){
    abortar_si_falta(config,nombre_modulo,clave);
    return strtod(config_get_double_value(config,clave),NULL);
}