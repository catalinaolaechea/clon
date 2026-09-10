#ifndef HANDSHAKE_H
#define HANDSHAKE_H

#include "protocolo.h"
#include "serializacion.h"

int enviar_handshake(int fd, t_modulo modulo, t_canal canal, char* identificador);

int recibir_handshake(int fd, t_modulo servidor, t_modulo* cliente, t_canal* canal, char** identificador);

#endif
