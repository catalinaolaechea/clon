#include "servicios.h"
#include <stdlib.h>
#include <utils/direcciones.h>
#include "inicializador.h"
#include "estados.h"
#include "cliente_placa.h"
#include "corto_plazo.h"
#include "jobs.h"

#define ERROR_SEM_INIT -1

void pedido_servicio_destruir(t_pedido_servicio* pedido) {
    free(pedido->archivo);  // free(NULL) no hace nada
    direcciones_destruir(pedido->direcciones);
    free(pedido);
}

static t_pedido_servicio* sacar_pedido(t_servicio* servicio) {
    pthread_mutex_lock(&servicio->mutex);
    t_pedido_servicio* pedido = queue_pop(servicio->cola);
    pthread_mutex_unlock(&servicio->mutex);
    return pedido;
}

static void* atender_servicio(void* arg) {
    t_servicio* servicio = arg;

    while (1) {
        sem_wait(&servicio->pedidos_pendientes);
        t_pedido_servicio* pedido = sacar_pedido(servicio);
        t_pcb* pcb = pedido->pcb;
        uint32_t jid = pcb->jid;  
        log_info(planificador_logger, LOG_INICIO_SERVICIO, jid, servicio->nombre);

        if (servicio->operar(pedido)) {
            if (!placa_deslockear_paginas(jid)) {
                log_error(planificador_logger, "No se pudieron deslockear las paginas del Job %u: vuelve a READY igual", jid);
            }

            log_info(planificador_logger, LOG_FIN_SERVICIO, jid, servicio->nombre);
            cambiar_estado(pcb, ESTADO_READY);
            corto_plazo_avisar_job_ready();
        } else {
            log_error(planificador_logger, "Fallo %s para el Job %u: se finaliza", servicio->nombre, jid);
            finalizar_job(pcb, FINALIZACION_ERROR_SERVICIO);
        }

        pedido_servicio_destruir(pedido);
    }

    return NULL;
}

void servicio_inicializar(t_servicio* servicio, const char* nombre, t_operacion_servicio operar) {
    servicio->nombre = nombre;
    servicio->operar = operar;
    servicio->cola = queue_create();
    pthread_mutex_init(&servicio->mutex, NULL);

    int response = sem_init(&servicio->pedidos_pendientes, 0, 0);
    if (response == ERROR_SEM_INIT) {
        log_error(planificador_logger, "Error al inicializar el semaforo del servicio %s", nombre);
        exit(EXIT_FAILURE);
    }
}

void servicio_iniciar(t_servicio* servicio) {
    pthread_t hilo_servicio;
    int resultado = pthread_create(&hilo_servicio, NULL, atender_servicio, servicio);
    if (resultado != 0) {
        log_error(planificador_logger, "Error al crear el hilo del servicio %s: %s", servicio->nombre, strerror(resultado));
        exit(EXIT_FAILURE);
    }

    pthread_detach(hilo_servicio); 
}

static void destruir_pedido(void* pedido) {
    pedido_servicio_destruir(pedido);
}

void servicio_destruir(t_servicio* servicio) {
    queue_destroy_and_destroy_elements(servicio->cola, destruir_pedido);
    pthread_mutex_destroy(&servicio->mutex);
    sem_destroy(&servicio->pedidos_pendientes);
}

void servicio_encolar(t_servicio* servicio, t_pedido_servicio* pedido) {
    pthread_mutex_lock(&servicio->mutex);
    queue_push(servicio->cola, pedido);
    pthread_mutex_unlock(&servicio->mutex);

    sem_post(&servicio->pedidos_pendientes);
}
