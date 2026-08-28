#ifndef SOCKETS_H_
#define SOCKETS_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>

// Las que devuelven un fd devuelven -1 ante error, con el detalle en stderr.
int  iniciar_servidor(char* puerto);
int  esperar_cliente(int socket_servidor);
int  crear_conexion(char* ip, char* puerto);
void liberar_conexion(int* socket);

#endif
