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

// Log de conexion: "## Módulo: <nombre_modulo>"
#define LOG_CONEXION_RECIBIDA "## Módulo: %s"
#define LOG_CONEXION_ESTABLECIDA "## Conectado a %s exitosamente"

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
    DEVOLUCION_PAGE_FAULT = 13,
    SYSCALL_INIT_JOB    = 20,  
    SYSCALL_ALLOC       = 21,
    SYSCALL_FREE        = 22,
    SYSCALL_LOAD_BATCH  = 23,
    SYSCALL_LABEL       = 24,
    SYSCALL_REPORT      = 25,
    SYSCALL_OK          = 30,  
    RESPUESTA_ALLOC     = 31,
    SYSCALL_ERROR       = 32,

    // --- 40-69: Core <-> Placa (Check 3) ---
    FETCH_INSTRUCCION     = 40,
    RESPUESTA_INSTRUCCION = 41,
    OBTENER_MARCO         = 42,
    RESPUESTA_MARCO       = 43,
    PAGE_FAULT            = 44,
    LEER_MEMORIA          = 45,
    RESPUESTA_LEER_MEMORIA = 46,
    ESCRIBIR_MEMORIA      = 47,
    ESCRIBIR_MEMORIA_OK   = 48,
    DESLOCKEAR_PAGINAS    = 49,
    FETCH_ERROR           = 50,
    DESLOCKEO_OK          = 51,
    OBTENER_TAM_PAGINA    = 52,
    RESPUESTA_TAM_PAGINA  = 53,
    OBTENER_MARCO_ERROR   = 54,


    // --- 70-99: Planificador <-> Placa (Check 3) ---
    CREAR_JOB           = 70,
    FINALIZAR_JOB       = 71,
    ALLOC               = 72,
    ALLOC_OK            = 73,
    ALLOC_ERROR         = 74,
    FREE                = 75,
    CARGAR_PAGINA       = 76,
    CREAR_JOB_OK = 81,
    CREAR_JOB_ERROR = 82,
    FINALIZAR_JOB_OK = 83,
    FREE_OK             = 84,
    CARGAR_PAGINA_OK    = 85

} t_op_code;


typedef enum {
    FETCH_ERROR_PC_FUERA_DE_RANGO = 0,
    FETCH_ERROR_JID_INEXISTENTE   = 1
} t_fetch_error;

// Motivo del OBTENER_MARCO_ERROR (54): por qué la Placa no pudo dar el marco. Igual que t_fetch_error.
typedef enum {
    MARCO_ERROR_JID_INEXISTENTE    = 0,   // no hay Job con ese JID
    MARCO_ERROR_PAGINA_NO_ASIGNADA = 1    // la página no es de ninguna asignación (ALLOC) del Job
} t_marco_error;

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
