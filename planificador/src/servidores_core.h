#ifndef SERVIDORES_CORE_H
#define SERVIDORES_CORE_H

#include <pthread.h>
#include <commons/collections/list.h>

#define ERROR_SERVER -1
#define ERROR_CLIENT -1
#define THREAD_CREATED 0

typedef struct {
    char* identificador;   // el que mandó el Core en el handshake
    int   fd_dispatch;     // -1 mientras no se conectó ese canal
    int   fd_interrupt;
} t_core_conectado;

extern t_list* cores_conectados;
extern pthread_mutex_t mutex_cores;

pthread_t iniciar_servidor_cores(void);

#endif