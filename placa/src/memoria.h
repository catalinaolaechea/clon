#ifndef MEMORIA_H_
#define MEMORIA_H_

#include "inicializador.h"

//memoria fisica
extern void* memoria;
extern int cant_marcos;

extern pthread_mutex_t mutex_memoria;

//memoria

//estructura de marcos
typedef struct {
    bool ocupado;
    uint32_t jid;
    uint32_t nro_pagina;
} t_marco;

//array de cant_marcos
extern t_marco* marcos;

#define SLOT_SIN_ASIGNAR UINT32_MAX

// tabla de paginas por job
typedef struct {
    bool presente;
    uint32_t marco;
    bool lockeada;
    bool uso;
    bool modificado;
    uint64_t ultimo_acceso;
    uint32_t slot_offload;
}t_entrada_pagina;

typedef struct {
    uint32_t pagina_inicio;
    uint32_t cant_paginas;
    uint32_t tamanio;
}t_asignacion;

typedef enum {
    MARCO_OK,
    MARCO_PAGE_FAULT,
    MARCO_JID_INEXISTENTE,
    MARCO_PAGINA_NO_ASIGNADA
} t_resultado_marco;


bool inicializar_memoria();
int marcos_libres();

void retardo_memoria();

// Anota que la página se acaba de usar (para el reemplazo: ultimo_acceso para LRU, uso para Clock-M).
// Se llama con mutex_memoria tomado.
void registrar_acceso(t_entrada_pagina* entrada);

// [Enunciado pág. 23 - Obtención de marco] Si devuelve MARCO_OK, en *marco queda el número de marco
// y la página quedó lockeada. Toma los mutex adentro.
t_resultado_marco obtener_marco(uint32_t jid, uint32_t nro_pagina, uint32_t* marco);


// [Enunciado pág. 23 - Deslockeo de páginas] Saca el lock de TODAS las páginas del Job.
// Devuelve false si el JID no existe. Toma los mutex adentro.
bool deslockear_paginas(uint32_t jid);




#endif
