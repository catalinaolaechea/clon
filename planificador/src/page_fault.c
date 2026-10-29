#include "page_fault.h"
#include <semaphore.h>
#include <pthread.h>
#include <commons/collections/queue.h>
#include "inicializador.h"
#include "estados.h"
#include "cliente_placa.h"
#include "corto_plazo.h"
#include "jobs.h"

#define ERROR_SEM_INIT -1

typedef struct {
    t_pcb* pcb; 
    uint32_t nro_pagina;
} t_pedido_page_fault;

// Un solo hilo atiende las cargas en orden de llegada: el Offload de la Placa es de un solo hilo y
// mutex_placa igual las serializa, asi que mas hilos no ganan nada
static t_queue* pedidos;
static pthread_mutex_t mutex_pedidos = PTHREAD_MUTEX_INITIALIZER;
static sem_t sem_pedidos;

static t_pedido_page_fault* sacar_pedido(void) {
    pthread_mutex_lock(&mutex_pedidos);
    t_pedido_page_fault* pedido = queue_pop(pedidos);
    pthread_mutex_unlock(&mutex_pedidos);
    return pedido;
}

static void* atender_page_faults(void* arg) {
    (void) arg;  // firma exigida por pthread_create, no se usa

    while (1) {
        sem_wait(&sem_pedidos);
        t_pedido_page_fault* pedido = sacar_pedido();
        t_pcb* pcb = pedido->pcb;

        if (placa_cargar_pagina(pcb->jid, pedido->nro_pagina)) {
            cambiar_estado(pcb, ESTADO_READY);
            corto_plazo_avisar_job_ready();
        } else {
            // a READY no: el Core reintentaria la instruccion y daria el mismo Page Fault para siempre
            log_error(planificador_logger, "No se pudo cargar la pagina %u del Job %u: se finaliza", pedido->nro_pagina, pcb->jid);
            finalizar_job(pcb, FINALIZACION_ERROR_CARGA_PAGINA);
        }

        free(pedido);
    }

    return NULL;
}

void inicializar_page_faults(void) {
    pedidos = queue_create();

    int response = sem_init(&sem_pedidos, 0, 0);
    if (response == ERROR_SEM_INIT) {
        log_error(planificador_logger, "Error al inicializar semaforo sem_pedidos de Page Faults");
        exit(EXIT_FAILURE);
    }
}

void destruir_page_faults(void) {
    queue_destroy_and_destroy_elements(pedidos, free);  // las PCB no: son de las colas de estados
    sem_destroy(&sem_pedidos);
}

void iniciar_page_faults(void) {
    pthread_t hilo_page_faults;
    int resultado = pthread_create(&hilo_page_faults, NULL, atender_page_faults, NULL);
    if (resultado != 0) {
        log_error(planificador_logger, "Error al crear el hilo de Page Faults: %s", strerror(resultado));
        exit(EXIT_FAILURE);
    }

    pthread_detach(hilo_page_faults);  // no termina nunca: nadie le hace join
}

void encolar_page_fault(t_pcb* pcb, uint32_t nro_pagina) {
    t_pedido_page_fault* pedido = malloc(sizeof(t_pedido_page_fault));
    pedido->pcb = pcb;
    pedido->nro_pagina = nro_pagina;

    pthread_mutex_lock(&mutex_pedidos);
    queue_push(pedidos, pedido);
    pthread_mutex_unlock(&mutex_pedidos);

    sem_post(&sem_pedidos);
}
