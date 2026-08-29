#ifndef PLACA_H_
#define PLACA_H_

#include <utils/protocolo.h>
#include <utils/serializacion.h>
#include <utils/sockets.h>
#include <utils/utils.h>

extern t_config* placa_config;
extern t_log* placa_logger;
extern char* archivo_config;

//archivo de configuracion
extern char* PUERTO_ESCUCHA;
extern char* LOG_LEVEL;
extern int TAM_MEMORIA;
extern int TAM_PAGINA;
extern int RETARDO_MEMORIA;
extern char* ALGORITMO_REEMPLAZO;
extern char* PATH_INSTRUCCIONES;
extern char* PATH_OFFLOAD;
extern int TAM_OFFLOAD;
extern int RETARDO_OFFLOAD;

void inicializar_log();
void inicializar_config();


#endif