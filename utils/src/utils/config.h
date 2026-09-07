#ifndef SO_CONFIG_H_
#define SO_CONFIG_H_

#include <commons/config.h>
t_config* iniciar_config(char* path_config);

char* so_config_get_string(t_config* config, char* nombre_modulo, char* clave);
int so_config_get_int(t_config* config, char* nombre_modulo, char*clave);
double so_config_get_double(t_config* config, char*nombre_modulo, char* clave);

#endif