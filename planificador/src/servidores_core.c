#include "servidores_core.h"
#include "inicializador.h"

t_list* cores_conectados;
pthread_mutex_t mutex_cores = PTHREAD_MUTEX_INITIALIZER;

static int fd_servidor;

static void atender_mensaje(int fd, uint8_t op_code, int identificador) {
    switch (op_code) {
        case MENSAJE_PRUEBA:
            log_info(planificador_logger, "Recibido MENSAJE_PRUEBA del Core con identificador: %d", identificador);
            break;
        default:
            log_warning(planificador_logger, "Código de operación desconocido recibido: %u", op_code);
    }
}

static void* atender_core(void* ctx) {
    int fd = *(int*) ctx;
    free(ctx);

    t_modulo modulo;
    t_canal canal;
    int identificador;

    int handshake_result = recibir_handshake(fd, MODULO_PLANIFICADOR, &modulo, &canal, &identificador);

    if (handshake_result != CONEXION_OK) {
        log_error(planificador_logger, "Error al recibir handshake del Core en el socket %d", fd);
        liberar_conexion(&fd);
        return NULL;
    }

    log_info(planificador_logger, LOG_CONEXION_RECIBIDA, modulo_to_string(modulo));

    while (1) {
        uint8_t op_code;
        int result = recibir_operacion(fd, &op_code);

        if (result == CONEXION_ERROR) {
            log_error(planificador_logger, "Error al recibir operación del Core con identificador: %d", identificador);
            break;
        }

        if (result == CONEXION_DESCONECTADO) {
            log_info(planificador_logger, "Core con identificador: %d se ha desconectado", identificador);
            break;
        }

        atender_mensaje(fd, op_code, identificador);
    }

    liberar_conexion(&fd);

    return NULL;
}

static void* escuchar_cores(void* arg) {
    (void) arg; // para evitar la advertencia de variable no utilizada

    while (1) {
        int fd_cliente = esperar_cliente(fd_servidor);

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