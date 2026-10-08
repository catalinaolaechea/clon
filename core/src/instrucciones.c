#include "instrucciones.h"
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define SEPARADORES " \t\r\n"

// ---------------------------------------------------------------- DECODE

t_instruccion* decodificar_instruccion(const char* linea) {
    if (linea == NULL) return NULL;

    t_instruccion* instruccion = calloc(1, sizeof(t_instruccion));
    instruccion->linea = strdup(linea);

    // strtok_r corta la copia en el lugar (reemplaza los separadores por '\0'),
    // por eso nombre y parametros quedan apuntando adentro de 'linea'.
    char* guardado = NULL;
    char* token = strtok_r(instruccion->linea, SEPARADORES, &guardado);

    if (token == NULL) {   // linea vacia
        destruir_instruccion(instruccion);
        return NULL;
    }

    instruccion->nombre = token;

    while ((token = strtok_r(NULL, SEPARADORES, &guardado)) != NULL) {
        if (instruccion->cantidad_parametros == MAX_PARAMETROS) {
            destruir_instruccion(instruccion);
            return NULL;
        }
        instruccion->parametros[instruccion->cantidad_parametros++] = token;
    }

    return instruccion;
}

void destruir_instruccion(t_instruccion* instruccion) {
    if (instruccion == NULL) return;
    free(instruccion->linea);
    free(instruccion);
}

char* parametros_a_string(const t_instruccion* instruccion) {
    size_t largo = 1;   // el '\0'
    for (int i = 0; i < instruccion->cantidad_parametros; i++) {
        largo += strlen(instruccion->parametros[i]) + 1;   // + el espacio separador
    }

    char* texto = malloc(largo);
    texto[0] = '\0';

    for (int i = 0; i < instruccion->cantidad_parametros; i++) {
        if (i > 0) strcat(texto, " ");
        strcat(texto, instruccion->parametros[i]);
    }

    return texto;
}

// ------------------------------------------------------------- REGISTROS

static const struct { const char* nombre; size_t offset; } REGISTROS[] = {
    { "PC", offsetof(t_contexto, pc) },
    { "P0", offsetof(t_contexto, p0) }, { "P1", offsetof(t_contexto, p1) },
    { "P2", offsetof(t_contexto, p2) }, { "P3", offsetof(t_contexto, p3) },
    { "P4", offsetof(t_contexto, p4) }, { "P5", offsetof(t_contexto, p5) },
    { "AX", offsetof(t_contexto, ax) }, { "BX", offsetof(t_contexto, bx) },
    { "CX", offsetof(t_contexto, cx) }, { "DX", offsetof(t_contexto, dx) },
    { "E1", offsetof(t_contexto, e1) }, { "E2", offsetof(t_contexto, e2) },
    { "E3", offsetof(t_contexto, e3) }
};

uint32_t* registro_por_nombre(t_contexto* contexto, const char* nombre) {
    for (size_t i = 0; i < sizeof(REGISTROS) / sizeof(REGISTROS[0]); i++) {
        if (strcmp(nombre, REGISTROS[i].nombre) == 0) {
            return (uint32_t*) ((char*) contexto + REGISTROS[i].offset);
        }
    }
    return NULL;
}

// --------------------------------------------------------------- EXECUTE

// Convierte un texto a uint32_t. Falla si no es un numero decimal valido o no entra.
static bool parsear_numero(const char* texto, uint32_t* resultado) {
    if (texto == NULL || texto[0] == '\0' || texto[0] == '-') return false;

    char* fin;
    errno = 0;
    unsigned long long valor = strtoull(texto, &fin, 10);

    if (*fin != '\0' || errno != 0 || valor > UINT32_MAX) return false;

    *resultado = (uint32_t) valor;
    return true;
}

// Un operando "Valor" puede ser un numero o el nombre de un registro (su contenido).
static bool valor_de(t_contexto* contexto, const char* operando, uint32_t* valor) {
    uint32_t* registro = registro_por_nombre(contexto, operando);
    if (registro != NULL) {
        *valor = *registro;
        return true;
    }
    return parsear_numero(operando, valor);
}

t_resultado_ejecucion ejecutar_basica(const t_instruccion* instruccion, t_contexto* contexto) {
    const char* nombre = instruccion->nombre;

    if (strcmp(nombre, "NOOP") == 0) {
        return instruccion->cantidad_parametros == 0 ? EJECUCION_OK : EJECUCION_ERROR;
    }

    if (strcmp(nombre, "SET") == 0 || strcmp(nombre, "SUM") == 0 || strcmp(nombre, "SUB") == 0) {
        if (instruccion->cantidad_parametros != 2) return EJECUCION_ERROR;

        uint32_t* destino = registro_por_nombre(contexto, instruccion->parametros[0]);
        uint32_t valor;

        if (destino == NULL || !valor_de(contexto, instruccion->parametros[1], &valor)) {
            return EJECUCION_ERROR;
        }

        if (strcmp(nombre, "SET") == 0)      *destino = valor;
        else if (strcmp(nombre, "SUM") == 0) *destino += valor;
        else                                 *destino -= valor;

        // Escribir el PC (SET PC 5, SUM PC 1...) tambien saltea el "sumar 1" del ciclo.
        return destino == &contexto->pc ? EJECUCION_PC_MODIFICADO : EJECUCION_OK;
    }

    if (strcmp(nombre, "JNZ") == 0) {
        if (instruccion->cantidad_parametros != 2) return EJECUCION_ERROR;

        uint32_t* registro = registro_por_nombre(contexto, instruccion->parametros[0]);
        uint32_t destino_pc;

        if (registro == NULL || !parsear_numero(instruccion->parametros[1], &destino_pc)) {
            return EJECUCION_ERROR;
        }

        if (*registro != 0) {
            contexto->pc = destino_pc;
            return EJECUCION_PC_MODIFICADO;
        }
        return EJECUCION_OK;   // registro en 0: no salta, el PC avanza normal
    }

    return EJECUCION_NO_BASICA;
}