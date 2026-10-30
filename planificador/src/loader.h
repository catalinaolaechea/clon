#ifndef LOADER_H
#define LOADER_H

#include <stdbool.h>
#include "servicios.h"

// LOAD_BATCH: lee los primeros 'tamanio' bytes del archivo de PATH_DATOS y los escribe en la memoria del Job.
// false si el archivo no existe, es mas corto o falla la escritura: el Job se finaliza.
bool operar_loader(t_pedido_servicio* pedido);

#endif
