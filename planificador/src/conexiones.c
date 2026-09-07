#include "conexiones.h"
#include "inicializador.h"
#include <errno.h>
#include <string.h>

// Los fd arrancan en -1 (el mismo valor que deja liberar_conexion) para que un
// uso antes del arranque falle de entrada en vez de escribir sobre el fd 0.
t_conexiones_planificador conexiones_planificador = {
    .fd_placa      = -1,
    .mutex_placa   = PTHREAD_MUTEX_INITIALIZER,
    .fd_storage    = -1,
    .mutex_storage = PTHREAD_MUTEX_INITIALIZER
};

// Hace un solo intento y devuelve el error ya logueado: la politica de que hacer
// ante un fallo (abortar o reintentar) queda en manos del que llama, que es el
// que sabe si puede seguir sin esta conexion.
int conectar_a_modulo(char* ip, char* puerto, t_modulo modulo_destino) {

    char* nombre_destino = modulo_to_string(modulo_destino);

    int fd = crear_conexion(ip, puerto);

    if (fd == ERROR_CONEXION) {
        // crear_conexion ya dejo el detalle exacto en stderr; errno queda seteado
        // por connect(), que es el caso tipico (modulo apagado -> ECONNREFUSED).
        log_error(planificador_logger, "No se pudo conectar con %s (%s:%s): %s",
                  nombre_destino, ip, puerto, strerror(errno));
        return ERROR_CONEXION;
    }

    int resultado = enviar_handshake(fd, MODULO_PLANIFICADOR, CANAL_UNICO);

    if (resultado != CONEXION_OK) {
        // El socket se abrio bien: el problema es de protocolo, no del SO, asi
        // que errno no aplica y logueamos el codigo que devolvio el handshake.
        log_error(planificador_logger, "Handshake rechazado por %s (%s:%s), resultado %d",
                  nombre_destino, ip, puerto, resultado);
        liberar_conexion(&fd);
        return ERROR_CONEXION;
    }

    log_info(planificador_logger, LOG_CONEXION_ESTABLECIDA, nombre_destino);

    return fd;
}
