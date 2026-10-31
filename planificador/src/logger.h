#ifndef LOGGER_H
#define LOGGER_H

#include <stdbool.h>
#include "servicios.h"

#define LOG_REPORTE "## (%u) - %s"

// Abre PATH_REPORTES, aparte del log del modulo. Antes de arrancar el hilo del Logger.
void inicializar_reportes(void);
void destruir_reportes(void);

// REPORT: lee 'tamanio' bytes de la memoria del Job y los escribe en hexa en PATH_REPORTES.
// false si falla la lectura: el Job se finaliza.
bool operar_logger(t_pedido_servicio* pedido);

#endif
