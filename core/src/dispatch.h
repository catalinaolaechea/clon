#ifndef DISPATCH_H_
#define DISPATCH_H_

#include "inicializador.h"
#include "conexiones.h"
#include "ciclo.h"
#include "interrupcion.h"
#include "servicios.h"

#define LOG_JOB_RECIBIDO "Core %d: DISPATCH_JOB recibido - JID: %u - PC: %u"
#define LOG_JOB_DEVUELTO "Core %d: DEVOLUCION_JOB enviada - JID: %u - PC: %u - Motivo: %s"
#define LOG_SYSCALL_ENVIADA "Core %d: %s enviada al Planificador (Job devuelto) - JID: %u - PC: %u"

void esperar_dispatch(void);

// Corre el ciclo hasta que el Job tenga que dejar el Core y devuelve el motivo.
// MOTIVO_PAGE_FAULT: *pagina_faltante.
// MOTIVO_SYSCALL_BLOQUEANTE: 'servicio' trae el pedido (LOAD_BATCH / LABEL / REPORT) ya traducido.
t_motivo_devolucion ejecutar_job(uint32_t jid, t_contexto* contexto, uint32_t* pagina_faltante, t_syscall_servicio* servicio);

int devolver_job(uint32_t jid, t_contexto* contexto, t_motivo_devolucion motivo);

int devolver_page_fault(uint32_t jid, t_contexto* contexto, uint32_t nro_pagina);

// Syscall de servicio: es UN solo mensaje que devuelve el Job y trae el contexto (con el PC ya en la
// siguiente instruccion), los parametros y las direcciones fisicas. No se manda DEVOLUCION_JOB aparte.
// Las paginas siguen lockeadas: las libera el Planificador.
int devolver_por_syscall(uint32_t jid, t_contexto* contexto, const t_syscall_servicio* servicio);

#endif