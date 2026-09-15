#include "dispatch.h"

void esperar_dispatch(void) {
    //entro a dispatch 
    log_info(core_logger, "Core %d: esperando dispatch del Planificador (bloqueado)", identificador);

    while (1) {
        uint8_t op_code;
        int resultado = recibir_operacion(fd_planificador, &op_code);

        if (resultado == CONEXION_DESCONECTADO) {
            log_info(core_logger, "Core %d: el Planificador cerro la conexion de dispatch", identificador);
            return;
        }

        if (resultado != CONEXION_OK) {
            log_error(core_logger, "Core %d: error esperando al Planificador (dispatch)", identificador);
            return;
        }
        //llega algo de verdad, lo lee
        t_buffer* buffer;
        int resultado_buffer = recibir_buffer(fd_planificador, &buffer);

        if (resultado_buffer != CONEXION_OK) {
            log_error(core_logger, "Core %d: error recibiendo el payload del Planificador", identificador);
            return;
        }

        switch (op_code) {
            // TODO (Check 2): DISPATCH_JOB -> recibir JID + contexto, arrancar el ciclo de instruccion
            default:
                log_warning(core_logger, "Core %d: op_code %u sin manejar todavia en dispatch", identificador, op_code);
        }

        eliminar_buffer(buffer);
    }
}