#ifndef OFFLOAD_H_
#define OFFLOAD_H_

#include "inicializador.h"
#include <commons/bitarray.h>
#include <fcntl.h>   // open() y O_RDWR, O_CREAT, O_TRUNC
#include <unistd.h>  // ftruncate() y close()
#include <errno.h>   // errno
#include <string.h>  // strerror()

//offload
extern int archivo_offload;
extern t_bitarray* slots_offload;
extern uint32_t cant_slots;

extern pthread_mutex_t mutex_offload;

bool inicializador_offload();

#endif
