#include "inicializador.h"

//variables globalbes
t_config* planificador_config;
t_config_planificador config_planificador;
t_log* planificador_logger;
char* archivo_config;
char* path_job_inicial;

static t_algoritmo algoritmo_desde_string(char* valor) {

    if (strcmp(valor, "FIFO") == 0) {
        return ALGORITMO_FIFO;
    } else if (strcmp(valor, "RR") == 0) {
        return ALGORITMO_RR;
    } else if (strcmp(valor, "HRRN") == 0) {
        return ALGORITMO_HRRN;
    } else {
        fprintf(stderr, "ERROR - Algoritmo de planificación desconocido: %s\n", valor);
        exit(EXIT_FAILURE);
    }
}

void inicializar_config(){

    planificador_config = iniciar_config(archivo_config);

    config_planificador.puerto_escucha = so_config_get_string(planificador_config, "PLANIFICADOR", "PUERTO_ESCUCHA");
    config_planificador.ip_placa = so_config_get_string(planificador_config, "PLANIFICADOR", "IP_PLACA");
    config_planificador.puerto_placa = so_config_get_string(planificador_config, "PLANIFICADOR", "PUERTO_PLACA");
    config_planificador.ip_storage = so_config_get_string(planificador_config, "PLANIFICADOR", "IP_STORAGE");
    config_planificador.puerto_storage = so_config_get_string(planificador_config, "PLANIFICADOR", "PUERTO_STORAGE");
    config_planificador.log_level = log_level_from_string(so_config_get_string(planificador_config, "PLANIFICADOR", "LOG_LEVEL"));
    config_planificador.algoritmo_planificacion = algoritmo_desde_string(so_config_get_string(planificador_config, "PLANIFICADOR", "ALGORITMO_PLANIFICACION"));
    config_planificador.rr_quantum = so_config_get_int(planificador_config, "PLANIFICADOR", "RR_QUANTUM");
    config_planificador.estimacion_inicial = so_config_get_int(planificador_config, "PLANIFICADOR", "ESTIMACION_INICIAL");
    config_planificador.hrrn_alfa = so_config_get_double(planificador_config, "PLANIFICADOR", "HRRN_ALFA");
    config_planificador.grado_multiprogramacion = so_config_get_int(planificador_config, "PLANIFICADOR", "GRADO_MULTIPROGRAMACION");
    config_planificador.path_datos = so_config_get_string(planificador_config, "PLANIFICADOR", "PATH_DATOS");
    config_planificador.retardo_loader = so_config_get_int(planificador_config, "PLANIFICADOR", "RETARDO_LOADER");
    config_planificador.path_reportes = so_config_get_string(planificador_config, "PLANIFICADOR", "PATH_REPORTES");
}

void inicializar_log(){
    planificador_logger = iniciar_logger("planificador.log","PLANIFICADOR", config_planificador.log_level);
}