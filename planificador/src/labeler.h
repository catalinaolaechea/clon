#ifndef LABELER_H
#define LABELER_H

#include <stdbool.h>
#include "servicios.h"

#define LOG_PEDIDO_LABELER "## (%u) - Ingrese %u caracteres:"

// LABEL: pide 'tamanio' caracteres por teclado y los escribe en la memoria del Job.
// Si sobran se cortan, si faltan se vuelven a pedir.
// false si se cierra la entrada o falla la escritura: el Job se finaliza.
bool operar_labeler(t_pedido_servicio* pedido);

#endif
