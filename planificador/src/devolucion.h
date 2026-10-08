#ifndef DEVOLUCION_H
#define DEVOLUCION_H

#include <utils/serializacion.h>

#define LOG_DESALOJO_QUANTUM "## (%u) - Desalojado por fin de quantum"

// DEVOLUCION_JOB = { uint32 jid, contexto, uint8 motivo } por el canal dispatch del Core.
// Guarda el contexto en la PCB, libera el Core y resuelve el Job segun el motivo.
void recibir_devolucion(int identificador_core, t_buffer* buffer);

#endif
