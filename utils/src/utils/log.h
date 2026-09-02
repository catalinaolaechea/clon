#ifndef SO_LOG_H_
#define SO_LOG_H_

#include <commons/log.h>

t_log* iniciar_logger(char* path_log, char* nombre_modulo, t_log_level nivel_inicial);
void so_log_set_level(t_log* logger, char* log_level_str);

#endif