#include "conexiones.h"
#include <errno.h>
#include <string.h>
#include <stdlib.h>
 
int fd_planificador;
int fd_placa;
 
// Conecta a un solo modulo servidor, hace el handshake, y loguea. Si algo falla, aborta el proceso.
static void conectar_a_modulo(int* fd_destino, char* ip, char* puerto, t_modulo modulo_servidor) {
 
    int fd = crear_conexion(ip, puerto);
    if (fd == -1) {
        log_error(core_logger, "No se pudo conectar a %s (%s:%s): %s",
                  modulo_to_string(modulo_servidor), ip, puerto, strerror(errno));
        exit(EXIT_FAILURE);
    }
 
    
    int id_numerico = atoi(identificador);
 
    t_canal canal = (modulo_servidor == MODULO_PLANIFICADOR) ? CANAL_DISPATCH : CANAL_UNICO;
 
    if (enviar_handshake(fd, MODULO_CORE, canal, id_numerico) != CONEXION_OK) {
        log_error(core_logger, "Handshake rechazado por %s", modulo_to_string(modulo_servidor));
        exit(EXIT_FAILURE);
    }
 
    log_info(core_logger, LOG_CONEXION_ESTABLECIDA, modulo_to_string(modulo_servidor));
 
    *fd_destino = fd;
}
 
void conectar_a_planificador_y_placa() {
    conectar_a_modulo(&fd_planificador, configuracion.ip_planificador, configuracion.puerto_planificador, MODULO_PLANIFICADOR);
    conectar_a_modulo(&fd_placa, configuracion.ip_placa, configuracion.puerto_placa, MODULO_PLACA);
}
 