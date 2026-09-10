#ifndef SERIALIZACION_H_
#define SERIALIZACION_H_

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <commons/collections/list.h>

#define TAM_MAXIMO_PAYLOAD (16 * 1024 * 1024) 
// 16 MB numero razonable para un payload maximo, se puede cambiar si se desea
#define CONEXION_OK 0
#define CONEXION_ERROR -1
#define CONEXION_DESCONECTADO -2

typedef struct {
    uint32_t size;      // bytes escritos
    uint32_t offset;    // cursor de lectura
    void* stream;
} t_buffer;

typedef struct {
    uint8_t op_code;
    t_buffer* buffer;
} t_paquete;

t_paquete* crear_paquete(uint8_t op_code);
void eliminar_paquete(t_paquete* paquete);
int enviar_paquete(int fd, t_paquete* paquete);
int recibir_operacion(int fd, uint8_t* op_code);
void eliminar_buffer(t_buffer* buffer);
int recibir_buffer(int fd, t_buffer** buffer);
t_list* recibir_paquete(int socket_cliente);

void buffer_add_uint32(t_buffer* buffer, uint32_t valor);
void buffer_add_uint8 (t_buffer* buffer, uint8_t valor);
void buffer_add_string(t_buffer* buffer, char* str);
void buffer_add       (t_buffer* buffer, void* data, uint32_t size);

uint32_t buffer_read_uint32(t_buffer* buffer);
uint8_t  buffer_read_uint8 (t_buffer* buffer);
int buffer_read_int(t_buffer* buffer);
char*    buffer_read_string(t_buffer* buffer);   // malloc: libera el llamador
void     buffer_read       (t_buffer* buffer, void* dest, uint32_t size);


#endif
