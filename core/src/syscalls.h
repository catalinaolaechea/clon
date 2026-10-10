#ifndef SYSCALLS_H_
#define SYSCALLS_H_

#include <stdint.h>
#include <stdbool.h>

bool solicitar_init_job(uint32_t jid, char* archivo);

#endif
