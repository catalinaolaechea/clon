#ifndef PLACA_H_
#define PLACA_H_

#include <utils/protocolo.h>
#include <utils/serializacion.h>
#include <utils/sockets.h>
#include <utils/utils.h>
#include <utils/config.h>
#include <utils/log.h>
#include <pthread.h>
#include <utils/handshake.h>
#include <utils/mensaje_prueba.h>

//consola
#include <readline/readline.h>
#include <readline/history.h>
#include <commons/bitarray.h>

extern t_config* placa_config;
extern t_log* placa_logger;
extern char* archivo_config;

//archivo de configuracion
extern char* PUERTO_ESCUCHA;
extern t_log_level LOG_LEVEL;
extern int TAM_MEMORIA;
extern int TAM_PAGINA;
extern int RETARDO_MEMORIA;
extern char* ALGORITMO_REEMPLAZO;
extern char* PATH_INSTRUCCIONES;
extern char* PATH_OFFLOAD;
extern int TAM_OFFLOAD;
extern int RETARDO_OFFLOAD;


//memoria fisica
extern void* memoria;
extern int cant_marcos;


//offload
extern int archivo_offload;
extern t_bitarray* slots_offload;

//variables globables
extern t_list* lista_core;
extern t_list* lista_jobs;

//semaforos
extern pthread_mutex_t mutex_lista_core;
extern pthread_mutex_t mutex_jobs;
extern pthread_mutex_t mutex_memoria;

typedef struct {
    int socket_core;
    int id_core;
}t_core_placa;


// [Enunciado pág. 21 - Memoria de Instrucciones] + [pág. 22 - Creación de Job]
// Estructura administrativa de cada Job en la Placa. Asocia el Job con sus instrucciones.
// En el Check 3 se suma la tabla de páginas del Job, que por enunciado (pág. 21) es una estructura
// administrativa y NO se guarda dentro de la memoria de datos.
typedef struct {
    uint32_t jid;
    char** instrucciones;
    uint32_t cant_instrucciones;
    t_list* tabla_paginas; //indice en la lista de nr de pagina
    t_list* asignaciones; // para que se sepa que liberar
} t_job_placa;

//memoria

//estructura de marcos
typedef struct {
    bool ocupado;
    uint32_t jid;
    uint32_t nro_pagina;
} t_marco;

//array de cant_marcos
extern t_marco* marcos;

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


void inicializar_log();
void inicializar_config();
void inicializador_memoria();
int marcos_libres();


//funciones de servidor
void* atender_cliente(void* socket);
void* atender_core(void* core);
void* atender_planificador(void* panificador);

//consola
void* atender_consola(void* arg);
void procesar_comando(char* linea);
void comando_info();
void comando_tls();

//antender_planificador
t_job_placa* inicializar_job(uint32_t jid, char* nombre_archivo);
t_job_placa* crear_job(uint32_t jid, char** instrucciones, uint32_t cantidad);
char** leer_instrucciones(char* path, uint32_t* cantidad);
t_job_placa* buscar_job(uint32_t jid);
t_job_placa* quitar_job(uint32_t jid);
void finalizar_job(uint32_t jid);

void retardo_memoria();


//
void comando_crear(char** args);
void comando_finalizar(char** args);
void comando_fetch(char** args);

#endif