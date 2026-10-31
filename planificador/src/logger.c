#include "logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <commons/string.h>
#include "inicializador.h"
#include "memoria_job.h"

// Solo lo usa el hilo del Logger
static t_log* reportes_logger;

void inicializar_reportes(void) {
    // Sin consola para no ensuciar la pantalla, y siempre en INFO: los reportes no dependen de LOG_LEVEL
    reportes_logger = log_create(config_planificador.path_reportes, "PLANIFICADOR", false, LOG_LEVEL_INFO);
    if (reportes_logger == NULL) {
        log_error(planificador_logger, "No se pudo abrir el archivo de reportes %s", config_planificador.path_reportes);
        exit(EXIT_FAILURE);
    }
}

void destruir_reportes(void) {
    log_destroy(reportes_logger);
}

// Cada byte como dos digitos hexa, separados por espacio: "48 6F 6C 61"
static char* bytes_a_hexa(unsigned char* bytes, uint32_t tamanio) {
    char* hexa = string_new();
    for (uint32_t i = 0; i < tamanio; i++) {
        string_append_with_format(&hexa, i == 0 ? "%02X" : " %02X", bytes[i]);
    }
    return hexa;
}

bool operar_logger(t_pedido_servicio* pedido) {
    uint32_t jid = pedido->pcb->jid;

    unsigned char* contenido = leer_memoria_job(jid, pedido->direcciones, pedido->tamanio);
    if (contenido == NULL) {
        return false;
    }

    char* hexa = bytes_a_hexa(contenido, pedido->tamanio);
    log_info(reportes_logger, LOG_REPORTE, jid, hexa);

    free(hexa);
    free(contenido);
    return true;
}
