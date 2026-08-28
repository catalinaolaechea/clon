#ifndef CONEXION_H_
#define CONEXION_H_

#include<stdio.h>
#include<stdlib.h>
#include<sys/socket.h>
#include<unistd.h>
#include<netdb.h>
#include<commons/log.h>
#include<string.h>

typedef struct
{
	op_code codigo_operacion;
	t_buffer* buffer;
} t_paquete;


//cliente
int iniciar_servidor(void);
int esperar_cliente(int);

//servidor
int crear_conexion(char* ip, char* puerto);
void liberar_conexion(int socket_cliente);


#endif


