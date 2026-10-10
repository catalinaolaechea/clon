#ifndef SERVIDORES_CORE_H
#define SERVIDORES_CORE_H

#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <commons/collections/list.h>

#define ERROR_SERVER -1
#define ERROR_CLIENT -1
#define THREAD_CREATED 0

typedef struct {
    int identificador;   // el que mando el Core en el handshake
    uint32_t numero_despacho; // Sube en cada despacho, identifica quantum vig
    int   fd_dispatch;     // -1 mientras no se conecto ese canal
    int   fd_interrupt;
    bool  libre;           // sin Job asignado
    uint32_t jid;          // Job que esta ejecutando: vale solo si !libre
} t_core_conectado;

extern t_list* cores_conectados;
extern pthread_mutex_t mutex_cores;

pthread_t iniciar_servidor_cores(void);

// Busca un Core libre con los dos canales conectados, lo marca ocupado con 'jid' y devuelve
// una copia (el original lo puede liberar desregistrar_core). false si no hay ninguno.
bool tomar_core_libre(uint32_t jid, t_core_conectado* core_tomado);

// Marca libre al Core y avisa al corto plazo, solo si tenia asignado ese 'jid'.
// false si el Core no existe o ejecutaba otro Job.
bool liberar_core(int identificador, uint32_t jid);

// Manda INTERRUPCION por el canal interrupt solo si el Core sigue con ese Job y ese despacho.
// false si ya no corresponde (el Job volvio o se despacho otro) o si fallo el envio.
bool interrumpir_core(int identificador, uint32_t jid, uint32_t numero_despacho);

#endif