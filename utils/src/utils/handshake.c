#include "handshake.h"
#include <stdbool.h>


// Define los modulos que pueden conectarse entre si y los canales que pueden usar para comunicarse
static bool handshake_valido(t_modulo servidor, t_modulo cliente, t_canal canal)  {
    
    switch(servidor) {
        case MODULO_PLANIFICADOR:
            if (cliente == MODULO_CORE && canal == CANAL_DISPATCH) return true;
            if (cliente == MODULO_CORE && canal == CANAL_INTERRUPT) return true;
            break;
        case MODULO_PLACA:
            if (cliente == MODULO_PLANIFICADOR && canal == CANAL_UNICO) return true;
            if (cliente == MODULO_CORE && canal == CANAL_UNICO) return true;
            break;
        case MODULO_STORAGE:
            if (cliente == MODULO_PLANIFICADOR && canal == CANAL_UNICO) return true;
            break;
        default:
            break;
    }

    return false;
}

int enviar_handshake(int fd, t_modulo modulo, t_canal canal, char* identificador) {
    // Creo el paquete de handshake
    t_paquete* paquete = crear_paquete(HANDSHAKE);

    // Agrego los datos del handshake al paquete
    buffer_add_uint8(paquete->buffer, modulo);
    buffer_add_uint8(paquete->buffer, canal);
    buffer_add_string(paquete->buffer, identificador);

    // Envio el paquete
    int resultado = enviar_paquete(fd, paquete);

    // Libero la memoria del paquete
    eliminar_paquete(paquete);

    if (resultado != CONEXION_OK) {
        fprintf(stderr, "Error al enviar el handshake: %d\n", resultado);
        return resultado;
    }

    uint8_t respuesta_op_code;
    // Recibo que operacion voy a realizar
    int resultado_op_code = recibir_operacion(fd, &respuesta_op_code);

    if (resultado_op_code != CONEXION_OK) {
        fprintf(stderr, "Error al recibir el resultado del op_code: %d\n", resultado_op_code);
        return resultado_op_code;
    }

    switch (respuesta_op_code) {
        case HANDSHAKE_OK:
            t_buffer* respuesta;                   
            recibir_buffer(fd, &respuesta); 
            eliminar_buffer(respuesta);
            return CONEXION_OK;
        case HANDSHAKE_ERROR:
            t_buffer* respuesta;                   
            recibir_buffer(fd, &respuesta);        
            char* motivo = buffer_read_string(respuesta); 
            eliminar_buffer(respuesta);
            fprintf(stderr, "Handshake rechazado: %s\n", motivo);
            return HANDSHAKE_INVALIDO;
        default:
            fprintf(stderr, "Operacion desconocida recibida: %d\n", respuesta_op_code);
            return OPERACION_DESCONOCIDA;
    }

    return resultado;
}
