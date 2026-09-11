#ifndef MENSAJE_PRUEBA_H
#define MENSAJE_PRUEBA_H

#include <stdbool.h>
#include <stdint.h>
#include <pthread.h>
#include <commons/log.h>

#include "protocolo.h"
#include "serializacion.h"

// MENSAJE_PRUEBA / MENSAJE_PRUEBA_ECO del Check 1. Contrato en docs/protocolo.md, seccion 8.
// Los 4 modulos arman y leen el payload con estas funciones y no a mano: son cinco campos en
// un orden fijo, y un campo leido fuera de orden no da error, da basura.

#define PRUEBA_RELLENO_CHICO 64
#define PRUEBA_RELLENO_GRANDE (8 * 1024)  // > 4 KB: obliga al recv a loopear, que es lo que se rompe en las VMs

// En el stream: uint8 origen, uint32 secuencia, uint32 tamanio_relleno, string texto, bytes
// del relleno. El relleno no lleva prefijo de longitud propio, su tamanio ya viaja arriba.
typedef struct {
    uint8_t  origen;           // t_modulo que lo emitio
    uint32_t secuencia;        // para seguir un mensaje puntual en los logs
    uint32_t tamanio_relleno;  // con > 4 KB se fuerza el loop de recv
    char*    texto;            // malloc
    void*    relleno;          // malloc de tamanio_relleno bytes, NULL si es 0
} t_mensaje_prueba;

t_mensaje_prueba* mensaje_prueba_crear(t_modulo origen, uint32_t secuencia, uint32_t tamanio_relleno, char* texto);  // copia el texto
t_paquete* mensaje_prueba_empaquetar(t_mensaje_prueba* mensaje, uint8_t op_code);  // MENSAJE_PRUEBA o MENSAJE_PRUEBA_ECO
t_mensaje_prueba* mensaje_prueba_leer(t_buffer* buffer);  // NULL si viene truncado, en vez de leer fuera del buffer
void mensaje_prueba_loguear(t_log* logger, char* rotulo, t_mensaje_prueba* mensaje);  // en DEBUG, para comparar ambos lados
bool mensaje_prueba_son_iguales(t_mensaje_prueba* enviado, t_mensaje_prueba* recibido);
int mensaje_prueba_responder_eco(int fd, t_mensaje_prueba* mensaje);  // devuelve el mensaje sin modificar
void mensaje_prueba_destruir(t_mensaje_prueba* mensaje);

// Lado emisor: manda, espera el eco, compara los cinco campos y loguea los dos lados. Devuelve false
// si algo no volvio identico. El mutex protege el socket cuando lo comparten varios hilos; va NULL si
// el fd lo usa uno solo, como en el Core.
bool mensaje_prueba_round_trip(int fd, pthread_mutex_t* mutex, t_log* logger, t_modulo origen,
                               t_modulo destino, uint32_t secuencia, uint32_t tamanio_relleno);

#endif
