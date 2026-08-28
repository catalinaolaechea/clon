#ifndef UTILS_PROTOCOLO_H
#define UTILS_PROTOCOLO_H

#include <stdint.h>

// Contrato de comunicacion entre los 4 modulos. Documentado en docs/protocolo.md:
// ahi estan el formato de cada payload, las reglas de serializacion y el porque.

// Todo mensaje viaja como [op_code (1B)][payload_size (4B)][payload].
#define PROTOCOLO_HEADER_SIZE 5

// Registros del contexto de ejecucion: 14 x uint32_t.
#define CONTEXTO_CANT_REGISTROS 14
#define CONTEXTO_SIZE (CONTEXTO_CANT_REGISTROS * sizeof(uint32_t))

// El 0 es invalido a proposito: una variable sin inicializar no se hace pasar
// por un modulo valido.
typedef enum {
    MODULO_INVALIDO     = 0,
    MODULO_PLANIFICADOR = 1,
    MODULO_CORE         = 2,
    MODULO_PLACA        = 3,
    MODULO_STORAGE      = 4
} t_modulo;

// El Core abre dos conexiones al Planificador y las distingue por este campo.
typedef enum {
    CANAL_UNICO     = 0,
    CANAL_DISPATCH  = 1,
    CANAL_INTERRUPT = 2
} t_canal;

// Rangos reservados por par de modulos, para poder agregar mensajes en paralelo:
typedef enum {
    // --- 0-9: handshake y control ---
    HANDSHAKE           = 1,
    HANDSHAKE_OK        = 2,
    HANDSHAKE_ERROR     = 3,
    MENSAJE_PRUEBA      = 4,
    MENSAJE_PRUEBA_ECO  = 5,

    // --- 10-39: Planificador <-> Core (Check 2) ---
    DISPATCH_JOB        = 10,
    DEVOLUCION_JOB      = 11,
    INTERRUPCION        = 12,

    // --- 40-69: Core <-> Placa (Check 3) ---
    FETCH_INSTRUCCION     = 40,
    RESPUESTA_INSTRUCCION = 41,
    OBTENER_MARCO         = 42,
    RESPUESTA_MARCO       = 43,
    PAGE_FAULT            = 44,
    DESLOCKEAR_PAGINAS    = 49,

    // --- 70-99: Planificador <-> Placa (Check 3) ---
    CREAR_JOB           = 70,
    FINALIZAR_JOB       = 71,
    CARGAR_PAGINA       = 76
} t_op_code;

// Motivo por el que el Core devuelve un Job al Planificador.
typedef enum {
    MOTIVO_FIN_QUANTUM       = 0,
    MOTIVO_PAGE_FAULT        = 1,
    MOTIVO_SYSCALL_BLOQUEANTE = 2,
    MOTIVO_EXIT              = 3
} t_motivo_devolucion;

// Nombre del modulo para los logs obligatorios de conexion.
char* modulo_to_string(t_modulo modulo);

#endif
