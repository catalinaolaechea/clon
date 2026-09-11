#ifndef PLANIFICADOR_H_
#define PLANIFICADOR_H_

#include <utils/protocolo.h>
#include <utils/serializacion.h>
#include <utils/handshake.h>
#include <utils/mensaje_prueba.h>
#include <utils/config.h>
#include <utils/log.h>
#include <utils/sockets.h>
#include <utils/utils.h>
#include <string.h>

typedef enum {
    ALGORITMO_FIFO,
    ALGORITMO_RR,
    ALGORITMO_HRRN
} t_algoritmo;

typedef struct {
    char* puerto_escucha;
    char* ip_placa;
    char* puerto_placa;
    char* ip_storage;
    char* puerto_storage;
    t_log_level log_level;
    t_algoritmo algoritmo_planificacion;
    int rr_quantum;
    int estimacion_inicial;
    double hrrn_alfa; // 0.0 < alfa < 1.0
    int grado_multiprogramacion;
    char* path_datos;
    int retardo_loader;
    char* path_reportes;
} t_config_planificador;

extern t_config* planificador_config;
extern t_log* planificador_logger;
extern t_config_planificador config_planificador;
extern char* archivo_config;
extern char* path_job_inicial;

void inicializar_config();
void inicializar_log();


#endif