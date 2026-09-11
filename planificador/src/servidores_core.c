#include "servidores_core.h"
#include "inicializador.h"

t_list* cores_conectados;
pthread_mutex_t mutex_cores = PTHREAD_MUTEX_INITIALIZER;

static int fd_servidor;

static void registrar_core(int identificador, t_canal canal, int fd) {
    pthread_mutex_lock(&mutex_cores);

    t_core_conectado* core = NULL;

    for (int i = 0; i < list_size(cores_conectados); i++) {
        t_core_conectado* actual = list_get(cores_conectados, i);
        if (actual->identificador == identificador) {
            core = actual;
            break;
        }
    }

    if (core == NULL) {
        core = malloc(sizeof(t_core_conectado));
        core->identificador = identificador;
        core->fd_dispatch = -1;
        core->fd_interrupt = -1;
        list_add(cores_conectados, core);
    }

    if (canal == CANAL_DISPATCH) {
        core->fd_dispatch = fd;
    } else {
        core->fd_interrupt = fd;
    }

    int size_cores = list_size(cores_conectados);
    pthread_mutex_unlock(&mutex_cores);
    log_info(planificador_logger, "Cores conectados post registro: %d", size_cores);
}

static void desregistrar_core(int identificador, t_canal canal) {
    pthread_mutex_lock(&mutex_cores);

    for (int i = 0; i < list_size(cores_conectados); i++) {
        t_core_conectado* core = list_get(cores_conectados, i);
        if (core->identificador == identificador) {
            if (canal == CANAL_DISPATCH) {
                core->fd_dispatch = -1;
            } else {
                core->fd_interrupt = -1;
            }

            if (core->fd_dispatch == -1 && core->fd_interrupt == -1) {
                list_remove(cores_conectados, i);
                //free(core->identificador);
                free(core);
            }
            break;
        }
    }

    int size_cores = list_size(cores_conectados);
    pthread_mutex_unlock(&mutex_cores);
    log_info(planificador_logger, "Cores conectados post desregistro: %d", size_cores);
}

static void atender_mensaje(int fd, uint8_t op_code, int identificador, t_buffer* buffer) {
    switch (op_code) {
        case MENSAJE_PRUEBA: {
            log_info(planificador_logger, "Recibido MENSAJE_PRUEBA del Core con identificador: %d", identificador);

            t_mensaje_prueba* mensaje = mensaje_prueba_leer(buffer);

            if (mensaje == NULL) {
                log_error(planificador_logger, "MENSAJE_PRUEBA mal formado del Core %d", identificador);
                break;
            }

            // el rotulo lleva el identificador porque con varios Cores los logs se intercalan
            char* rotulo = string_from_format("MENSAJE_PRUEBA recibido del Core %d", identificador);
            mensaje_prueba_loguear(planificador_logger, rotulo, mensaje);
            free(rotulo);

            if (mensaje_prueba_responder_eco(fd, mensaje) != CONEXION_OK) {  // los campos vuelven sin modificar
                log_error(planificador_logger, "No se pudo responder el eco al Core %d", identificador);
            }

            mensaje_prueba_destruir(mensaje);
            break;
        }
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

    registrar_core(identificador, canal, fd);

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

        t_buffer* buffer;
        result = recibir_buffer(fd, &buffer);

        if (result != CONEXION_OK) {
            log_error(planificador_logger, "Error al recibir el payload del Core con identificador: %d", identificador);
            break;
        }

        atender_mensaje(fd, op_code, identificador, buffer);
        eliminar_buffer(buffer);
    }

    // TODO: Ante la desconexión de un core, el job que estaba ejecutando debe volver a READY
    // y debe solicitar a la placa el deslockeo de sus paginas
    desregistrar_core(identificador, canal);
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