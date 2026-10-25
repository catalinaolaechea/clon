#ifndef INTERRUPCION_H_
#define INTERRUPCION_H_

#include <stdbool.h>
#include "inicializador.h"
#include "conexiones.h"

//Core atiende una interrupcion
#define LOG_INTERRUPCION "## Interrupción recibida"
#define LOG_INTERRUPCION_DESCARTADA "Core %d: interrupcion descartada, era para el JID %u y se ejecuta el JID %u"

// COR-08: arranca el hilo que escucha el canal de interrupciones 
void iniciar_hilo_interrupt(void);

// Corta la espera del hilo y lo espera. Llamar antes de liberar fd_interrupt.
void detener_hilo_interrupt(void);

// Resultado de mirar si hay una interrupcion pendiente.
typedef enum {
    INTERRUPCION_NINGUNA,      // no llego nada desde el ultimo chequeo
    INTERRUPCION_PARA_ESTE_JOB, // llego una interrupcion para el Job que se esta ejecutando
    INTERRUPCION_DE_OTRO_JOB    // llego tarde: era para un Job que ya no esta en este Core
} t_estado_interrupcion;

// CHECK INTERRUPT: mira (sin bloquearse) si llego una interrupcion y la consume.
// 'jid_en_curso' es el Job que se esta ejecutando. Si la interrupcion era de otro Job
// se descarta, y si 'jid_recibido' no es NULL se devuelve ahi el JID que traia.
t_estado_interrupcion consumir_interrupcion(uint32_t jid_en_curso, uint32_t* jid_recibido);

#endif