#include "consola.h"
#include "atender_core.h"
#include "jobs.h"

//consola
#include <readline/readline.h>
#include <readline/history.h>

static void procesar_comando(char* linea);
static void comando_info();
static void comando_tls();

// [Enunciado pág. 23-24 - Comandos locales]
// La Placa implementa una consola por stdin con los comandos INFO y TLS, que responden en formato
// "human-readable". Se pueden agregar otros comandos siempre que no modifiquen ni contradigan
// el funcionamiento del módulo.
void* atender_consola(void* arg){
    (void) arg;  // firma exigida por pthread_create, no se usa
    char* linea;

    while((linea = readline("Placa> ")) !=NULL){
        if(strlen(linea)>0){
            add_history(linea);
            procesar_comando(linea);
        }
        free(linea);
    }
    return NULL;
}

static void procesar_comando(char* linea){
    char** args = string_split(linea, " ");

    if(strcmp(args[0], "INFO") ==0){
        comando_info();
    }else if(strcmp(args[0],"TLS") ==0){
        comando_tls();
    }else {
        printf("Comando desconocido: %s\n",args[0]);
    }

    string_array_destroy(args);
}

// [Enunciado pág. 23 - Comandos locales: INFO]
// Muestra: porcentaje de memoria y de Offload ocupados; cantidad de frames libres, lockeados y
// totales; y cantidad de Jobs actuales.
static void comando_info(){
    pthread_mutex_lock(&mutex_lista_core);
    int cantidad_cores = list_size(lista_core);
    pthread_mutex_unlock(&mutex_lista_core);

    printf("Cores conectados: %d\n", cantidad_cores);

    /*Falta
    porcentaje de memoria/offload
    frames libres/lockeados/totales
    cantidad de jobs

    */
}

// [Enunciado pág. 24 - Comandos locales: TLS]
// Lista todos los Jobs registrados indicando, para cada uno: ID, cantidad de páginas del conjunto
// residente (las que están cargadas en la Placa) y cantidad de páginas totales.
static void comando_tls(){
    pthread_mutex_lock(&mutex_lista_core);

    for(int i = 0; i < list_size(lista_core); i++){
        t_core_placa* core = list_get(lista_core,i);
        printf("Core conectado - ID: %d\n",core->id_core);
    }

    pthread_mutex_unlock(&mutex_lista_core);

    /*Falta
    Listar jobs(no cores) con paginas de conjunto residente/totales
    */
}
