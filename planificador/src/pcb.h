#ifndef PCB_H
#define PCB_H

#include <stdint.h>
#include <stdbool.h>
#include <commons/temporal.h>
#include <commons/collections/list.h>
#include <utils/contexto.h>

#define LOG_CAMBIO_ESTADO "## (%u) Pasa del estado %s al estado %s"

typedef enum {
    ESTADO_NEW,
    ESTADO_READY,
    ESTADO_EXEC,
    ESTADO_BLOCKED,
    ESTADO_EXIT
} t_estado;

typedef struct {
    t_temporal* tiempo_total; // Tiempo total desde que se creo el PCB
    t_temporal* tiempo_ready; // Tiempo acumulado en ready
    t_temporal* tiempo_new; // Tiempo acumulado en new
    uint32_t memoria_actual;
    uint32_t memoria_maxima;
    uint32_t cantidad_syscalls;
    uint32_t cantidad_page_faults;
    uint32_t transiciones_ready_exec;
} t_estadisticas;

typedef struct {
    //Identificacion
    uint32_t jid;
    t_estado estado;
    char* archivo_pseudocodigo;
    //Ejecucion
    t_contexto contexto;
    int core_asignado;
    //Planificacion
    double estimacion_rafaga;
    t_temporal* espera_en_ready; // Tiempo de espera en ready para calcular el tiempo de espera, es para HRRN (w + s) / s
    t_temporal* rafaga_actual;
    //I/O
    uint8_t syscall_pendiente;
    //Memoria
    t_list* asignaciones; // las de ALLOC todavia sin FREE: { dir_logica, tamanio }
    //Estadisticas
    t_estadisticas estadisticas;
} t_pcb;

char* estado_to_string(t_estado estado);
t_pcb* pcb_crear(uint32_t jid, char* archivo_pseudocodigo);
void pcb_destruir(t_pcb* pcb);

// Frena el cronometro del estado que deja, arranca el del que entra y cuenta READY->EXEC.
void pcb_actualizar_estadisticas(t_pcb* pcb, t_estado anterior, t_estado siguiente);

// Para HRRN: pone en 0 la espera al entrar a READY y la rafaga al entrar a EXEC, y las frena al salir.
// Al salir de EXEC recalcula la estimacion de la proxima rafaga con HRRN_ALFA.
void pcb_actualizar_planificacion(t_pcb* pcb, t_estado anterior, t_estado siguiente);

// Guarda la asignacion y suma 'tamanio' a la memoria actual del Job (y a la maxima si la supera).
void pcb_registrar_alloc(t_pcb* pcb, uint32_t dir_logica, uint32_t tamanio);

// Saca la asignacion que empieza en 'dir_logica' y resta su tamanio. false si no habia ninguna.
bool pcb_registrar_free(t_pcb* pcb, uint32_t dir_logica);

#endif