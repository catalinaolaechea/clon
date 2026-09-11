#ifndef CONEXIONES_H_
#define CONEXIONES_H_
 
#include "inicializador.h"
#include <utils/handshake.h>
#include <utils/mensaje_prueba.h>

extern int fd_planificador;
extern int fd_placa;

void conectar_a_planificador_y_placa();

// Round-trip de MENSAJE_PRUEBA contra Planificador y Placa: manda, espera el eco y compara campo por
// campo. Devuelve false si alguno de los cuatro intercambios no volvio identico.
bool probar_round_trip(void);
 
#endif