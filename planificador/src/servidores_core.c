#include "servidores_core.h"
#include "inicializador.h"
#include "sockets.h"

t_list* cores_conectados;
pthread_mutex_t mutex_cores = PTHREAD_MUTEX_INITIALIZER;

static int fd_servidor;

static void* escuchar_cores(void* arg) {
    (void) arg; // para evitar la advertencia de variable no utilizada

    while (1) {
        fd_cliente = esperar_cliente(fd_servidor);

        if (fd_cliente == ERROR_CLIENT) {
            log_error(planificador_logger, "Error al esperar cliente en el servidor de cores");
            continue;
        }

        int* ctx = malloc(sizeof(int));
        *ctx = fd_cliente;

        pthread_t hilo_core;

        int thread_result = pthread_create(&hilo_core, NULL, atender_core, ctx);
        if (thread_result != THREAD_CREATED) {
            log_error(planificador_logger, "No se pudo crear el hilo de atención del Core");
            free(ctx);
            liberar_conexion(&fd_cliente);
            continue;
        }

        pthread_detach(hilo_core);
    }

    return NULL;
}

pthread_t iniciar_servidor_cores(void) {
    fd_servidor = iniciar_servidor(config_planificador.puerto_escucha);

    if (fd_servidor == ERROR_SERVER) {
        log_error(planificador_logger, "No se pudo iniciar el servidor de cores en el puerto %s", config_planificador.puerto_escucha);
        exit(EXIT_FAILURE);
    }

    cores_conectados = list_create();

    pthread_t hilo_escucha;

    int thread_result = pthread_create(&hilo_escucha, NULL, escuchar_cores, NULL);

    if (thread_result != THREAD_CREATED) {
        log_error(planificador_logger, "No se pudo crear el hilo para escuchar cores");
        exit(EXIT_FAILURE);
    }

    log_info(planificador_logger, "Servidor de Cores escuchando en el puerto %s", config_planificador.puerto_escucha);

    return hilo_escucha;
}