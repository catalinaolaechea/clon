#include "conexiones.h"
#include "inicializador.h"
#include <errno.h>
#include <string.h>
#include <unistd.h>

#define INTENTOS_CONEXION 3                    // tolera que Placa o Storage tarden en levantar
#define ESPERA_ENTRE_INTENTOS_US (500 * 1000)  // 500 ms, 1 segundo de tolerancia total

t_conexiones_planificador conexiones_planificador = {
    .fd_placa      = -1,  // -1 y no 0, que es un fd valido (stdin)
    .mutex_placa   = PTHREAD_MUTEX_INITIALIZER,
    .fd_storage    = -1,
    .mutex_storage = PTHREAD_MUTEX_INITIALIZER
};

int conectar_a_modulo(char* ip, char* puerto, t_modulo modulo_destino) {

    char* nombre_destino = modulo_to_string(modulo_destino);

    int fd = crear_conexion(ip, puerto);

    if (fd == ERROR_CONEXION) {
        log_error(planificador_logger, "No se pudo conectar con %s (%s:%s): %s",
                  nombre_destino, ip, puerto, strerror(errno));  // errno lo dejo connect()
        return ERROR_CONEXION;
    }

    int resultado = enviar_handshake(fd, MODULO_PLANIFICADOR, CANAL_UNICO, NULL);

    if (resultado != CONEXION_OK) {
        log_error(planificador_logger, "Handshake rechazado por %s (%s:%s), resultado %d",
                  nombre_destino, ip, puerto, resultado);  // fallo de protocolo: errno no aplica
        liberar_conexion(&fd);  // sin esto el fd queda filtrado
        return ERROR_CONEXION;
    }

    log_info(planificador_logger, LOG_CONEXION_ESTABLECIDA, nombre_destino);

    return fd;
}

static int conectar_con_reintentos(char* ip, char* puerto, t_modulo modulo_destino) {

    char* nombre_destino = modulo_to_string(modulo_destino);

    for (int intento = 1; intento <= INTENTOS_CONEXION; intento++) {

        int fd = conectar_a_modulo(ip, puerto, modulo_destino);  // reintenta conexion + handshake

        if (fd != ERROR_CONEXION) return fd;

        if (intento < INTENTOS_CONEXION) {
            log_warning(planificador_logger, "Reintentando conexion con %s (intento %d de %d)",
                        nombre_destino, intento + 1, INTENTOS_CONEXION);
            usleep(ESPERA_ENTRE_INTENTOS_US);
        }
    }

    return ERROR_CONEXION;
}

static int conectar_o_abortar(char* ip, char* puerto, t_modulo modulo_destino) {

    int fd = conectar_con_reintentos(ip, puerto, modulo_destino);

    if (fd == ERROR_CONEXION) {
        log_error(planificador_logger, "No se pudo establecer la conexion con %s (%s:%s) tras %d intentos, se aborta el arranque",
                  modulo_to_string(modulo_destino), ip, puerto, INTENTOS_CONEXION);
        exit(EXIT_FAILURE);  // arrancar a medias esconde el error hasta la primera syscall
    }

    return fd;
}

void inicializar_conexiones(void) {

    conexiones_planificador.fd_placa = conectar_o_abortar(  // Placa primero, como pide el enunciado
        config_planificador.ip_placa, config_planificador.puerto_placa, MODULO_PLACA);

    conexiones_planificador.fd_storage = conectar_o_abortar(  // y recien despues Storage
        config_planificador.ip_storage, config_planificador.puerto_storage, MODULO_STORAGE);
}
