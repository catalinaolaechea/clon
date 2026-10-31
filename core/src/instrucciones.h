#ifndef INSTRUCCIONES_H_
#define INSTRUCCIONES_H_

#include <stdint.h>
#include <stdbool.h>
#include <utils/contexto.h>

#define MAX_PARAMETROS 8

typedef struct {
    char* linea;
    char* nombre;                        
    char* parametros[MAX_PARAMETROS];    
    int cantidad_parametros;
} t_instruccion;

typedef enum {
    EJECUCION_OK,             // se ejecuto; el ciclo le suma 1 al PC
    EJECUCION_PC_MODIFICADO,  // se ejecuto y la instruccion escribio el PC: NO se suma 1
    EJECUCION_NO_BASICA,      // no es una de las 5 basicas (NOOP, SET, SUM, SUB, JNZ)
    EJECUCION_ERROR           // parametros invalidos (registro inexistente, cantidad, etc.)
} t_resultado_ejecucion;

t_instruccion* decodificar_instruccion(const char* linea);
void destruir_instruccion(t_instruccion* instruccion);

// Los parametros unidos por espacios ("AX 3"), para el log. String malloc'eado: el
// llamador hace free(). Sin parametros devuelve "".
char* parametros_a_string(const t_instruccion* instruccion);

// Puntero al registro del contexto con ese nombre ("PC", "P0".."P5", "AX".."DX", "E1".."E3"),
// o NULL si no existe.
uint32_t* registro_por_nombre(t_contexto* contexto, const char* nombre);

// numero o registro (su contenido)
bool valor_de(t_contexto* contexto, const char* operando, uint32_t* valor);

t_resultado_ejecucion ejecutar_basica(const t_instruccion* instruccion, t_contexto* contexto);

#endif