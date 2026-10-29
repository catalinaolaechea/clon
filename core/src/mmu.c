#include "mmu.h"

static uint32_t tam_pagina_placa = 0;

// true: Core tiene paginas lockeadas en la Placa 
static bool hay_paginas_lockeadas = false;

uint32_t tam_pagina(void) {
    return tam_pagina_placa;
}

bool obtener_tam_pagina(void) {
    t_paquete* paquete = crear_paquete(OBTENER_TAM_PAGINA);   
    int resultado = enviar_paquete(fd_placa, paquete);
    eliminar_paquete(paquete);

    if (resultado != CONEXION_OK) {
        log_error(core_logger, "Core %d: no se pudo pedir el tamanio de pagina a la Placa", identificador);
        return false;
    }

    uint8_t op_code;
    if (recibir_operacion(fd_placa, &op_code) != CONEXION_OK) {
        log_error(core_logger, "Core %d: la Placa no respondio el tamanio de pagina", identificador);
        return false;
    }

    // Siempre se lee el payload completo, sea cual sea la respuesta.
    t_buffer* buffer;
    if (recibir_buffer(fd_placa, &buffer) != CONEXION_OK) {
        log_error(core_logger, "Core %d: respuesta incompleta al pedir el tamanio de pagina", identificador);
        return false;
    }

    uint32_t tamanio = 0;
    if (op_code == RESPUESTA_TAM_PAGINA && buffer->size >= sizeof(uint32_t)) {
        tamanio = buffer_read_uint32(buffer);
    }
    eliminar_buffer(buffer);

    if (op_code != RESPUESTA_TAM_PAGINA) {
        log_error(core_logger, "Core %d: respuesta inesperada al pedir el tamanio de pagina (op_code %u)", identificador, op_code);
        return false;
    }

    if (tamanio == 0) {
        log_error(core_logger, "Core %d: la Placa informo un tamanio de pagina invalido (0)", identificador);
        return false;
    }

    tam_pagina_placa = tamanio;
    log_info(core_logger, "Core %d: tamanio de pagina de la Placa: %u bytes", identificador, tam_pagina_placa);
    return true;
}

static const char* motivo_marco_error_to_string(uint32_t motivo) {
    switch (motivo) {
        case MARCO_ERROR_JID_INEXISTENTE:    return "el JID no existe";
        case MARCO_ERROR_PAGINA_NO_ASIGNADA: return "la pagina no pertenece a ninguna asignacion del Job";
        default:                             return "motivo desconocido";
    }
}

typedef enum {
    MARCO_OK,
    MARCO_PAGE_FAULT,
    MARCO_ERROR
} t_resultado_marco;

