#ifndef CICLO_H_
#define CICLO_H_
 
#include "inicializador.h"
#include "conexiones.h"
#include "instrucciones.h"
 
// Log obligatorio del Fetch: "## JID: <JID> - FETCH - Program Counter: <PROGRAM_COUNTER>"
#define LOG_FETCH "## JID: %u - FETCH - Program Counter: %u"
 
// FETCH (COR-05): le pide a la Placa la instruccion numero 'pc' base 0 del Job 'jid'.

// Devuelve la linea de pseudocodigo en un string malloc'eado: el llamador
// hace free(). Devuelve NULL si no se pudo obtener (error de red, pc fuera
// de rango, jid desconocido); el motivo queda logueado con log_error.
char* fetch_instruccion(uint32_t jid, uint32_t pc);

#define LOG_EJECUTANDO "## JID: %u - Ejecutando: %s - %s"
 
typedef enum {
    CICLO_CONTINUA,   // ejecuto una instruccion basica y el Job puede seguir
    CICLO_NO_BASICA,  // la instruccion no es basica (EXIT, syscalls, entrenamiento): ver 'pendiente'
    CICLO_ERROR       // fallo el Fetch o la instruccion es invalida (el motivo queda logueado)
} t_resultado_ciclo;

t_resultado_ciclo ejecutar_ciclo(uint32_t jid, t_contexto* contexto, t_instruccion** pendiente);
 
#endif