#ifndef SYSCALLS_H_
#define SYSCALLS_H_

#include <stdint.h>
#include <stdbool.h>
#include <utils/contexto.h>
#include "instrucciones.h"

bool solicitar_init_job(uint32_t jid, char* archivo);

bool ejecutar_alloc(uint32_t jid, const t_instruccion* instruccion, t_contexto* contexto);
bool ejecutar_free(uint32_t jid, const t_instruccion* instruccion, t_contexto* contexto);

#endif
