#ifndef SERVICIOS_H_
#define SERVICIOS_H_

#include <stdbool.h>
#include <stdint.h>
#include <commons/collections/list.h>
#include <utils/contexto.h>
#include "instrucciones.h"

typedef struct {
    uint8_t op_code;       // SYSCALL_LOAD_BATCH, SYSCALL_LABEL o SYSCALL_REPORT
    char* archivo;         // solo LOAD_BATCH 
    uint32_t tamanio;      // cant bytes del rango
    t_list* direcciones;   // una por pagina
} t_syscall_servicio;

typedef enum {
    SERVICIO_OK,           // el pedido esta listo: el Job se devuelve con la syscall
    SERVICIO_PAGE_FAULT,   // alguna pagina no estaba: ya se logueo y se deslockeo, ver 'pagina_faltante'
    SERVICIO_ERROR         // parametros invalidos o fallo de la Placa (queda logueado): se finaliza el Job
} t_resultado_servicio;

bool es_syscall_de_servicio(const char* nombre);

t_resultado_servicio preparar_syscall_servicio(uint32_t jid, const t_instruccion* instruccion, t_contexto* contexto, t_syscall_servicio* servicio, uint32_t* pagina_faltante);

void syscall_servicio_destruir(t_syscall_servicio* servicio);

#endif