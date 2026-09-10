#include "handshake.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>


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

// La usan enviar_handshake y recibir_handshake, para que los dos lados no puedan
// desalinearse: si un dia cambia la regla, cambia para los dos a la vez.
static bool requiere_identificador(t_modulo modulo) {
    return modulo == MODULO_CORE;
}

static int enviar_handshake_y_esperar_respuesta(int fd, t_paquete* paquete){

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
        case HANDSHAKE_OK: {
            t_buffer* respuesta;
            resultado = recibir_buffer(fd, &respuesta);
            if (resultado != CONEXION_OK) return resultado;

            eliminar_buffer(respuesta);
            return CONEXION_OK;
        }
        case HANDSHAKE_ERROR: {
            t_buffer* respuesta;
            resultado = recibir_buffer(fd, &respuesta);
            if (resultado != CONEXION_OK) return resultado;

            char* motivo = buffer_read_string(respuesta);
            eliminar_buffer(respuesta);

            fprintf(stderr, "enviar_handshake: rechazado por el servidor: %s\n", motivo);
            free(motivo);
            return CONEXION_ERROR;
        }
        default:
            fprintf(stderr, "enviar_handshake: op_code inesperado: %u\n", respuesta_op_code);
            return CONEXION_ERROR;
    }

}

int enviar_handshake(int fd, t_modulo modulo, t_canal canal, int identificador) {
    // Creo el paquete de handshake
    t_paquete* paquete = crear_paquete(HANDSHAKE);

    // Agrego los datos del handshake al paquete
    buffer_add_uint8(paquete->buffer, modulo);
    buffer_add_uint8(paquete->buffer, canal);

    if (requiere_identificador(modulo)) {
        buffer_add_uint8(paquete->buffer, identificador);
    }

    return enviar_handshake_y_esperar_respuesta(fd, paquete);
}

static int recibir_payload_handshake(int fd, t_buffer** buffer){

    uint8_t op_code;
    int resultado = recibir_operacion(fd, &op_code);
    if (resultado != CONEXION_OK) {
        fprintf(stderr, "recibir_handshake: no llego el handshake\n");
        return resultado;
    }

    if (op_code != HANDSHAKE) {
        fprintf(stderr, "recibir_handshake: el primer mensaje no es un handshake (op_code %d)\n", op_code);
        return CONEXION_ERROR;
    }

    resultado = recibir_buffer(fd, buffer);
    if (resultado != CONEXION_OK) {
        fprintf(stderr, "recibir_handshake: no llego el payload del handshake\n");
        return resultado;
    }

    return CONEXION_OK;

}

static int validar_y_responder_handshake(int fd, t_modulo servidor, t_modulo cliente, t_canal canal){

    if (!handshake_valido(servidor, cliente, canal)) {
        fprintf(stderr, "recibir_handshake: rechazado (modulo %d, canal %d)\n", cliente, canal);

        t_paquete* rechazo = crear_paquete(HANDSHAKE_ERROR);
        buffer_add_string(rechazo->buffer, "modulo o canal no esperado por este servidor");
        enviar_paquete(fd, rechazo);
        eliminar_paquete(rechazo);

        return CONEXION_ERROR;
    }

    t_paquete* ok = crear_paquete(HANDSHAKE_OK);
    int resultado = enviar_paquete(fd, ok);
    eliminar_paquete(ok);

    return resultado;

}

int recibir_handshake(int fd, t_modulo servidor, t_modulo* cliente, t_canal* canal, int* identificador) {

    t_buffer* buffer;

    int resultado = recibir_payload_handshake(fd, &buffer);

    if (resultado != CONEXION_OK) return resultado;

    *cliente       = buffer_read_uint8(buffer);
    *canal         = buffer_read_uint8(buffer);
    *identificador = 0;   // los modulos unicos no mandan identificador

    if (requiere_identificador(*cliente)) {
        *identificador = buffer_read_int(buffer);
    }

    eliminar_buffer(buffer);

    resultado = validar_y_responder_handshake(fd, servidor, *cliente, *canal);

    return resultado;
}
