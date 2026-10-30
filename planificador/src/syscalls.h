#ifndef SYSCALLS_H
#define SYSCALLS_H
#include <utils/serializacion.h>

#define LOG_SYSCALL "## (%u) - Solicitó syscall: %s"

// SYSCALL_INIT_JOB = { uint32 jid, string archivo }: crea el Job en NEW y responde SYSCALL_OK al Core.
void atender_init_job(int fd, int identificador_core, t_buffer* buffer);

// SYSCALL_LOAD_BATCH / LABEL / REPORT = { jid, contexto, [string archivo], uint32 tamanio, lista_direcciones }.
// Devuelven el Job: libera el Core, lo pasa a BLOCKED y lo encola en su servicio. No se le responde al Core.
void atender_syscall_servicio(int identificador_core, uint8_t op_code, t_buffer* buffer);

#endif