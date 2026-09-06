#include "serializacion.h"
#include <sys/socket.h>
#include <errno.h>
#include <unistd.h>

// No van en el header porque son funciones privadas de este módulo
static int enviar_todo(int fd, void* datos, uint32_t size) {
    uint32_t movidos = 0;
    while (movidos < size) {
        ssize_t enviados = send(fd, datos + movidos, size - movidos, MSG_NOSIGNAL);

        if (enviados == -1) {
            if (errno == EINTR) continue;  // si la llamada fue interrumpida, reintentar
            fprintf(stderr, "enviar_todo: send: %s\n", strerror(errno));
            return CONEXION_ERROR;
        }

        movidos += enviados;
    }

    return CONEXION_OK;
}

static int recibir_todo(int fd, void* datos, uint32_t size) {
    uint32_t movidos = 0;
    while (movidos < size) {
        ssize_t recibidos = recv(fd, datos + movidos, size - movidos, MSG_WAITALL);

        if (recibidos == -1) {
            if (errno == EINTR) continue;  // si la llamada fue interrumpida, reintentar
            fprintf(stderr, "recibir_todo: recv: %s\n", strerror(errno));
            return CONEXION_ERROR;
        }

        if (recibidos == 0) {
            return CONEXION_DESCONECTADO;
        }
        
        movidos += recibidos;
    }

    return CONEXION_OK;
}

void eliminar_buffer(t_buffer* buffer) {
    free(buffer->stream);
    free(buffer);
}

static void* serializar_paquete(t_paquete* paquete, uint32_t bytes) {
    void* magic = malloc(bytes);
    uint32_t desplazamiento = 0;

    memcpy(magic + desplazamiento, &(paquete->op_code), sizeof(uint8_t));
    desplazamiento += sizeof(uint8_t);
    memcpy(magic + desplazamiento, &(paquete->buffer->size), sizeof(uint32_t));
    desplazamiento += sizeof(uint32_t);
    if (paquete->buffer->size > 0) {
        memcpy(magic + desplazamiento, paquete->buffer->stream, paquete->buffer->size);
        desplazamiento += paquete->buffer->size;
    }

    return magic;
}

int enviar_paquete(int fd, t_paquete* paquete) {
    if (paquete == NULL || paquete->buffer == NULL) {
        fprintf(stderr, "enviar_paquete: paquete inválido\n");
        return CONEXION_ERROR;
    }

    uint32_t bytes = sizeof(uint8_t) + sizeof(uint32_t) + paquete->buffer->size;

    void* a_enviar = serializar_paquete(paquete, bytes);

    int resultado = enviar_todo(fd, a_enviar, bytes);

    free(a_enviar);

    return resultado;
}

int recibir_buffer(int fd, t_buffer** buffer) {
    *buffer = NULL; // inicializar el puntero a NULL en caso de error
    uint32_t size; // tamaño del buffer a recibir
    
    int resultado = recibir_todo(fd, &size, sizeof(uint32_t));
    if (resultado != CONEXION_OK) return resultado;
    
    if (size > TAM_MAXIMO_PAYLOAD) {
        fprintf(stderr, "recibir_buffer: tamaño de buffer demasiado grande: %u bytes\n", size);
        return CONEXION_ERROR;
    }

    *buffer = malloc(sizeof(t_buffer));
    (*buffer)->size = size;
    (*buffer)->offset = 0;
    (*buffer)->stream = NULL;

    // si el tamaño es 0, no hay datos que recibir, así que podemos devolver CONEXION_OK directamente
    if (size > 0) {
        (*buffer)->stream = malloc(size);

        resultado = recibir_todo(fd, (*buffer)->stream, size);
        if (resultado != CONEXION_OK) {
            free((*buffer)->stream);
            free(*buffer);
            *buffer = NULL;
            return resultado;
        }
    }

    return CONEXION_OK;
}

int recibir_operacion(int fd, uint8_t* op_code) {
    int resultado = recibir_todo(fd, op_code, sizeof(uint8_t));
    return resultado;
}

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
    eliminar_buffer(paquete->buffer);
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
