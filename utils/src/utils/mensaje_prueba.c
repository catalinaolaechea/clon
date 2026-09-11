#include "mensaje_prueba.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <commons/string.h>

// El relleno lleva un patron deterministico y no ceros: si el loop de recv se saltea o duplica
// bytes, el patron se corre y la comparacion del eco lo detecta. 251 es primo para que el ciclo
// no se alinee con los limites de buffer, que son potencias de dos.
#define RELLENO_CICLO 251

// buffer_read no tiene cotas, asi que antes de cada lectura miramos cuanto queda.
static bool alcanza(t_buffer* buffer, uint32_t bytes) {
    if (buffer == NULL || buffer->offset > buffer->size) return false;
    return buffer->size - buffer->offset >= bytes;
}

// Para el log: el relleno son KB, no se compara a ojo. El checksum si.
static uint32_t checksum_relleno(t_mensaje_prueba* mensaje) {
    uint32_t acumulado = 0;
    uint8_t* bytes = mensaje->relleno;

    for (uint32_t i = 0; i < mensaje->tamanio_relleno; i++) {
        acumulado = acumulado * 31 + bytes[i];
    }

    return acumulado;
}

t_mensaje_prueba* mensaje_prueba_crear(t_modulo origen, uint32_t secuencia, uint32_t tamanio_relleno, char* texto) {
    t_mensaje_prueba* mensaje = malloc(sizeof(t_mensaje_prueba));

    mensaje->origen          = (uint8_t) origen;
    mensaje->secuencia       = secuencia;
    mensaje->tamanio_relleno = tamanio_relleno;
    mensaje->texto           = strdup(texto);  // copia: el llamador puede pasar un literal
    mensaje->relleno         = NULL;

    if (tamanio_relleno > 0) {
        mensaje->relleno = malloc(tamanio_relleno);

        for (uint32_t i = 0; i < tamanio_relleno; i++) {
            ((uint8_t*) mensaje->relleno)[i] = (uint8_t) (i % RELLENO_CICLO);
        }
    }

    return mensaje;
}

t_paquete* mensaje_prueba_empaquetar(t_mensaje_prueba* mensaje, uint8_t op_code) {
    t_paquete* paquete = crear_paquete(op_code);

    buffer_add_uint8 (paquete->buffer, mensaje->origen);
    buffer_add_uint32(paquete->buffer, mensaje->secuencia);
    buffer_add_uint32(paquete->buffer, mensaje->tamanio_relleno);
    buffer_add_string(paquete->buffer, mensaje->texto);

    if (mensaje->tamanio_relleno > 0) {
        buffer_add(paquete->buffer, mensaje->relleno, mensaje->tamanio_relleno);  // sin prefijo: ya fue el tamanio
    }

    return paquete;
}

t_mensaje_prueba* mensaje_prueba_leer(t_buffer* buffer) {
    if (!alcanza(buffer, sizeof(uint8_t) + 2 * sizeof(uint32_t))) {
        fprintf(stderr, "mensaje_prueba_leer: payload corto para los campos fijos\n");
        return NULL;
    }

    t_mensaje_prueba* mensaje = malloc(sizeof(t_mensaje_prueba));
    mensaje->texto   = NULL;  // para que mensaje_prueba_destruir sea seguro si salimos por error
    mensaje->relleno = NULL;

    mensaje->origen          = buffer_read_uint8(buffer);
    mensaje->secuencia       = buffer_read_uint32(buffer);
    mensaje->tamanio_relleno = buffer_read_uint32(buffer);

    uint32_t longitud_texto = 0;  // espiamos la longitud antes de leer el string, para validarla
    memcpy(&longitud_texto, buffer->stream + buffer->offset, sizeof(uint32_t));

    if (longitud_texto == 0 || !alcanza(buffer, sizeof(uint32_t) + longitud_texto)) {
        fprintf(stderr, "mensaje_prueba_leer: longitud de texto invalida: %u\n", longitud_texto);
        mensaje_prueba_destruir(mensaje);
        return NULL;
    }

    mensaje->texto = buffer_read_string(buffer);
    mensaje->texto[longitud_texto - 1] = '\0';  // si el emisor mintio la longitud, igual queda cerrado

    if (mensaje->tamanio_relleno > 0) {
        if (!alcanza(buffer, mensaje->tamanio_relleno)) {
            fprintf(stderr, "mensaje_prueba_leer: faltan bytes de relleno: esperaba %u\n", mensaje->tamanio_relleno);
            mensaje_prueba_destruir(mensaje);
            return NULL;
        }

        mensaje->relleno = malloc(mensaje->tamanio_relleno);
        buffer_read(buffer, mensaje->relleno, mensaje->tamanio_relleno);
    }

    return mensaje;
}

