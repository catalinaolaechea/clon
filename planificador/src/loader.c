#include "loader.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <commons/string.h>
#include "inicializador.h"
#include "memoria_job.h"

// PATH_DATOS + nombre, con la barra si el config no la trae
static char* armar_ruta(char* archivo) {
    char* directorio = config_planificador.path_datos;
    bool tiene_barra = string_ends_with(directorio, "/");
    return string_from_format("%s%s%s", directorio, tiene_barra ? "" : "/", archivo);
}

static bool leer_lote(uint32_t jid, char* ruta, uint32_t tamanio, void* contenido) {
    FILE* lote = fopen(ruta, "rb");
    if (lote == NULL) {
        log_error(planificador_logger, "Loader del Job %u: no existe el archivo %s", jid, ruta);
        return false;
    }

    size_t leidos = fread(contenido, 1, tamanio, lote);
    fclose(lote);

    if (leidos < tamanio) {
        log_error(planificador_logger, "Loader del Job %u: %s tiene %zu bytes y se pidieron %u", jid, ruta, leidos, tamanio);
        return false;
    }

    return true;
}

bool operar_loader(t_pedido_servicio* pedido) {
    uint32_t jid = pedido->pcb->jid;

    usleep(config_planificador.retardo_loader * 1000);  // RETARDO_LOADER viene en ms

    char* ruta = armar_ruta(pedido->archivo);
    void* contenido = malloc(pedido->tamanio);
    bool ok = leer_lote(jid, ruta, pedido->tamanio, contenido)
           && escribir_memoria_job(jid, pedido->direcciones, contenido);

    free(ruta);
    free(contenido);
    return ok;
}
