#ifndef INSTRUCCIONES_H_
#define INSTRUCCIONES_H_

#include <stdint.h>
#include <utils/contexto.h>

#define MAX_PARAMETROS 8

// Una linea de pseudocodigo ya separada en nombre y parametros (resultado del DECODE).
// 'linea' es una copia propia; 'nombre' y 'parametros' apuntan adentro de esa copia.
typedef struct {
    char* linea;
    char* nombre;                        // "SET", "JNZ", ...
    char* parametros[MAX_PARAMETROS];    // "AX", "3", ...
    int cantidad_parametros;
} t_instruccion;

typedef enum {
    EJECUCION_OK,             // se ejecuto; el ciclo le suma 1 al PC
    EJECUCION_PC_MODIFICADO,  // se ejecuto y la instruccion escribio el PC: NO se suma 1
    EJECUCION_NO_BASICA,      // no es una de las 5 basicas (NOOP, SET, SUM, SUB, JNZ)
    EJECUCION_ERROR           // parametros invalidos (registro inexistente, cantidad, etc.)
} t_resultado_ejecucion;

// DECODE: separa la linea en nombre y parametros. Devuelve NULL si la linea esta vacia
// o tiene mas de MAX_PARAMETROS parametros. Se libera con destruir_instruccion().
t_instruccion* decodificar_instruccion(const char* linea);
void destruir_instruccion(t_instruccion* instruccion);

// Los parametros unidos por espacios ("AX 3"), para el log. String malloc'eado: el
// llamador hace free(). Sin parametros devuelve "".
char* parametros_a_string(const t_instruccion* instruccion);

// Puntero al registro del contexto con ese nombre ("PC", "P0".."P5", "AX".."DX", "E1".."E3"),
// o NULL si no existe.
uint32_t* registro_por_nombre(t_contexto* contexto, const char* nombre);

// EXECUTE de las instrucciones basicas sobre los registros del contexto. NO suma 1 al PC:
// eso lo decide el ciclo segun el resultado (EJECUCION_OK suma, EJECUCION_PC_MODIFICADO no).
// Los registros son uint32_t: SUB por debajo de 0 da la vuelta (aritmetica sin signo de C).
t_resultado_ejecucion ejecutar_basica(const t_instruccion* instruccion, t_contexto* contexto);

#endif