#ifndef HANDSHAKE_H
#define HANDSHAKE_H

#include "protocolo.h"
#include "serializacion.h"

int enviar_handshake(int fd, t_modulo yo, t_canal canal, char* identificador);

#endif