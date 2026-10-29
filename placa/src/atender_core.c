#include "atender_core.h"
#include "jobs.h"
#include "memoria.h"

t_list* lista_core = NULL;

pthread_mutex_t mutex_lista_core = PTHREAD_MUTEX_INITIALIZER;


// [Enunciado pág. 22-23 - Operaciones]
// Pedidos que llegan desde un Core: obtener una instrucción (Fetch), obtención de marco (MMU),
// lectura y escritura de datos, y deslockeo de páginas.
void* atender_core(void* args){

    t_core_placa* core = (t_core_placa*) args;

    int socket_cliente = core->socket_core;

    while(1){
        uint8_t operacion_core;
        int codigo_validacion = recibir_operacion(socket_cliente,&operacion_core);


        if (codigo_validacion == CONEXION_ERROR) {
            log_error(placa_logger, "Operación desconocida recibida de CORE %d: %d", core->id_core, operacion_core);
            break;
        }

        if (codigo_validacion == CONEXION_DESCONECTADO) {
            log_info(placa_logger, " ## Core %d se ha desconectado", core->id_core);
            break;
        }

        // todo mensaje viaja como [op_code][tamanio][payload], asi que hay que consumir el payload
        // si o si, aunque el case todavia no lo use: si queda en el socket, la proxima vuelta lee el
        // primer byte del tamanio creyendo que es un op_code y el protocolo se desincroniza
        t_buffer* payload;
        codigo_validacion = recibir_buffer(socket_cliente, &payload);

        if (codigo_validacion != CONEXION_OK) {
            log_error(placa_logger, "Error al recibir el payload del op_code %d del CORE %d", operacion_core, core->id_core);
            break;
        }

        switch (operacion_core)
        {
        case MENSAJE_PRUEBA:{
            log_info(placa_logger, "Recibido MENSAJE_PRUEBA del Core con identificador: %d", core->id_core);

            t_mensaje_prueba* mensaje = mensaje_prueba_leer(payload);

            if (mensaje == NULL) {
                log_error(placa_logger, "MENSAJE_PRUEBA mal formado del Core %d", core->id_core);
                break;
            }

            mensaje_prueba_loguear(placa_logger, "MENSAJE_PRUEBA recibido del Core", mensaje);

            if (mensaje_prueba_responder_eco(socket_cliente, mensaje) != CONEXION_OK) {
                log_error(placa_logger, "No se pudo responder el eco al Core %d", core->id_core);
            }

            mensaje_prueba_destruir(mensaje);
            break;

        }
        // [Enunciado pág. 16 - Core / Fetch] + [pág. 21 - Memoria de Instrucciones]
        // El Core pide cada instrucción con el Program Counter, que es el número de instrucción
        // relativo al Job (PC = 0 es la primera línea). La Placa responde la instrucción de ese
        // Job; el enunciado no impone una forma específica de obtenerla.
        // Si el JID no existe o el PC se pasa del final, se responde FETCH_ERROR (decisión del
        // grupo, SIS-40) en vez de devolver basura.
        // Log obligatorio (pág. 24): "## JID: <JID> - Obtener instrucción: <PC> - Instrucción: <INSTRUCCIÓN> <...ARGS>"
        case FETCH_INSTRUCCION:{

            uint32_t jid = buffer_read_uint32(payload);
            uint32_t pc = buffer_read_uint32(payload);

            int motivo_error = -1;

            char* instruccion = obtener_instruccion(jid, pc, &motivo_error);

            retardo_memoria();

            t_paquete* respuesta;

            if(instruccion != NULL){
                log_info(placa_logger, "## JID: %u - Obtener instrucción: %u - Instrucción: %s", jid,pc,instruccion);
                respuesta = crear_paquete(RESPUESTA_INSTRUCCION);
                buffer_add_string(respuesta->buffer, instruccion);
                free(instruccion);
            }else {
                log_error(placa_logger, "Fetch inválido - JID: %u - PC: %u - Motivo: %d", jid, pc, motivo_error);

                respuesta = crear_paquete(FETCH_ERROR);
                buffer_add_uint32(respuesta->buffer, motivo_error);

            }

            int validacion = enviar_paquete(socket_cliente, respuesta);

            if(validacion != CONEXION_OK){
                log_error(placa_logger, "No se pudo responder el fetch al core %d", core->id_core);
            }

            eliminar_paquete(respuesta);
            break;

        }

        case OBTENER_TAM_PAGINA:{
            t_paquete* respuesta = crear_paquete(RESPUESTA_TAM_PAGINA);
            buffer_add_uint32(respuesta->buffer, TAM_PAGINA);

            if(enviar_paquete(socket_cliente, respuesta) != CONEXION_OK)
                log_error(placa_logger,"No se pudo enviar el tamaño de pagina al Core %d", core->id_core);

            eliminar_paquete(respuesta);

            break;
        }

        // [Enunciado pág. 23 - Lectura de datos] (Check 3)
        // Puede llegar del Core o del Planificador: JID, dirección física y tamaño a leer.
        // Se responde el contenido del rango pedido.
        // [Enunciado pág. 23 - Escritura de datos] (Check 3)
        // Mismos parámetros que la lectura, más el contenido a escribir.
        // Log obligatorio (pág. 24): "## JID: <JID> - <Lectura/Escritura> - Dir. Física: <DIRECCION_FISICA> - Tamaño: <TAMAÑO>"
        /*case LEER_DATO:{

            break;

        }
        case ESCRIBIR_DATO:{

            break;
        }

        */

        // [Enunciado pág. 23 - Obtención de marco] (Check 3)
        // La pide el Core durante la traducción de la MMU, con el JID y el número de página.
        // Se consulta la tabla de páginas del Job:
        //  - si la página está residente, se responde el número de marco y la página queda lockeada;
        //  - si no está residente, se responde que ocurrió un Page Fault.
        case OBTENER_MARCO:{

            uint32_t jid    = buffer_read_uint32(payload);
            uint32_t pagina = buffer_read_uint32(payload);

            uint32_t marco = 0;      
            int motivo     = -1;     

            uint8_t op_respuesta = obtener_marco(jid, pagina, &marco, &motivo);

            retardo_memoria();      

            t_paquete* respuesta = crear_paquete(op_respuesta);

            if(op_respuesta == RESPUESTA_MARCO){
                // la página está en memoria (y quedó lockeada): se manda su marco
                buffer_add_uint32(respuesta->buffer, marco);

            } else if(op_respuesta == PAGE_FAULT){
                // no está en memoria: el contrato pide jid y página
                buffer_add_uint32(respuesta->buffer, jid);
                buffer_add_uint32(respuesta->buffer, pagina);

            } else {
                // MARCO_ERROR: el motivo viaja adentro, igual que en FETCH_ERROR
                log_error(placa_logger, "Obtener marco inválido - JID: %u - Página: %u - Motivo: %d", jid, pagina, motivo);
                buffer_add_uint32(respuesta->buffer, motivo);
            }

            int validar = enviar_paquete(socket_cliente, respuesta);

            if(validar != CONEXION_OK){
                log_error(placa_logger, "No se pudo responder OBTENER_MARCO al Core %d", core->id_core);
            }

            eliminar_paquete(respuesta);
            break;
        }

        // [Enunciado pág. 23 - Deslockeo de páginas] (Check 3)
        // El Core la envía al terminar con éxito cada ciclo de instrucción, o al devolverle el Job
        // al Planificador (por ejemplo, tras un Page Fault). Se recibe el JID y se liberan los
        // locks de TODAS sus páginas, para que sus marcos vuelvan a poder ser víctimas de reemplazo.
        case DESLOCKEAR_PAGINAS:{

            uint32_t jid = buffer_read_uint32(payload);

            if(!deslockear_paginas(jid))
                log_warning(placa_logger, "DESLOCKEAR PAGINAS de un JID inexistente %u",jid);
                
            retardo_memoria();

            t_paquete* respuesta = crear_paquete(DESLOCKEO_OK);
            
            int validar = enviar_paquete(socket_cliente,respuesta);

            if(validar != CONEXION_OK){
                log_error(placa_logger,"No se pudo responder DESLOCKEO_OK al core %d", core->id_core);
            }

            eliminar_paquete(respuesta);

            break;

        }

        default:
            //log_warning(placa_logger, "Operación desconocida recibida de CORE: %d",operacion_core);
            break;

        }

        eliminar_buffer(payload);

    }

    // saco al core de la lista y libero, UNA sola vez, acá afuera del while
    pthread_mutex_lock(&mutex_lista_core);
    list_remove_element(lista_core, core);
    pthread_mutex_unlock(&mutex_lista_core);

    liberar_conexion(&socket_cliente);
    //free(core->id_core);
    free(core);

    return NULL;

}
