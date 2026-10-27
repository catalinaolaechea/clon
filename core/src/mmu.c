#include "mmu.h"

static uint32_t tam_pagina_placa = 0;

uint32_t tam_pagina(void) {
    return tam_pagina_placa;
}

bool obtener_tam_pagina(void) {
    t_paquete* paquete = crear_paquete(OBTENER_TAM_PAGINA);   // payload vacio
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