// OBTENER_MARCO = { jid, nro_pagina }. La Placa responde RESPUESTA_MARCO = { nro_marco }
// (la pagina queda lockeada) o PAGE_FAULT = { jid, nro_pagina }
static t_resultado_marco pedir_marco(uint32_t jid, uint32_t nro_pagina, uint32_t* nro_marco, uint32_t* pagina_faltante) {
    t_paquete* paquete = crear_paquete(OBTENER_MARCO);
    buffer_add_uint32(paquete->buffer, jid);
    buffer_add_uint32(paquete->buffer, nro_pagina);
    int resultado = enviar_paquete(fd_placa, paquete);
    eliminar_paquete(paquete);

    if (resultado != CONEXION_OK) {
        log_error(core_logger, "Core %d: no se pudo pedir el marco de la pagina %u (JID %u) a la Placa", identificador, nro_pagina, jid);
        return MARCO_ERROR;
    }

    uint8_t op_code;
    if (recibir_operacion(fd_placa, &op_code) != CONEXION_OK) {
        log_error(core_logger, "Core %d: la Placa no respondio el marco de la pagina %u (JID %u)", identificador, nro_pagina, jid);
        return MARCO_ERROR;
    }

    t_buffer* buffer;
    if (recibir_buffer(fd_placa, &buffer) != CONEXION_OK) {
        log_error(core_logger, "Core %d: respuesta incompleta al pedir el marco de la pagina %u (JID %u)", identificador, nro_pagina, jid);
        return MARCO_ERROR;
    }

    t_resultado_marco res = MARCO_ERROR;

    if (op_code == RESPUESTA_MARCO && buffer->size >= sizeof(uint32_t)) {
        *nro_marco = buffer_read_uint32(buffer);
        hay_paginas_lockeadas = true;                       
        res = MARCO_OK;

    } else if (op_code == PAGE_FAULT && buffer->size >= 2 * sizeof(uint32_t)) {
        buffer_read_uint32(buffer);                         
        *pagina_faltante = buffer_read_uint32(buffer);      
        res = MARCO_PAGE_FAULT;

    } else if (op_code == OBTENER_MARCO_ERROR && buffer->size >= sizeof(uint32_t)) {
        uint32_t motivo = buffer_read_uint32(buffer);
        log_error(core_logger, "Core %d: la Placa no pudo dar el marco de la pagina %u (JID %u): %s", identificador, nro_pagina, jid, motivo_marco_error_to_string(motivo));
        // res queda en MARCO_ERROR: el Job se finaliza, igual que con FETCH_ERROR

    } else {
        log_error(core_logger, "Core %d: respuesta inesperada al pedir el marco de la pagina %u (JID %u): op_code %u", identificador, nro_pagina, jid, op_code);
    }

    eliminar_buffer(buffer);
    return res;
}

t_resultado_traduccion traducir_rango(uint32_t jid, uint32_t dir_logica, uint32_t tamanio, t_list** direcciones, uint32_t* pagina_faltante) {
    *direcciones = NULL;

    if (tam_pagina_placa == 0) {
        log_error(core_logger, "Core %d: se pidio traducir antes de conocer el tamanio de pagina", identificador);
        return TRADUCCION_ERROR;
    }

    // El rango no puede pasarse del espacio de direcciones de 32 bits.
    if ((uint64_t) dir_logica + tamanio > (uint64_t) UINT32_MAX + 1) {
        log_error(core_logger, "Core %d: rango logico invalido (JID %u): dir %u, tamanio %u", identificador, jid, dir_logica, tamanio);
        return TRADUCCION_ERROR;
    }

    t_list* lista = list_create();
    uint32_t dir_actual = dir_logica;
    uint32_t restante   = tamanio;

    while (restante > 0) {
        uint32_t nro_pagina     = dir_actual / tam_pagina_placa;
        uint32_t desplazamiento = dir_actual % tam_pagina_placa;

        uint32_t en_esta_pagina = tam_pagina_placa - desplazamiento;
        uint32_t bytes = (restante < en_esta_pagina) ? restante : en_esta_pagina;

        uint32_t nro_marco = 0;
        t_resultado_marco marco = pedir_marco(jid, nro_pagina, &nro_marco, pagina_faltante);

        if (marco == MARCO_PAGE_FAULT) {
            direcciones_destruir(lista);
            return TRADUCCION_PAGE_FAULT;
        }
        if (marco == MARCO_ERROR) {
            direcciones_destruir(lista);
            return TRADUCCION_ERROR;
        }

        uint64_t dir_fisica = (uint64_t) nro_marco * tam_pagina_placa + desplazamiento;
        if (dir_fisica > UINT32_MAX) {
            log_error(core_logger, "Core %d: la direccion fisica de la pagina %u (JID %u) no entra en 32 bits", identificador, nro_pagina, jid);
            direcciones_destruir(lista);
            return TRADUCCION_ERROR;
        }

        list_add(lista, direccion_fisica_crear((uint32_t) dir_fisica, bytes));

        dir_actual += bytes;
        restante   -= bytes;
    }

    *direcciones = lista;
    return TRADUCCION_OK;
}

