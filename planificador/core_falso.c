// Cliente descartable para probar el servidor multihilo de Cores (PLN-03 / PLN-04).
// NO es parte del TP y no va en src/, para que el Makefile no lo levante como un
// segundo main. Se borra cuando el modulo Core real pueda hacer el handshake solo.
//
// Compilar, parado en planificador/ (sale a bin/, que ya esta en el .gitignore):
//   gcc -g -Wall -o bin/core_falso core_falso.c -I../utils/src -L../utils/lib -lutils -lcommons -lpthread -lreadline -lm
//
// Usar:
//   ./bin/core_falso <ip> <puerto> <identificador> [ambos|dispatch|interrupt]
//
// Por defecto abre LOS DOS canales, que es lo que hace un Core de verdad: dos
// conexiones al Planificador con el mismo identificador.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>

#include <utils/protocolo.h>
#include <utils/sockets.h>
#include <utils/serializacion.h>
#include <utils/handshake.h>

static char* nombre_canal(t_canal canal) {
    return canal == CANAL_DISPATCH ? "dispatch" : "interrupt";
}

// Devuelve el fd conectado y con handshake hecho, o -1.
static int abrir_canal(char* ip, char* puerto, char* identificador, t_canal canal) {

    int fd = crear_conexion(ip, puerto);

    if (fd == -1) {
        fprintf(stderr, "[%s/%s] no me pude conectar a %s:%s\n",
                identificador, nombre_canal(canal), ip, puerto);
        return -1;
    }

    if (enviar_handshake_core(fd, MODULO_CORE, canal, identificador) != CONEXION_OK) {
        fprintf(stderr, "[%s/%s] handshake rechazado\n", identificador, nombre_canal(canal));
        liberar_conexion(&fd);
        return -1;
    }

    printf("[%s/%s] handshake OK (socket %d)\n", identificador, nombre_canal(canal), fd);
    return fd;
}

int main(int argc, char* argv[]) {

    if (argc < 4 || argc > 5) {
        printf("Uso: %s <ip> <puerto> <identificador> [ambos|dispatch|interrupt]\n", argv[0]);
        return EXIT_FAILURE;
    }

    char* ip            = argv[1];
    char* puerto        = argv[2];
    char* identificador = argv[3];
    char* modo          = argc == 5 ? argv[4] : "ambos";

    bool quiere_dispatch  = strcmp(modo, "interrupt") != 0;
    bool quiere_interrupt = strcmp(modo, "dispatch")  != 0;

    int fd_dispatch  = -1;
    int fd_interrupt = -1;

    if (quiere_dispatch) {
        fd_dispatch = abrir_canal(ip, puerto, identificador, CANAL_DISPATCH);
        if (fd_dispatch == -1) return EXIT_FAILURE;
    }

    if (quiere_interrupt) {
        fd_interrupt = abrir_canal(ip, puerto, identificador, CANAL_INTERRUPT);
        if (fd_interrupt == -1) {
            liberar_conexion(&fd_dispatch);
            return EXIT_FAILURE;
        }
    }

    // Payload vacio a proposito: atender_mensaje todavia no llama a recibir_buffer,
    // asi que mandarle datos desincronizaria el stream. Cuando SIS-31 lea el payload,
    // aca se le agregan los buffer_add_* que hagan falta.
    if (fd_dispatch != -1) {
        t_paquete* paquete = crear_paquete(MENSAJE_PRUEBA);
        if (enviar_paquete(fd_dispatch, paquete) != CONEXION_OK) {
            fprintf(stderr, "[%s] no pude mandar el MENSAJE_PRUEBA\n", identificador);
        }
        eliminar_paquete(paquete);
    }

    printf("[%s] conectado y esperando. Ctrl-C, kill o kill -9 para desconectar.\n", identificador);

    // Se queda vivo sin mandar nada mas: es lo que permite tener N Cores conectados
    // al mismo tiempo, que es el criterio de aceptacion del issue.
    pause();

    liberar_conexion(&fd_dispatch);
    liberar_conexion(&fd_interrupt);
    return EXIT_SUCCESS;
}
