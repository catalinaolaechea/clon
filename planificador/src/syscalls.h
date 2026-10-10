#ifndef SYSCALLS_H
#define SYSCALLS_H
#include <utils/serializacion.h>

#define LOG_SYSCALL "## (%u) - Solicitó syscall: %s"

// SYSCALL_INIT_JOB = { uint32 jid, string archivo }: crea el Job en NEW y responde SYSCALL_OK al Core.
void atender_init_job(int fd, int identificador_core, t_buffer* buffer);

#endif