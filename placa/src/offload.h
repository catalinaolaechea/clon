#ifndef OFFLOAD_H_
#define OFFLOAD_H_

#include "inicializador.h"
#include <commons/bitarray.h>

//offload
extern int archivo_offload;
extern t_bitarray* slots_offload;
extern uint32_t cant_slots;

extern pthread_mutex_t mutex_offload;

bool inicializador_offload();
bool escribir_pagina_offload(uint32_t slot, void* origen);
bool leer_pagina_offload(uint32_t slot, void* destino);

// Reserva "cantidad" slots libres y deja sus números en "slots". Todo o nada.
// Llamar con mutex_memoria tomado.
bool reservar_slots(uint32_t cantidad, uint32_t* slots);

// Marca el slot como libre. Llamar con mutex_memoria tomado.
void liberar_slot(uint32_t slot);

#endif
