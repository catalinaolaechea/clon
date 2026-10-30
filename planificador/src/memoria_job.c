#include "memoria_job.h"
#include <stdlib.h>
#include <utils/direcciones.h>
#include "inicializador.h"
#include "cliente_placa.h"

void* leer_memoria_job(uint32_t jid, t_list* direcciones, uint32_t tamanio) {
    void* contenido = malloc(tamanio);
    uint32_t desplazamiento = 0;

    for (int i = 0; i < list_size(direcciones); i++) {
        t_direccion_fisica* pedazo = list_get(direcciones, i);

        if (desplazamiento + pedazo->tamanio > tamanio) {
            log_error(planificador_logger, "Los pedazos del JID %u suman mas que los %u bytes pedidos", jid, tamanio);
            free(contenido);
            return NULL;
        }

        if (!placa_leer_memoria(jid, pedazo, (char*) contenido + desplazamiento)) {
            free(contenido);
            return NULL;
        }

        desplazamiento += pedazo->tamanio;
    }

    if (desplazamiento != tamanio) {
        log_error(planificador_logger, "Los pedazos del JID %u suman %u bytes y se pidieron %u", jid, desplazamiento, tamanio);
        free(contenido);
        return NULL;
    }

    return contenido;
}

bool escribir_memoria_job(uint32_t jid, t_list* direcciones, void* datos) {
    uint32_t desplazamiento = 0;

    for (int i = 0; i < list_size(direcciones); i++) {
        t_direccion_fisica* pedazo = list_get(direcciones, i);

        if (!placa_escribir_memoria(jid, pedazo, (char*) datos + desplazamiento)) {
            return false;
        }

        desplazamiento += pedazo->tamanio;
    }

    return true;
}
