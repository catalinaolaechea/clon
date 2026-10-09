#include "interrupcion.h"
#include <pthread.h>
#include <sys/socket.h>

// El flag lo escribe el hilo de interrupciones y lo lee el hilo principal (el ciclo
// de instruccion): el mutex evita que se pisen.
static pthread_mutex_t mutex_interrupcion = PTHREAD_MUTEX_INITIALIZER;
static bool interrupcion_pendiente = false;

static pthread_t hilo_interrupt;
static bool hilo_iniciado = false;

// Las commons no son thread-safe: este hilo NO usa core_logger. Solo avisa por el flag;
// el log "## Interrupción recibida" lo escribe el hilo principal al atenderla.
static void* escuchar_interrupciones(void* arg) {
    (void) arg;

    while (1) {
        uint8_t op_code;
        if (recibir_operacion(fd_interrupt, &op_code) != CONEXION_OK) break;   // cerrada o error

        // Siempre se lee el payload completo, para no desalinear el socket.
        t_buffer* buffer;
        if (recibir_buffer(fd_interrupt, &buffer) != CONEXION_OK) break;
        eliminar_buffer(buffer);

        if (op_code == INTERRUPCION) {
            pthread_mutex_lock(&mutex_interrupcion);
            interrupcion_pendiente = true;
            pthread_mutex_unlock(&mutex_interrupcion);
        }
        // Cualquier otro op_code se ignora en este canal.
    }
    return NULL;
}

void iniciar_hilo_interrupt(void) {
    if (pthread_create(&hilo_interrupt, NULL, escuchar_interrupciones, NULL) != 0) {
        log_error(core_logger, "Core %d: no se pudo crear el hilo de interrupciones", identificador);
        exit(EXIT_FAILURE);
    }
    hilo_iniciado = true;
}

void detener_hilo_interrupt(void) {
    if (!hilo_iniciado) return;
    shutdown(fd_interrupt, SHUT_RDWR);   
    pthread_join(hilo_interrupt, NULL);
    hilo_iniciado = false;
}

bool hay_interrupcion(void) {
    pthread_mutex_lock(&mutex_interrupcion);
    bool hay = interrupcion_pendiente;
    interrupcion_pendiente = false;
    pthread_mutex_unlock(&mutex_interrupcion);
    return hay;
}

void limpiar_interrupcion(void) {
    pthread_mutex_lock(&mutex_interrupcion);
    interrupcion_pendiente = false;
    pthread_mutex_unlock(&mutex_interrupcion);
}