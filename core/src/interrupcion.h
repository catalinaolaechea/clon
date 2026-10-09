#ifndef INTERRUPCION_H_
#define INTERRUPCION_H_

#include <stdbool.h>
#include "inicializador.h"
#include "conexiones.h"

//Core atiende una interrupcion
#define LOG_INTERRUPCION "## Interrupción recibida"

// COR-08: arranca el hilo que escucha el canal de interrupciones 
void iniciar_hilo_interrupt(void);

// Corta la espera del hilo y lo espera. Llamar antes de liberar fd_interrupt.
void detener_hilo_interrupt(void);

// CHECK INTERRUPT
bool hay_interrupcion(void);

// Descarta una interrupcion pendiente. Se llama al empezar un Job, para que una
// interrupcion que llego "tarde" para el Job anterior no corte al nuevo.
void limpiar_interrupcion(void);

#endif