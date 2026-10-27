#ifndef MMU_H_
#define MMU_H_

#include <stdbool.h>
#include "inicializador.h"
#include "conexiones.h"

// Pide a la Placa el tamanio de pagina (OBTENER_TAM_PAGINA)
bool obtener_tam_pagina(void);

// Tamanio de pagina de la Placa en bytes. 0 si todavia no se pidio.
uint32_t tam_pagina(void);

#endif