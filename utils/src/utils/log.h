#ifndef SO_LOG_H_
#define SO_LOG_H_

#include <commons/log.h>
#include <string.h>

#define LOG_LEVEL_INVALID ((t_log_level) -1)  // mismo tipo que el enum: sin esto -Wsign-compare

t_log* iniciar_logger(char* path_log, char* nombre_modulo, t_log_level nivel_inicial);

#endif