bool deslockear_paginas(uint32_t jid) {
    t_paquete* paquete = crear_paquete(DESLOCKEAR_PAGINAS);
    buffer_add_uint32(paquete->buffer, jid);
    int resultado = enviar_paquete(fd_placa, paquete);
    eliminar_paquete(paquete);

    if (resultado != CONEXION_OK) {
        log_error(core_logger, "Core %d: no se pudo pedir el deslockeo de paginas (JID %u)", identificador, jid);
        return false;
    }

    uint8_t op_code;
    if (recibir_operacion(fd_placa, &op_code) != CONEXION_OK) {
        log_error(core_logger, "Core %d: la Placa no confirmo el deslockeo de paginas (JID %u)", identificador, jid);
        return false;
    }

    t_buffer* buffer;
    if (recibir_buffer(fd_placa, &buffer) != CONEXION_OK) {
        log_error(core_logger, "Core %d: respuesta incompleta al deslockear paginas (JID %u)", identificador, jid);
        return false;
    }
    eliminar_buffer(buffer);

    if (op_code != DESLOCKEO_OK) {
        log_error(core_logger, "Core %d: respuesta inesperada al deslockear paginas (JID %u): op_code %u", identificador, jid, op_code);
        return false;
    }

    hay_paginas_lockeadas = false;
    return true;
}

bool liberar_paginas_del_ciclo(uint32_t jid) {
    if (!hay_paginas_lockeadas) return true;   // nada que liberar: no se molesta a la Placa
    return deslockear_paginas(jid);
}

bool atender_page_fault(uint32_t jid, uint32_t nro_pagina) {
    log_info(core_logger, LOG_PAGE_FAULT, jid, nro_pagina);
    return deslockear_paginas(jid);
}

// LEER_MEMORIA = { jid, dir_fisica, tamanio }. La Placa responde RESPUESTA_LEER_MEMORIA = { bloque }.
static bool leer_pedazo(uint32_t jid, const t_direccion_fisica* pedazo, void* destino) {
    log_info(core_logger, LOG_ACCESO_MEMORIA, jid, "Lectura", pedazo->direccion, pedazo->tamanio);

    t_paquete* paquete = crear_paquete(LEER_MEMORIA);
    buffer_add_uint32(paquete->buffer, jid);
    buffer_add_uint32(paquete->buffer, pedazo->direccion);
    buffer_add_uint32(paquete->buffer, pedazo->tamanio);
    int resultado = enviar_paquete(fd_placa, paquete);
    eliminar_paquete(paquete);

    if (resultado != CONEXION_OK) {
        log_error(core_logger, "Core %d: no se pudo pedir la lectura de memoria (JID %u, direccion %u)", identificador, jid, pedazo->direccion);
        return false;
    }

    uint8_t op_code;
    if (recibir_operacion(fd_placa, &op_code) != CONEXION_OK) {
        log_error(core_logger, "Core %d: la Placa no respondio la lectura (JID %u, direccion %u)", identificador, jid, pedazo->direccion);
        return false;
    }

    t_buffer* buffer;
    if (recibir_buffer(fd_placa, &buffer) != CONEXION_OK) {
        log_error(core_logger, "Core %d: respuesta incompleta a la lectura (JID %u, direccion %u)", identificador, jid, pedazo->direccion);
        return false;
    }

    bool ok = false;

    if (op_code == RESPUESTA_LEER_MEMORIA) {
        // El bloque es { uint32 tamanio, bytes }: se verifica que traiga justo lo pedido antes de copiar.
        uint32_t declarado = 0;
        if (buffer->size >= sizeof(uint32_t)) memcpy(&declarado, buffer->stream, sizeof(uint32_t));

        if (declarado == pedazo->tamanio && buffer->size == sizeof(uint32_t) + declarado) {
            uint32_t recibido;
            void* datos = buffer_read_bloque(buffer, &recibido);
            memcpy(destino, datos, recibido);
            free(datos);
            ok = true;
        } else {
            log_error(core_logger, "Core %d: la Placa devolvio un bloque de tamanio inesperado (JID %u, direccion %u): pedi %u", identificador, jid, pedazo->direccion, pedazo->tamanio);
        }
    } else {
        log_error(core_logger, "Core %d: respuesta inesperada a la lectura (JID %u, direccion %u): op_code %u", identificador, jid, pedazo->direccion, op_code);
    }

    eliminar_buffer(buffer);
    return ok;
}

