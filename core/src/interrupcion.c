#include "interrupcion.h"
#include <pthread.h>
#include <sys/socket.h>

// El flag lo escribe el hilo de interrupciones y lo lee el hilo principal (el ciclo
// de instruccion): el mutex evita que se pisen.
static pthread_mutex_t mutex_interrupcion = PTHREAD_MUTEX_INITIALIZER;
static bool interrupcion_pendiente = false;
static uint32_t jid_interrumpido = 0;   // JID que trajo la ultima INTERRUPCION

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

        // INTERRUPCION = { uint32 jid }. Si el payload viniera corto no se lee: seria leer fuera del buffer.
        if (op_code == INTERRUPCION && buffer->size >= sizeof(uint32_t)) {
            uint32_t jid = buffer_read_uint32(buffer);
            pthread_mutex_lock(&mutex_interrupcion);
            jid_interrumpido = jid;
            interrupcion_pendiente = true;
            pthread_mutex_unlock(&mutex_interrupcion);
        }
        eliminar_buffer(buffer);
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

t_estado_interrupcion consumir_interrupcion(uint32_t jid_en_curso, uint32_t* jid_recibido) {
    pthread_mutex_lock(&mutex_interrupcion);
    bool hay = interrupcion_pendiente;
    uint32_t jid = jid_interrumpido;
    interrupcion_pendiente = false;
    pthread_mutex_unlock(&mutex_interrupcion);

    if (!hay) return INTERRUPCION_NINGUNA;
    if (jid_recibido != NULL) *jid_recibido = jid;
    return (jid == jid_en_curso) ? INTERRUPCION_PARA_ESTE_JOB : INTERRUPCION_DE_OTRO_JOB;
}