
typedef enum {
    MARCO_OK,
    MARCO_PAGE_FAULT,
    MARCO_ERROR
} t_resultado_marco;

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
        res = MARCO_OK;

    } else if (op_code == PAGE_FAULT && buffer->size >= 2 * sizeof(uint32_t)) {
        buffer_read_uint32(buffer);                         // jid: es el nuestro
        *pagina_faltante = buffer_read_uint32(buffer);      // la pagina que dice la Placa
        res = MARCO_PAGE_FAULT;

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

        // Cuanto del rango entra en ESTA pagina: lo que queda de pagina o lo que falta, lo menor.
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