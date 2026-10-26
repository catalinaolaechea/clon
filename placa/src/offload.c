#include "offload.h"

//offload
int archivo_offload;
t_bitarray* slots_offload;

pthread_mutex_t mutex_offload = PTHREAD_MUTEX_INITIALIZER;
