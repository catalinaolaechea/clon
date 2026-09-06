#include "inicializador.h"

t_config* core_config;
t_log* core_logger;
char* archivo_config;
char* identificador;

char* LOG_LEVEL;
char* IP_PLANIFICADOR;
char* PUERTO_PLANIFICADOR;
char* IP_PLACA;
char* PUERTO_PLACA;


void inicializar_config() {
    core_config = iniciar_config(archivo_config); 
    LOG_LEVEL           = so_config_get_string(core_config, "CORE", "LOG_LEVEL");
    IP_PLANIFICADOR     = so_config_get_string(core_config, "CORE", "IP_PLANIFICADOR");
    PUERTO_PLANIFICADOR = so_config_get_string(core_config, "CORE", "PUERTO_PLANIFICADOR");
    IP_PLACA            = so_config_get_string(core_config, "CORE", "IP_PLACA");
    PUERTO_PLACA        = so_config_get_string(core_config, "CORE", "PUERTO_PLACA");
}

void inicializar_log() {
    char nombre_log[64];
    snprintf(nombre_log, sizeof(nombre_log), "core%s.log", identificador);
 
    t_log_level nivel = log_level_from_string(LOG_LEVEL);
    core_logger = iniciar_logger(nombre_log, "CORE", nivel);
}