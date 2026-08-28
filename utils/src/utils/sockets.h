#ifndef SOCKETS_H_
#define SOCKETS_H_

#include<stdio.h>
#include<stdlib.h>
#include<sys/socket.h>
#include<unistd.h>
#include<netdb.h>
#include<commons/log.h>
#include<string.h>

int iniciar_servidor(char* puerto);
int esperar_cliente(int);
int crear_conexion(char* ip, char* puerto);
void liberar_conexion(int socket_cliente);


#endif


