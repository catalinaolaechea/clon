#include "servicios.h"
#include <stdlib.h>
#include <utils/direcciones.h>

void pedido_servicio_destruir(t_pedido_servicio* pedido) {
    free(pedido->archivo);  // free(NULL) no hace nada
    direcciones_destruir(pedido->direcciones);
    free(pedido);
}
