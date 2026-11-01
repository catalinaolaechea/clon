#ifndef SYSCALLS_H
#define SYSCALLS_H
#include <utils/serializacion.h>

#define LOG_SYSCALL "## (%u) - Solicitó syscall: %s"

void atender_init_job(int fd, int identificador_core, t_buffer* buffer);

void atender_syscall_servicio(int identificador_core, uint8_t op_code, t_buffer* buffer);

void atender_alloc(int fd, int identificador_core, t_buffer* buffer);

void atender_free(int fd, int identificador_core, t_buffer* buffer);

#endif