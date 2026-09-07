#ifndef PLACA_H_
#define PLACA_H_

#include <utils/protocolo.h>
#include <utils/serializacion.h>
#include <utils/sockets.h>
#include <utils/utils.h>
#include <utils/config.h>
#include <utils/log.h>
#include <pthread.h>
#include <utils/handshake.h>

extern t_config* placa_config;
extern t_log* placa_logger;
extern char* archivo_config;

//archivo de configuracion
extern char* PUERTO_ESCUCHA;
extern t_log_level LOG_LEVEL;
extern int TAM_MEMORIA;
extern int TAM_PAGINA;
extern int RETARDO_MEMORIA;
extern char* ALGORITMO_REEMPLAZO;
extern char* PATH_INSTRUCCIONES;
extern char* PATH_OFFLOAD;
extern int TAM_OFFLOAD;
extern int RETARDO_OFFLOAD;


typedef struct {
    int socket_core;
    int id_core;
}t_core_placa;

void inicializar_log();
void inicializar_config();

//funciones de servidor
void* atender_cliente(void* socket);
void* atender_core(void* core);
void* atender_planificador(void* panificador);


#endif