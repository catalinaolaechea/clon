#ifndef CORE_H_
#define CORE_H_
 
#include <utils/protocolo.h>
#include <utils/serializacion.h>
#include <utils/sockets.h>
#include <utils/utils.h>
#include <utils/config.h>
#include <utils/log.h>
 
extern t_config* core_config;
extern t_log* core_logger;
extern char* archivo_config;
extern int identificador;

typedef struct {
    char* log_level;
    char* ip_planificador;
    char* puerto_planificador;
    char* ip_placa;
    char* puerto_placa;
} t_config_core;

extern t_config_core configuracion;

void inicializar_config();
void inicializar_log();
 
#endif