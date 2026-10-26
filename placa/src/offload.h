#ifndef OFFLOAD_H_
#define OFFLOAD_H_

#include "inicializador.h"
#include <commons/bitarray.h>

//offload
extern int archivo_offload;
extern t_bitarray* slots_offload;

extern pthread_mutex_t mutex_offload;

#endif
