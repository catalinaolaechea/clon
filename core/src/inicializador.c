#include "inicializador.h"

t_config* core_config;
t_log* core_logger;
char* archivo_config;
int identificador;

t_config_core configuracion;

void inicializar_config() {
    core_config = iniciar_config(archivo_config);
    configuracion.log_level           = so_config_get_string(core_config, "CORE", "LOG_LEVEL");
    configuracion.ip_planificador     = so_config_get_string(core_config, "CORE", "IP_PLANIFICADOR");
    configuracion.puerto_planificador = so_config_get_string(core_config, "CORE", "PUERTO_PLANIFICADOR");
    configuracion.ip_placa            = so_config_get_string(core_config, "CORE", "IP_PLACA");
    configuracion.puerto_placa        = so_config_get_string(core_config, "CORE", "PUERTO_PLACA");
}

void inicializar_log() {
    char nombre_log[64];
    snprintf(nombre_log, sizeof(nombre_log), "core%d.log", identificador);
 
    t_log_level nivel = log_level_from_string(configuracion.log_level);
    core_logger = iniciar_logger(nombre_log, "CORE", nivel);
}