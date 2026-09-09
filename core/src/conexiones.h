#ifndef CONEXIONES_H_
#define CONEXIONES_H_
 
#include "inicializador.h"
#include <utils/handshake.h>
 
extern int fd_planificador;
extern int fd_placa;
 
void conectar_a_planificador_y_placa();
 
#endif