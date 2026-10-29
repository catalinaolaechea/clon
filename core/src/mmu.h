#ifndef MMU_H_
#define MMU_H_

#include <stdbool.h>
#include <commons/collections/list.h>
#include <utils/direcciones.h>
#include "inicializador.h"
#include "conexiones.h"

#define LOG_PAGE_FAULT "## JID: %u - Page Fault - Página: %u"

// Pide a la Placa el tamanio de pagina (OBTENER_TAM_PAGINA).
bool obtener_tam_pagina(void);

// Tamanio de pagina de la Placa en bytes.
uint32_t tam_pagina(void);

typedef enum {
    TRADUCCION_OK,           
    TRADUCCION_PAGE_FAULT,   
    TRADUCCION_ERROR         
} t_resultado_traduccion;

t_resultado_traduccion traducir_rango(uint32_t jid, uint32_t dir_logica, uint32_t tamanio, t_list** direcciones, uint32_t* pagina_faltante);

bool deslockear_paginas(uint32_t jid);
bool atender_page_fault(uint32_t jid, uint32_t nro_pagina);

// ningun Job sale del Core, ni termina un ciclo, con paginas lockeadas. Devuelve false si fallo el deslockeo (queda logueado).
bool liberar_paginas_del_ciclo(uint32_t jid);


/*
Log obligatorio de cada acceso a memoria, uno por cada pedazo [dir_fisica; tamanio]:
"## JID: <JID> - Acción: <Lectura/Escritura> - Dirección Física: <DIRECCION_FISICA> - Tamaño: <TAMAÑO>"
*/
#define LOG_ACCESO_MEMORIA "##JID: %u - Acción: %s - Dirección Física: %u - Tamaño: %u"

bool leer_memoria(uint32_t jid, t_list* direcciones, void** contenido, uint32_t* tamanio);

bool escribir_memoria(uint32_t jid, t_list* direcciones, const void* contenido);

#endif