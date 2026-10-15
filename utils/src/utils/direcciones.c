#include "direcciones.h"

t_direccion_fisica* direccion_fisica_crear(uint32_t direccion, uint32_t tamanio) {
    t_direccion_fisica* direccion_fisica = malloc(sizeof(t_direccion_fisica));
    direccion_fisica->direccion = direccion;
    direccion_fisica->tamanio   = tamanio;
    return direccion_fisica;
}

void direcciones_destruir(t_list* direcciones) {
    list_destroy_and_destroy_elements(direcciones, free);
}

// uint32 cantidad + por cada una: uint32 direccion, uint32 tamanio (protocolo.md §10)
void buffer_add_direcciones(t_buffer* buffer, t_list* direcciones) {
    buffer_add_uint32(buffer, list_size(direcciones));

    for (int i = 0; i < list_size(direcciones); i++) {
        t_direccion_fisica* direccion_fisica = list_get(direcciones, i);
        buffer_add_uint32(buffer, direccion_fisica->direccion);
        buffer_add_uint32(buffer, direccion_fisica->tamanio);
    }
}

t_list* buffer_read_direcciones(t_buffer* buffer) {
    t_list* direcciones = list_create();
    uint32_t cantidad = buffer_read_uint32(buffer);

    for (uint32_t i = 0; i < cantidad; i++) {
        uint32_t direccion = buffer_read_uint32(buffer);
        uint32_t tamanio   = buffer_read_uint32(buffer);
        list_add(direcciones, direccion_fisica_crear(direccion, tamanio));
    }

    return direcciones;
}
