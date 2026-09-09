#include "sockets.h"

// El detalle tecnico va a stderr y se devuelve -1; el modulo que llama decide
// que loguear en su archivo y si aborta.

int iniciar_servidor(char* puerto) {
    struct addrinfo hints;
    struct addrinfo* servinfo;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    int rc = getaddrinfo(NULL, puerto, &hints, &servinfo);
    if (rc != 0) {
        fprintf(stderr, "iniciar_servidor: getaddrinfo(%s): %s\n", puerto, gai_strerror(rc));
        return -1;
    }

    int socket_servidor = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
    if (socket_servidor == -1) {
        fprintf(stderr, "iniciar_servidor: socket: %s\n", strerror(errno));
        freeaddrinfo(servinfo);
        return -1;
    }

    // Sin esto el puerto queda en TIME_WAIT ~60s cada vez que muere el proceso.
    if (setsockopt(socket_servidor, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) == -1) {
        fprintf(stderr, "iniciar_servidor: setsockopt: %s\n", strerror(errno));
        close(socket_servidor);
        freeaddrinfo(servinfo);
        return -1;
    }

    if (bind(socket_servidor, servinfo->ai_addr, servinfo->ai_addrlen) == -1) {
        fprintf(stderr, "iniciar_servidor: bind(%s): %s\n", puerto, strerror(errno));
        close(socket_servidor);
        freeaddrinfo(servinfo);
        return -1;
    }

    if (listen(socket_servidor, SOMAXCONN) == -1) {
        fprintf(stderr, "iniciar_servidor: listen: %s\n", strerror(errno));
        close(socket_servidor);
        freeaddrinfo(servinfo);
        return -1;
    }

    freeaddrinfo(servinfo);
    return socket_servidor;
}

int esperar_cliente(int socket_servidor) {
    while (1) {
        int socket_cliente = accept(socket_servidor, NULL, NULL);
        if (socket_cliente == -1) {
            if (errno == EINTR) continue;  // si nos interrumpieron con una señal, reintentamos
            fprintf(stderr, "esperar_cliente: accept: %s\n", strerror(errno));
            return -1;
        }
        return socket_cliente;
    }
}

int crear_conexion(char* ip, char* puerto) {
    struct addrinfo hints;
    struct addrinfo* server_info;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    int rc = getaddrinfo(ip, puerto, &hints, &server_info);
    if (rc != 0) {
        fprintf(stderr, "crear_conexion: getaddrinfo(%s:%s): %s\n", ip, puerto, gai_strerror(rc));
        return -1;
    }

    int socket_cliente = socket(server_info->ai_family, server_info->ai_socktype, server_info->ai_protocol);
    if (socket_cliente == -1) {
        fprintf(stderr, "crear_conexion: socket: %s\n", strerror(errno));
        freeaddrinfo(server_info);
        return -1;
    }

    if (connect(socket_cliente, server_info->ai_addr, server_info->ai_addrlen) == -1) {
        fprintf(stderr, "crear_conexion: connect(%s:%s): %s\n", ip, puerto, strerror(errno));
        close(socket_cliente);
        freeaddrinfo(server_info);
        return -1;
    }

    freeaddrinfo(server_info);
    return socket_cliente;
}

void liberar_conexion(int* socket) {
    if (socket == NULL || *socket == -1) return;
    close(*socket);
    *socket = -1;
}
