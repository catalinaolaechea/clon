#include "labeler.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "inicializador.h"
#include "memoria_job.h"

// Una linea de stdin sin el salto final. NULL si se cerro la entrada.
static char* leer_linea(void) {
    char* linea = NULL;
    size_t capacidad = 0;
    if (getline(&linea, &capacidad, stdin) == -1) {
        free(linea);
        return NULL;
    }
    linea[strcspn(linea, "\n")] = '\0';
    return linea;
}

// Pide hasta tener al menos 'tamanio' caracteres. NULL si se cerro la entrada.
static char* pedir_caracteres(uint32_t jid, uint32_t tamanio) {
    while (1) {
        log_info(planificador_logger, LOG_PEDIDO_LABELER, jid, tamanio);
        printf("> ");
        fflush(stdout);  // sin salto de linea no sale solo

        char* linea = leer_linea();
        if (linea == NULL) {
            log_error(planificador_logger, "Labeler del Job %u: se cerro la entrada antes de recibir los caracteres", jid);
            return NULL;
        }

        size_t ingresados = strlen(linea);
        if (ingresados >= tamanio) {
            if (ingresados > tamanio) {
                log_warning(planificador_logger, "Labeler del Job %u: se ingresaron %zu caracteres, se usan los primeros %u", jid, ingresados, tamanio);
            }
            return linea;
        }

        log_warning(planificador_logger, "Labeler del Job %u: se ingresaron %zu de %u caracteres, se vuelve a pedir", jid, ingresados, tamanio);
        free(linea);
    }
}

bool operar_labeler(t_pedido_servicio* pedido) {
    uint32_t jid = pedido->pcb->jid;

    char* caracteres = pedir_caracteres(jid, pedido->tamanio);
    if (caracteres == NULL) {
        return false;
    }

    // Se escriben solo los primeros 'tamanio': lo que sobra y el '\0' no van a la memoria
    bool ok = escribir_memoria_job(jid, pedido->direcciones, caracteres);

    free(caracteres);
    return ok;
}
