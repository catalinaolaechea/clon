#include "cliente_placa.h"
#include "inicializador.h"

// TODO(SIS-43): reemplazar por CREAR_JOB real
bool placa_crear_job(uint32_t jid, char* archivo_pseudocodigo) {
    log_debug(planificador_logger, "[MOCK PLACA] CREAR_JOB JID %u - archivo %s", jid, archivo_pseudocodigo);
    return true;
}