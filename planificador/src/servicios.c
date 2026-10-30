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

// TODO SIS-76: leer el archivo de PATH_DATOS, escribirlo en la memoria del Job y esperar RETARDO_LOADER
static bool operar_loader(t_pedido_servicio* pedido) {
    log_warning(planificador_logger, "Loader del Job %u: operacion pendiente (SIS-76)", pedido->pcb->jid);
    return true;
}

// TODO SIS-77: pedir los caracteres por teclado y escribirlos en la memoria del Job
static bool operar_labeler(t_pedido_servicio* pedido) {
    log_warning(planificador_logger, "Labeler del Job %u: operacion pendiente (SIS-77)", pedido->pcb->jid);
    return true;
}

// TODO SIS-78: leer la memoria del Job y escribirla en hexa en PATH_REPORTES
static bool operar_logger(t_pedido_servicio* pedido) {
    log_warning(planificador_logger, "Logger del Job %u: operacion pendiente (SIS-78)", pedido->pcb->jid);
    return true;
}

static t_servicio servicios[CANTIDAD_SERVICIOS];

void inicializar_servicios(void) {
    servicio_inicializar(&servicios[SERVICIO_LOADER], "Loader", operar_loader);
    servicio_inicializar(&servicios[SERVICIO_LABELER], "Labeler", operar_labeler);
    servicio_inicializar(&servicios[SERVICIO_LOGGER], "Logger", operar_logger);
}

void iniciar_servicios(void) {
    for (int i = 0; i < CANTIDAD_SERVICIOS; i++) {
        servicio_iniciar(&servicios[i]);
    }
}

void destruir_servicios(void) {
    for (int i = 0; i < CANTIDAD_SERVICIOS; i++) {
        servicio_destruir(&servicios[i]);
    }
}

void encolar_en_servicio(t_servicio_id id, t_pedido_servicio* pedido) {
    servicio_encolar(&servicios[id], pedido);
}
