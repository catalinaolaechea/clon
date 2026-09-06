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
extern char* identificador;

extern char* LOG_LEVEL;
extern char* IP_PLANIFICADOR;
extern char* PUERTO_PLANIFICADOR; 
extern char* IP_PLACA;
extern char* PUERTO_PLACA; 

void inicializar_config();
void inicializar_log();
 
#endif