#ifndef DISPATCH_H_
#define DISPATCH_H_

#include "inicializador.h"
#include "conexiones.h"
#include "ciclo.h"

// Log al recibir un Job del Planificador.
#define LOG_JOB_RECIBIDO "Core %d: DISPATCH_JOB recibido - JID: %u - PC: %u"
#define LOG_JOB_DEVUELTO "Core %d: DEVOLUCION_JOB enviada - JID: %u - PC: %u - Motivo: %s"

// Loop principal del Core: bloqueado en el canal de dispatch esperando que el
// Planificador le asigne un Job. Por cada DISPATCH_JOB ejecuta el Job y vuelve
// a esperar. Retorna solo si se cierra la conexion o hay un error de red.
void esperar_dispatch(void);

// COR-07: corre el ciclo de instruccion sobre 'contexto' hasta que el Job tenga
// que dejar el Core. Devuelve el motivo de la devolucion (hoy solo MOTIVO_EXIT).
t_motivo_devolucion ejecutar_job(uint32_t jid, t_contexto* contexto);

// COR-07: DEVOLUCION_JOB = { uint32 jid, contexto, uint8 motivo } por el canal dispatch.
int devolver_job(uint32_t jid, t_contexto* contexto, t_motivo_devolucion motivo);

#endif