void mensaje_prueba_loguear(t_log* logger, char* rotulo, t_mensaje_prueba* mensaje) {
    log_debug(logger, "%s | origen=%s secuencia=%u tamanio_relleno=%u texto=\"%s\" checksum_relleno=0x%08x",
              rotulo, modulo_to_string(mensaje->origen), mensaje->secuencia,
              mensaje->tamanio_relleno, mensaje->texto, checksum_relleno(mensaje));
}

bool mensaje_prueba_son_iguales(t_mensaje_prueba* enviado, t_mensaje_prueba* recibido) {
    if (enviado == NULL || recibido == NULL) return false;

    if (enviado->origen          != recibido->origen)          return false;
    if (enviado->secuencia       != recibido->secuencia)       return false;
    if (enviado->tamanio_relleno != recibido->tamanio_relleno) return false;
    if (strcmp(enviado->texto, recibido->texto) != 0)          return false;

    if (enviado->tamanio_relleno > 0 &&
        memcmp(enviado->relleno, recibido->relleno, enviado->tamanio_relleno) != 0) return false;

    return true;
}

int mensaje_prueba_responder_eco(int fd, t_mensaje_prueba* mensaje) {
    t_paquete* eco = mensaje_prueba_empaquetar(mensaje, MENSAJE_PRUEBA_ECO);

    int resultado = enviar_paquete(fd, eco);
    eliminar_paquete(eco);

    return resultado;
}

void mensaje_prueba_destruir(t_mensaje_prueba* mensaje) {
    if (mensaje == NULL) return;

    free(mensaje->texto);
    free(mensaje->relleno);
    free(mensaje);
}

bool mensaje_prueba_round_trip(int fd, pthread_mutex_t* mutex, t_log* logger, t_modulo origen,
                               t_modulo destino, uint32_t secuencia, uint32_t tamanio_relleno) {

    char* nombre = modulo_to_string(destino);

    t_mensaje_prueba* enviado = mensaje_prueba_crear(origen, secuencia, tamanio_relleno, "round-trip del Check 1");
    t_paquete* paquete = mensaje_prueba_empaquetar(enviado, MENSAJE_PRUEBA);

    char* rotulo = string_from_format("MENSAJE_PRUEBA enviado a %s", nombre);
    mensaje_prueba_loguear(logger, rotulo, enviado);
    free(rotulo);

    uint8_t op_code = 0;
    t_buffer* respuesta = NULL;

    // El envio y la espera del eco van sin soltar el lock: el protocolo admite una sola operacion en
    // vuelo por socket, y si se soltara entre medio dos hilos cruzarian las respuestas.
    if (mutex != NULL) pthread_mutex_lock(mutex);

    int resultado = enviar_paquete(fd, paquete);
    if (resultado == CONEXION_OK) resultado = recibir_operacion(fd, &op_code);
    if (resultado == CONEXION_OK) resultado = recibir_buffer(fd, &respuesta);

    if (mutex != NULL) pthread_mutex_unlock(mutex);

    eliminar_paquete(paquete);

    bool coincide = false;

    if (resultado != CONEXION_OK) {
        log_error(logger, "MENSAJE_PRUEBA con %s: se corto la comunicacion (resultado %d)", nombre, resultado);

    } else if (op_code != MENSAJE_PRUEBA_ECO) {
        log_error(logger, "MENSAJE_PRUEBA con %s: respondio op_code %u en vez de %u", nombre, op_code, MENSAJE_PRUEBA_ECO);

    } else {
        t_mensaje_prueba* recibido = mensaje_prueba_leer(respuesta);

        if (recibido == NULL) {
            log_error(logger, "El eco de %s vino mal formado", nombre);
        } else {
            rotulo = string_from_format("MENSAJE_PRUEBA_ECO recibido de %s", nombre);
            mensaje_prueba_loguear(logger, rotulo, recibido);
            free(rotulo);

            coincide = mensaje_prueba_son_iguales(enviado, recibido);

            if (!coincide) {
                log_error(logger, "El eco de %s no coincide con lo enviado", nombre);
            }

            mensaje_prueba_destruir(recibido);
        }
    }

    if (respuesta != NULL) eliminar_buffer(respuesta);
    mensaje_prueba_destruir(enviado);

    return coincide;
}
