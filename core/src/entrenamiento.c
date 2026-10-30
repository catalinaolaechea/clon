#include "entrenamiento.h"
#include "mmu.h"

#define CANTIDAD_E 3                         
#define TAMANIO_REGISTRO sizeof(uint32_t)    

typedef struct {
    const char* nombre;
    int cantidad_parametros;      
    bool tiene_acumulador;        
    bool lee[CANTIDAD_E];         
    bool escribe[CANTIDAD_E];     
} t_especificacion;

static const t_especificacion ESPECIFICACIONES[] = {
    { "FORWARD",  4, true,  { true, true, false }, { false, false, true } },
    { "BACKWARD", 4, true,  { true, true, false }, { false, false, true } },
    { "UPDATE",   3, false, { true, true, true  }, { false, true,  true } }
};

static const t_especificacion* buscar_especificacion(const char* nombre) {
    for (size_t i = 0; i < sizeof(ESPECIFICACIONES) / sizeof(ESPECIFICACIONES[0]); i++) {
        if (strcmp(nombre, ESPECIFICACIONES[i].nombre) == 0) return &ESPECIFICACIONES[i];
    }
    return NULL;
}

bool es_instruccion_entrenamiento(const char* nombre) {
    return buscar_especificacion(nombre) != NULL;
}

void calcular_entrenamiento(const char* nombre, uint32_t e[3], uint32_t* acumulador) {
    if (strcmp(nombre, "FORWARD") == 0) {
        *acumulador = *acumulador + e[0] * e[1];
        e[2] = *acumulador;

    } else if (strcmp(nombre, "BACKWARD") == 0) {
        *acumulador = *acumulador - e[0] * e[1];
        e[2] = *acumulador;

    } else if (strcmp(nombre, "UPDATE") == 0) {
        e[1] = (e[1] + e[0]) / 2;
        e[2] = e[2] - e[1];
    }
}

static bool es_registro_puntero(const char* nombre) {
    return strlen(nombre) == 2 && nombre[0] == 'P' && nombre[1] >= '0' && nombre[1] <= '5';
}

static void destruir_listas(t_list* listas[CANTIDAD_E]) {
    for (int i = 0; i < CANTIDAD_E; i++) {
        if (listas[i] != NULL) direcciones_destruir(listas[i]);
        listas[i] = NULL;
    }
}

t_resultado_entrenamiento ejecutar_entrenamiento(uint32_t jid, const t_instruccion* instruccion, t_contexto* contexto, uint32_t* pagina_faltante) {
    const t_especificacion* spec = buscar_especificacion(instruccion->nombre);

    if (spec == NULL || instruccion->cantidad_parametros != spec->cantidad_parametros) {
        log_error(core_logger, "Core %d: %s espera %d parametros y recibio %d (JID %u, PC %u)", identificador,
                  instruccion->nombre, spec ? spec->cantidad_parametros : 0, instruccion->cantidad_parametros, jid, contexto->pc);
        return ENTRENAMIENTO_ERROR;
    }

    uint32_t dir_logica[CANTIDAD_E];
    for (int i = 0; i < CANTIDAD_E; i++) {
        uint32_t* puntero = es_registro_puntero(instruccion->parametros[i]) ? registro_por_nombre(contexto, instruccion->parametros[i]) : NULL;
        if (puntero == NULL) {
            log_error(core_logger, "Core %d: %s: el parametro %d (%s) tiene que ser un registro puntero P0..P5 (JID %u, PC %u)",
                      identificador, instruccion->nombre, i + 1, instruccion->parametros[i], jid, contexto->pc);
            return ENTRENAMIENTO_ERROR;
        }
        dir_logica[i] = *puntero;
    }

    uint32_t* acumulador = NULL;
    if (spec->tiene_acumulador) {
        acumulador = registro_por_nombre(contexto, instruccion->parametros[CANTIDAD_E]);
        if (acumulador == NULL) {
            log_error(core_logger, "Core %d: %s: el acumulador %s no es un registro (JID %u, PC %u)", identificador,
                      instruccion->nombre, instruccion->parametros[CANTIDAD_E], jid, contexto->pc);
            return ENTRENAMIENTO_ERROR;
        }
    }

    // 1) TRADUCIR todo antes de tocar la memoria o los registros. Cada operando se traduce una sola vez
    // y esa lista sirve para leerlo y para escribirlo.
    t_list* direcciones[CANTIDAD_E] = { NULL, NULL, NULL };

    for (int i = 0; i < CANTIDAD_E; i++) {
        if (!spec->lee[i] && !spec->escribe[i]) continue;

        t_resultado_traduccion traduccion = traducir_rango(jid, dir_logica[i], TAMANIO_REGISTRO, &direcciones[i], pagina_faltante);

        if (traduccion == TRADUCCION_PAGE_FAULT) {
            destruir_listas(direcciones);
            // Se loguea, se deslockea lo ya obtenido, y el llamador devuelve el Job con el PC sin sumar.
            return atender_page_fault(jid, *pagina_faltante) ? ENTRENAMIENTO_PAGE_FAULT : ENTRENAMIENTO_ERROR;
        }
        if (traduccion == TRADUCCION_ERROR) {
            destruir_listas(direcciones);
            return ENTRENAMIENTO_ERROR;
        }
    }

    // 2) CARGAR en E1/E2/E3 solo lo que la instruccion consume.
    uint32_t e[CANTIDAD_E] = { contexto->e1, contexto->e2, contexto->e3 };

    for (int i = 0; i < CANTIDAD_E; i++) {
        if (!spec->lee[i]) continue;

        void* contenido = NULL;
        uint32_t tamanio = 0;
        bool ok = leer_memoria(jid, direcciones[i], &contenido, &tamanio) && tamanio == TAMANIO_REGISTRO;

        if (ok) memcpy(&e[i], contenido, TAMANIO_REGISTRO);
        free(contenido);

        if (!ok) {
            destruir_listas(direcciones);
            return ENTRENAMIENTO_ERROR;
        }
    }

    // 3) CALCULAR
    uint32_t acum = acumulador ? *acumulador : 0;
    calcular_entrenamiento(instruccion->nombre, e, &acum);

    // 4) ESCRIBIR en la memoria del Job solo los E que la instruccion modifico.
    for (int i = 0; i < CANTIDAD_E; i++) {
        if (!spec->escribe[i]) continue;

        if (!escribir_memoria(jid, direcciones[i], &e[i])) {
            destruir_listas(direcciones);
            return ENTRENAMIENTO_ERROR;
        }
    }

    // 5) Recien ahora, con todo hecho, se actualizan los registros del contexto.
    contexto->e1 = e[0];
    contexto->e2 = e[1];
    contexto->e3 = e[2];
    if (acumulador != NULL) *acumulador = acum;

    destruir_listas(direcciones);
    return ENTRENAMIENTO_OK;
}