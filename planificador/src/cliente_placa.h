#ifndef CLIENTE_PLACA_H
#define CLIENTE_PLACA_H
#include <stdint.h>
#include <stdbool.h>
#include <utils/direcciones.h>

#define LOG_DESLOCKEO "## (%u) - Deslockeo de paginas"
#define LOG_ACCESO_MEMORIA "## (%u) - %s - Dirección Física: %u - Tamaño: %u"

bool placa_crear_job(uint32_t jid, char* archivo_pseudocodigo);
bool placa_finalizar_job(uint32_t jid);

// Pide a la Placa que deslockee todas las paginas del Job y espera DESLOCKEO_OK.
bool placa_deslockear_paginas(uint32_t jid);

// Pide a la Placa que cargue esa pagina del Job (atencion del Page Fault) y espera CARGAR_PAGINA_OK.
bool placa_cargar_pagina(uint32_t jid, uint32_t nro_pagina);

// Lee de la Placa un pedazo de la memoria del Job (no cruza de marco) y lo copia en 'destino'.
// false si la Placa responde MEMORIA_ERROR o un bloque de otro tamaño.
bool placa_leer_memoria(uint32_t jid, t_direccion_fisica* pedazo, void* destino);

#endif