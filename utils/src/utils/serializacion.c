#include "serializacion.h"
#include "sys/socket.h"
#include "errno.h"

t_paquete* crear_paquete(uint8_t op_code) {
    t_paquete* paquete = malloc(sizeof(t_paquete));
    paquete->op_code = op_code;
    paquete->buffer = malloc(sizeof(t_buffer));
    paquete->buffer->size = 0;
    paquete->buffer->offset = 0;
    paquete->buffer->stream = NULL;
    return paquete;
}

void eliminar_paquete(t_paquete* paquete) {
    free(paquete->buffer->stream);
    free(paquete->buffer);
    free(paquete);
}

void buffer_add(t_buffer* buffer, void* data, uint32_t size) {
    buffer->stream = realloc(buffer->stream, buffer->size + size);
    memcpy(buffer->stream + buffer->size, data, size);
    buffer->size += size;
}

void buffer_add_uint32(t_buffer* buffer, uint32_t valor) {
    buffer_add(buffer, &valor, sizeof(uint32_t));
}

void buffer_add_uint8(t_buffer* buffer, uint8_t valor) {
    buffer_add(buffer, &valor, sizeof(uint8_t));
}

void buffer_add_string(t_buffer* buffer, char* str) {
    uint32_t longitud = strlen(str) + 1;   // el '\0' va incluido en la longitud
    buffer_add_uint32(buffer, longitud);
    buffer_add(buffer, str, longitud);
}

void buffer_read(t_buffer* buffer, void* dest, uint32_t size) {
    memcpy(dest, buffer->stream + buffer->offset, size);
    buffer->offset += size;
}

uint32_t buffer_read_uint32(t_buffer* buffer) {
    uint32_t valor;
    buffer_read(buffer, &valor, sizeof(uint32_t));
    return valor;
}

uint8_t buffer_read_uint8(t_buffer* buffer) {
    uint8_t valor;
    buffer_read(buffer, &valor, sizeof(uint8_t));
    return valor;
}

char* buffer_read_string(t_buffer* buffer) {
    uint32_t longitud = buffer_read_uint32(buffer);
    char* str = malloc(longitud);
    buffer_read(buffer, str, longitud);
    return str;
}

// No van en el header porque son funciones privadas de este módulo
static int enviar_todo(int fd, void* datos, uint32_t size) {
    int movidos = 0;
    while (movidos < size) {
        int enviados = send(fd, datos + movidos, size - movidos, 0);

        if (enviados == -1) {
            fprintf(stderr, "enviar_todo: send: %s\n", strerror(errno));
            return CONEXION_ERROR;
        }

        movidos += enviados;
    }
}

static int recibir_todo(int fd, void* datos, uint32_t size) {
    int movidos = 0;
    while (movidos < size) {
        int recibidos = recv(fd, datos + movidos, size - movidos, 0);

        if (recibidos == -1) {
            fprintf(stderr, "recibir_todo: recv: %s\n", strerror(errno));
            return CONEXION_ERROR;
        }

        if (recibidos == 0) {
            fprintf(stderr, "recibir_todo: recv: conexión cerrada por el otro extremo\n");
            return CONEXION_DESCONECTADO;
        }
        
        movidos += recibidos;
    }
}