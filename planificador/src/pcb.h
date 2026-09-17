#ifndef PCB_H
#define PCB_H

#include <stdint.h>
#include <commons/temporal.h>
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
    //Estadisticas
    t_estadisticas estadisticas;
} t_pcb;

char* estado_to_string(t_estado estado);
t_pcb* pcb_crear(uint32_t jid, char* archivo_pseudocodigo);
void pcb_destruir(t_pcb* pcb);

#endif