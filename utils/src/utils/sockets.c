#include "sockets.h"

//cliente



int crear_conexion(char *ip, char* puerto)
{
	struct addrinfo hints;
	struct addrinfo *server_info;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	getaddrinfo(ip, puerto, &hints, &server_info);

	// Ahora vamos a crear el socket.
	int socket_cliente = socket(
		server_info->ai_family,
		server_info->ai_socktype,
		server_info->ai_protocol
	);

	// Ahora que tenemos el socket, vamos a conectarlo
	connect(socket_cliente,server_info->ai_addr,server_info->ai_addrlen);
	
	freeaddrinfo(server_info);

	return socket_cliente;
}


void liberar_conexion(int socket_cliente)
{
	close(socket_cliente);
}

//servidor

t_log* logger;

int iniciar_servidor(void)
{
	int socket_servidor;
	struct addrinfo hints, *servinfo;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	getaddrinfo(NULL, PUERTO, &hints, &servinfo);
	socket_servidor = socket(
		servinfo->ai_family,
		servinfo->ai_socktype,
		servinfo->ai_protocol
	);

	// Creamos el socket de escucha del servidor
	setsockopt(socket_servidor, SOL_SOCKET,SO_REUSEPORT, &(int){1},sizeof(int));
	
	// Asociamos el socket a un puerto
	bind(socket_servidor,servinfo->ai_addr, servinfo->ai_addrlen);

	// Escuchamos las conexiones entrantes
	listen(socket_servidor,SOMAXCONN);

	freeaddrinfo(servinfo);
	log_trace(logger, "Listo para escuchar a mi cliente");

	return socket_servidor;
}

int esperar_cliente(int socket_servidor)
{

	// Aceptamos un nuevo cliente
	int socket_cliente = accept(socket_servidor,NULL,NULL);

	log_info(logger, "Se conecto un cliente!");

	return socket_cliente;
}
