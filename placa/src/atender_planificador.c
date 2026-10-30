#include "atender_planificador.h"
#include "jobs.h"
#include "memoria.h"

// [Enunciado pág. 22-23 - Operaciones]
// Pedidos que llegan desde el Planificador: creación y finalización de Job, asignación y liberación
// de espacio (ALLOC/FREE), carga de página tras un Page Fault, lectura y escritura de datos de las
// syscalls de memoria, y deslockeo de páginas al terminar esas syscalls.
void* atender_planificador(void* planificador){

    int socket_cliente = *(int*) planificador;
    free(planificador);

    while(1){
        uint8_t operacion_planificador;
        int codigo_validacion = recibir_operacion(socket_cliente,&operacion_planificador);

        if (codigo_validacion == CONEXION_ERROR) {
            log_error(placa_logger, "Operación desconocida recibida del PLANIFICADOR: %d", operacion_planificador);
            break;
        }

        if (codigo_validacion == CONEXION_DESCONECTADO) {
            log_info(placa_logger, "## Planificador desconectado");
            break;
        }

        // mismo motivo que en atender_core: el payload se consume siempre, lo use o no el case
        t_buffer* payload;
        codigo_validacion = recibir_buffer(socket_cliente, &payload);

        if (codigo_validacion != CONEXION_OK) {
            log_error(placa_logger, "Error al recibir el payload del op_code %d del PLANIFICADOR", operacion_planificador);
            break;
        }

        switch (operacion_planificador)
        {
        case MENSAJE_PRUEBA:{
            log_info(placa_logger, "Recibido MENSAJE_PRUEBA del Planificador");

            t_mensaje_prueba* mensaje = mensaje_prueba_leer(payload);

            if (mensaje == NULL) {
                log_error(placa_logger, "MENSAJE_PRUEBA mal formado del Planificador");
                break;
            }

            mensaje_prueba_loguear(placa_logger, "MENSAJE_PRUEBA recibido del Planificador", mensaje);

            t_paquete* eco = mensaje_prueba_empaquetar(mensaje, MENSAJE_PRUEBA_ECO);
            int resultado = enviar_paquete(socket_cliente, eco);
            eliminar_paquete(eco);

            if (resultado != CONEXION_OK) {
                log_error(placa_logger, "No se pudo responder el eco al Planificador");
            }

            mensaje_prueba_destruir(mensaje);
            break;

        }
        // [Enunciado pág. 22 - Creación de Job]
        // Se recibe un JID y el nombre de un archivo de pseudocódigo, relativo al path base del
        // config (PATH_INSTRUCCIONES). Con esos datos se crean las estructuras administrativas del Job.
        // Paginación bajo demanda (pág. 21): el Job arranca SIN marcos asignados.
        // Respuesta: CREAR_JOB_OK, o CREAR_JOB_ERROR si no se pudo abrir el archivo (SIS-43).
        // Log obligatorio (pág. 24): "## JID: <JID> - Job Creado"
        case CREAR_JOB:{

            uint32_t jid = buffer_read_uint32(payload);
            char* archivo = buffer_read_string(payload);

            t_job_placa* job = inicializar_job(jid, archivo);

            free(archivo);

            t_paquete* respuesta;
            if(job != NULL){
                log_info(placa_logger, "## JID: %u - Job Creado",jid);
                respuesta = crear_paquete(CREAR_JOB_OK);

            }else {
                respuesta = crear_paquete(CREAR_JOB_ERROR);
            }

            retardo_memoria();

            if(enviar_paquete(socket_cliente, respuesta) != CONEXION_OK){
                log_error(placa_logger, "No se pudo responder CREAR_JOB al Planificador");
            }

            eliminar_paquete(respuesta);


            break;

        }

        // [Enunciado pág. 22 - Asignación de espacio] (Check 3)
        // Se recibe el JID y el tamaño en bytes; se responde la dirección lógica donde empieza el
        // espacio asignado. Cada asignación arranca al inicio de una página. Hay que reservar en
        // el Offload el lugar para respaldar esas páginas: si no alcanza, la operación falla y se
        // le avisa al Planificador para que finalice el Job.
        // [Enunciado pág. 22 - Liberación de espacio] (Check 3)
        // Se recibe el JID y la dirección lógica donde empieza un espacio asignado antes. Se liberan
        // los marcos de sus páginas y el lugar que tenían reservado en el Offload.
        // Log obligatorio (pág. 24): "## JID: <JID> - <Asignación/Liberación> - Dirección Lógica: <DIRECCIÓN_LÓGICA> - Tamaño: <TAMAÑO>"

        // [Enunciado pág. 23 - Lectura / Escritura de datos] (Check 3)
        // Las usa el Planificador en las syscalls de memoria (LOAD_BATCH, LABEL, REPORT y checkpoints).
        case LEER_MEMORIA:{

            uint32_t jid = buffer_read_uint32(payload);
            uint32_t dir_fisica = buffer_read_uint32(payload);
            uint32_t tamanio = buffer_read_uint32(payload);

            int motivo =-1;

            void* datos = leer_memoria(jid,dir_fisica,tamanio,&motivo);

            retardo_memoria();

            t_paquete* respuesta;

            if(datos!= NULL){
                respuesta = crear_paquete(RESPUESTA_LEER_MEMORIA);
                buffer_add_bloque(respuesta->buffer,datos,tamanio);
                free(datos);
            } else {
                log_error(placa_logger, "Lectura inválida - JID: %u - Dir. Física: %u - Tamaño: %u - Motivo: %d", jid, dir_fisica, tamanio, motivo);

                respuesta = crear_paquete(MEMORIA_ERROR);
                buffer_add_uint32(respuesta->buffer,motivo);

            }

            int validar = enviar_paquete(socket_cliente,respuesta);

            if(validar != CONEXION_OK ){
                log_error(placa_logger, "No se pudo responder LEER_MEMORIA al Planificador");
            }

            eliminar_paquete(respuesta);
            break;

        }
        
        case ESCRIBIR_MEMORIA:{

            uint32_t jid        = buffer_read_uint32(payload);
            uint32_t dir_fisica = buffer_read_uint32(payload);

            uint32_t tamanio;
            void* contenido = buffer_read_bloque(payload, &tamanio);   

            int motivo = -1;                                           
            bool escribio = escribir_memoria(jid, dir_fisica, contenido, tamanio, &motivo);

            free(contenido);                                            

            retardo_memoria();                                          

            t_paquete* respuesta;

            if(escribio){
                respuesta = crear_paquete(ESCRIBIR_MEMORIA_OK);         
            } else {
                log_error(placa_logger, "Escritura inválida - JID: %u - Dir. Física: %u - Tamaño: %u - Motivo: %d", jid, dir_fisica, tamanio, motivo);
                respuesta = crear_paquete(MEMORIA_ERROR);               
                buffer_add_uint32(respuesta->buffer, motivo);
            }

            int validar = enviar_paquete(socket_cliente, respuesta);

            if(validar != CONEXION_OK){
                log_error(placa_logger, "No se pudo responder ESCRIBIR_MEMORIA al Planificador");
            }

            eliminar_paquete(respuesta);
            break;
        }


        // [Enunciado pág. 23 - Finalización de Job]
        // Solo se recibe un JID. Se liberan todos los marcos que tuviera ocupados, el lugar
        // reservado en el Offload y todas sus estructuras asociadas.
        // En el Check 2 todavía no hay marcos ni Offload: solo se liberan las estructuras (SIS-43).
        // Respuesta: FINALIZAR_JOB_OK siempre, aunque el JID no exista (finalizar es idempotente).
        case FINALIZAR_JOB:{

            uint32_t jid = buffer_read_uint32(payload);

            finalizar_job(jid);

            retardo_memoria();

            t_paquete* respuesta = crear_paquete(FINALIZAR_JOB_OK);
            int validar = enviar_paquete(socket_cliente, respuesta);

            if(validar != CONEXION_OK){
                log_error(placa_logger, "No se pudo responder FINALIZAR_JOB al Planificador");
            }

            eliminar_paquete(respuesta);

            break;

        }
        // [Enunciado pág. 23 - Carga de página] (Check 3)
        // La pide el Planificador cuando un Core informa un Page Fault. Se recibe el JID y el número
        // de página, y se carga en la memoria de la Placa. Si no hay marcos libres, hay que elegir
        // una víctima con el algoritmo de reemplazo (pág. 22).
        // Logs obligatorios (pág. 24):
        //   "## JID: <JID> - Subida a Memoria - Página: <NRO_PÁGINA> - Marco: <NRO_MARCO>"
        //   "## JID: <JID> - Bajada al Offload - Página: <NRO_PÁGINA>"
        //   "## Reemplazo - Marco: <NRO_MARCO> - JID Víctima: <JID> - Página Víctima: <NRO_PÁGINA>"
        case CARGAR_PAGINA:{

            break;

        }

        case DESLOCKEAR_PAGINAS:{
            uint32_t jid = buffer_read_uint32(payload);

            if(!deslockear_paginas(jid))
                log_warning(placa_logger, "DESLOCKEAR_PAGINAS de un JID inexistente: %u", jid);

            retardo_memoria();

            t_paquete* respuesta = crear_paquete(DESLOCKEO_OK);

            if(enviar_paquete(socket_cliente, respuesta) != CONEXION_OK)
                log_error(placa_logger, "No se pudo responder DESLOCKEO_OK al Planificador");

            eliminar_paquete(respuesta);
            break;
        }

        default:
            log_warning(placa_logger, "Operación desconocida recibida del PLANIFICADOR: %d", operacion_planificador);
            break;
        }

        eliminar_buffer(payload);
    }

    liberar_conexion(&socket_cliente);

    return EXIT_SUCCESS;

}
