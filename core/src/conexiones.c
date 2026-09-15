#include "conexiones.h"
#include <errno.h>
#include <string.h>
#include <stdlib.h>

int fd_planificador;
int fd_placa;
int fd_interrupt;

// Conecta a un solo modulo servidor, hace el handshake, y loguea. Si algo falla, aborta el proceso.
static void conectar_a_modulo(int* fd_destino, char* ip, char* puerto, t_modulo modulo_servidor, t_canal canal) {

    int fd = crear_conexion(ip, puerto);
    if (fd == -1) {
        log_error(core_logger, "No se pudo conectar a %s (%s:%s): %s", modulo_to_string(modulo_servidor), ip, puerto, strerror(errno));
        exit(EXIT_FAILURE);
    }

    if (enviar_handshake(fd, MODULO_CORE, canal, identificador) != CONEXION_OK) {
        log_error(core_logger, "Handshake rechazado por %s (canal %d)", modulo_to_string(modulo_servidor), canal);
        exit(EXIT_FAILURE);
    }

    log_info(core_logger, LOG_CONEXION_ESTABLECIDA, modulo_to_string(modulo_servidor));

    *fd_destino = fd;
}

void conectar_a_planificador_y_placa() {
    conectar_a_modulo(&fd_planificador, configuracion.ip_planificador, configuracion.puerto_planificador, MODULO_PLANIFICADOR, CANAL_DISPATCH);
    conectar_a_modulo(&fd_placa, configuracion.ip_placa, configuracion.puerto_placa, MODULO_PLACA, CANAL_UNICO);
}

void conectar_canal_interrupt() {
    conectar_a_modulo(&fd_interrupt, configuracion.ip_planificador, configuracion.puerto_planificador, MODULO_PLANIFICADOR, CANAL_INTERRUPT);
}

bool probar_round_trip(void) {

    struct { int fd; t_modulo modulo; } destinos[] = {
        { fd_planificador, MODULO_PLANIFICADOR },
        { fd_placa,        MODULO_PLACA        }
    };


    uint32_t tamanios[] = { PRUEBA_RELLENO_CHICO, PRUEBA_RELLENO_GRANDE };

    uint32_t secuencia = 0;
    bool todos_ok = true;

    for (size_t d = 0; d < sizeof(destinos) / sizeof(destinos[0]); d++) {
        for (size_t t = 0; t < sizeof(tamanios) / sizeof(tamanios[0]); t++) {

            secuencia++;
            bool ok = mensaje_prueba_round_trip(destinos[d].fd, NULL, core_logger, MODULO_CORE, destinos[d].modulo, secuencia, tamanios[t]);

            log_info(core_logger, "Core %d - Round-trip con %s (relleno %u B): %s",
                     identificador, modulo_to_string(destinos[d].modulo), tamanios[t], ok ? "OK" : "FALLO");

            todos_ok = todos_ok && ok;
        }
    }

    return todos_ok;
}