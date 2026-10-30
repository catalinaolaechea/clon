#include "servicios.h"
#include "mmu.h"

typedef struct {
    const char* nombre;
    uint8_t op_code;
    int cantidad_parametros;   
    bool tiene_archivo;
} t_especificacion;

static const t_especificacion ESPECIFICACIONES[] = {
    { "LOAD_BATCH", SYSCALL_LOAD_BATCH, 3, true  },
    { "LABEL",      SYSCALL_LABEL,      2, false },
    { "REPORT",     SYSCALL_REPORT,     2, false }
};

static const t_especificacion* buscar_especificacion(const char* nombre) {
    for (size_t i = 0; i < sizeof(ESPECIFICACIONES) / sizeof(ESPECIFICACIONES[0]); i++) {
        if (strcmp(nombre, ESPECIFICACIONES[i].nombre) == 0) 
            return &ESPECIFICACIONES[i];
    }
    return NULL;
}

bool es_syscall_de_servicio(const char* nombre) {
    return buscar_especificacion(nombre) != NULL;
}

t_resultado_servicio preparar_syscall_servicio(uint32_t jid, const t_instruccion* instruccion, t_contexto* contexto, t_syscall_servicio* servicio, uint32_t* pagina_faltante) {
    memset(servicio, 0, sizeof(*servicio));

    const t_especificacion* spec = buscar_especificacion(instruccion->nombre);

    if (spec == NULL || instruccion->cantidad_parametros != spec->cantidad_parametros) {
        log_error(core_logger, "Core %d: %s espera %d parametros y recibio %d (JID %u, PC %u)", identificador, instruccion->nombre, spec ? spec->cantidad_parametros : 0, instruccion->cantidad_parametros, jid, contexto->pc);
        return SERVICIO_ERROR;
    }

    int primero = spec->tiene_archivo ? 1 : 0;
    uint32_t* registro_direccion = registro_por_nombre(contexto, instruccion->parametros[primero]);
    uint32_t* registro_tamanio   = registro_por_nombre(contexto, instruccion->parametros[primero + 1]);

    if (registro_direccion == NULL || registro_tamanio == NULL) {
        log_error(core_logger, "Core %d: %s: parametros que no son registros (%s, %s) (JID %u, PC %u)", identificador, instruccion->nombre, instruccion->parametros[primero], instruccion->parametros[primero + 1], jid, contexto->pc);
        return SERVICIO_ERROR;
    }

    uint32_t dir_logica = *registro_direccion;
    uint32_t tamanio    = *registro_tamanio;

    t_list* direcciones = NULL;
    t_resultado_traduccion traduccion = traducir_rango(jid, dir_logica, tamanio, &direcciones, pagina_faltante);

    if (traduccion == TRADUCCION_PAGE_FAULT) {
        return atender_page_fault(jid, *pagina_faltante) ? SERVICIO_PAGE_FAULT : SERVICIO_ERROR;
    }
    if (traduccion == TRADUCCION_ERROR) {
        return SERVICIO_ERROR;
    }

    servicio->op_code     = spec->op_code;
    servicio->archivo     = spec->tiene_archivo ? strdup(instruccion->parametros[0]) : NULL;
    servicio->tamanio     = tamanio;
    servicio->direcciones = direcciones;

    contexto->pc++;   // el Planificador recibe el PC apuntando a la siguiente instruccion
    return SERVICIO_OK;
}

void syscall_servicio_destruir(t_syscall_servicio* servicio) {
    free(servicio->archivo);
    if (servicio->direcciones != NULL) direcciones_destruir(servicio->direcciones);
    memset(servicio, 0, sizeof(*servicio));
}