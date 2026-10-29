#ifndef ENTRENAMIENTO_H_
#define ENTRENAMIENTO_H_

#include <stdbool.h>
#include <stdint.h>
#include <utils/contexto.h>
#include "instrucciones.h"

typedef enum {
    ENTRENAMIENTO_OK,          // se ejecuto: el llamador le suma 1 al PC
    ENTRENAMIENTO_PAGE_FAULT,  // alguna pagina no estaba: ya se logueo y se deslockeo, ver 'pagina_faltante'
    ENTRENAMIENTO_ERROR        // parametros invalidos o fallo de la Placa : se finaliza el Job
} t_resultado_entrenamiento;

bool es_instruccion_entrenamiento(const char* nombre);

// La cuenta de cada instruccion, sobre E1, E2, E3 (e[0], e[1], e[2]) y el acumulador.
void calcular_entrenamiento(const char* nombre, uint32_t e[3], uint32_t* acumulador);

// Ejecuta la instruccion completa: traduce TODAS las direcciones primero (si una pagina falta, no se
// toca ni la memoria ni los registros), carga lo que consume, calcula, y escribe lo modificado.
// Las paginas quedan lockeadas: las libera el llamador al terminar el ciclo.
// Con ENTRENAMIENTO_PAGE_FAULT el contexto queda intacto y el llamador devuelve el Job SIN sumar 1 al PC.
t_resultado_entrenamiento ejecutar_entrenamiento(uint32_t jid, const t_instruccion* instruccion, t_contexto* contexto, uint32_t* pagina_faltante);

#endif