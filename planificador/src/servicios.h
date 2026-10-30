#ifndef SERVICIOS_H
#define SERVICIOS_H

#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#include <semaphore.h>
#include <commons/collections/list.h>
#include <commons/collections/queue.h>
#include "pcb.h"

#define LOG_INICIO_SERVICIO "## (%u) inicia %s"
#define LOG_FIN_SERVICIO "## (%u) finalizó %s y pasa a READY"

typedef struct {
    t_pcb* pcb;
    char* archivo;          // solo LOAD_BATCH, NULL en los demas
    uint32_t tamanio;
    t_list* direcciones;    // de t_direccion_fisica, con las paginas lockeadas
} t_pedido_servicio;

// Libera el pedido, su archivo y su lista de direcciones. La PCB no: es de las colas de estados.
void pedido_servicio_destruir(t_pedido_servicio* pedido);

// La operacion propia de cada servicio. false si falla: el Job se finaliza.
typedef bool (*t_operacion_servicio)(t_pedido_servicio* pedido);

// Un hilo por servicio que atiende su cola de a un pedido, en orden de llegada
typedef struct {
    const char* nombre;
    t_operacion_servicio operar;
    t_queue* cola;
    pthread_mutex_t mutex;
    sem_t pedidos_pendientes;
} t_servicio;

void servicio_inicializar(t_servicio* servicio, const char* nombre, t_operacion_servicio operar);
void servicio_iniciar(t_servicio* servicio);
void servicio_destruir(t_servicio* servicio);

// El Job del pedido ya tiene que estar en BLOCKED: el hilo del servicio lo pasa a READY al terminar.
void servicio_encolar(t_servicio* servicio, t_pedido_servicio* pedido);

#endif
