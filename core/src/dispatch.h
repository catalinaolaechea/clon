#ifndef DISPATCH_H_
#define DISPATCH_H_

#include "inicializador.h"
#include "conexiones.h"
#include "ciclo.h"
#include "interrupcion.h"

#define LOG_JOB_RECIBIDO "Core %d: DISPATCH_JOB recibido - JID: %u - PC: %u"
#define LOG_JOB_DEVUELTO "Core %d: DEVOLUCION_JOB enviada - JID: %u - PC: %u - Motivo: %s"

void esperar_dispatch(void);

t_motivo_devolucion ejecutar_job(uint32_t jid, t_contexto* contexto, uint32_t* pagina_faltante);

int devolver_job(uint32_t jid, t_contexto* contexto, t_motivo_devolucion motivo);

int devolver_page_fault(uint32_t jid, t_contexto* contexto, uint32_t nro_pagina);

#endif