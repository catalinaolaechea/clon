#ifndef DEVOLUCION_H
#define DEVOLUCION_H

#include <utils/serializacion.h>

#define LOG_DESALOJO_QUANTUM "## (%u) - Desalojado por fin de quantum"

// DEVOLUCION_JOB = { uint32 jid, contexto, uint8 motivo } por el canal dispatch del Core.
// Guarda el contexto en la PCB, libera el Core y resuelve el Job segun el motivo.
void recibir_devolucion(int identificador_core, t_buffer* buffer);

// DEVOLUCION_PAGE_FAULT = { uint32 jid, contexto, uint32 nro_pagina } por el canal dispatch del Core.
// Guarda el contexto, libera el Core, pasa el Job a BLOCKED y encola la carga de la pagina.
void recibir_devolucion_page_fault(int identificador_core, t_buffer* buffer);

// El Core se desconecto con este Job en EXEC: pide el deslockeo de sus paginas y lo pasa a READY.
// No hace nada si el Job ya no esta en EXEC con ese Core.
void recuperar_job_de_core_caido(uint32_t jid, int identificador_core);

#endif