// ESCRIBIR_MEMORIA = { jid, dir_fisica, bloque }. La Placa responde ESCRIBIR_MEMORIA_OK (vacio).
static bool escribir_pedazo(uint32_t jid, const t_direccion_fisica* pedazo, const void* datos) {
    log_info(core_logger, LOG_ACCESO_MEMORIA, jid, "Escritura", pedazo->direccion, pedazo->tamanio);

    t_paquete* paquete = crear_paquete(ESCRIBIR_MEMORIA);
    buffer_add_uint32(paquete->buffer, jid);
    buffer_add_uint32(paquete->buffer, pedazo->direccion);
    buffer_add_bloque(paquete->buffer, (void*) datos, pedazo->tamanio);
    int resultado = enviar_paquete(fd_placa, paquete);
    eliminar_paquete(paquete);

    if (resultado != CONEXION_OK) {
        log_error(core_logger, "Core %d: no se pudo pedir la escritura de memoria (JID %u, direccion %u)", identificador, jid, pedazo->direccion);
        return false;
    }

    uint8_t op_code;
    if (recibir_operacion(fd_placa, &op_code) != CONEXION_OK) {
        log_error(core_logger, "Core %d: la Placa no confirmo la escritura (JID %u, direccion %u)", identificador, jid, pedazo->direccion);
        return false;
    }

    t_buffer* buffer;
    if (recibir_buffer(fd_placa, &buffer) != CONEXION_OK) {
        log_error(core_logger, "Core %d: respuesta incompleta a la escritura (JID %u, direccion %u)", identificador, jid, pedazo->direccion);
        return false;
    }
    eliminar_buffer(buffer);

    if (op_code != ESCRIBIR_MEMORIA_OK) {
        log_error(core_logger, "Core %d: respuesta inesperada a la escritura (JID %u, direccion %u): op_code %u", identificador, jid, pedazo->direccion, op_code);
        return false;
    }

    return true;
}

bool leer_memoria(uint32_t jid, t_list* direcciones, void** contenido, uint32_t* tamanio) {
    *contenido = NULL;
    *tamanio = 0;

    uint64_t total = 0;
    for (int i = 0; i < list_size(direcciones); i++) {
        total += ((t_direccion_fisica*) list_get(direcciones, i))->tamanio;
    }

    if (total > UINT32_MAX) {
        log_error(core_logger, "Core %d: lectura demasiado grande (JID %u): %lu bytes", identificador, jid, (unsigned long) total);
        return false;
    }
    if (total == 0) return true;

    uint8_t* datos = malloc(total);
    uint32_t escrito = 0;

    for (int i = 0; i < list_size(direcciones); i++) {
        t_direccion_fisica* pedazo = list_get(direcciones, i);

        if (!leer_pedazo(jid, pedazo, datos + escrito)) {
            free(datos);
            return false;
        }
        escrito += pedazo->tamanio;
    }

    *contenido = datos;
    *tamanio = (uint32_t) total;
    return true;
}

bool escribir_memoria(uint32_t jid, t_list* direcciones, const void* contenido) {
    const uint8_t* datos = contenido;
    uint32_t leido = 0;

    for (int i = 0; i < list_size(direcciones); i++) {
        t_direccion_fisica* pedazo = list_get(direcciones, i);

        if (!escribir_pedazo(jid, pedazo, datos + leido)) return false;
        leido += pedazo->tamanio;
    }

    return true;
}