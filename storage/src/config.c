#include "config.h"  // header que definimos anteriormente, el tipo "t_storage_config" y las 4 firmas que vamos a definir aca 

#include <stdio.h>    // entrada salida (fprintf, printf, stderr) 
#include <stdlib.h>   // memoria y utilidades (malloc, free, exit) 
#include <string.h>   // manejo de cadenas (strdup, strlen, strcmp, strerror)
#include <errno.h>    // de POSIX (interfaz del SO), para la variable (errno, EEXIST, ...) y ciertos codigos para distinguir entre si algo existia o si fallo etc.
#include <sys/stat.h> // POSIX operaciones sobre archivos y directorios  (mkdir, stat)
#include <unistd.h>   // estandares de UNIX (access, read, write, close, usleep)

#include <commons/string.h>  // funciones de las commons para manejo de strings
#include <utils/config.h>    // funciones nuestras del archivo de utils, especificamente para manejo de los archivos .config 

static bool es_potencia_de_dos(uint32_t n) {
   return n != 0 && (n & (n - 1)) == 0;
}

// devuelve "<directorio>/<archivo>" teniendo en cuenta que el "PATH_STORAGE" puede venir del .config con o sin la barra final. El string que devuelve es nuevo entonces quien lo llama lo tiene que liberar
static char* juntar_path(char* directorio, char* archivo) {
    if (string_ends_with(directorio, "/")) {  // wrapper de utils
        return string_from_format("%s%s", directorio, archivo);  // wrapper de utils 
    }
    return string_from_format("%s/%s", directorio, archivo);
}

// !que las claves EXISTAN ya lo garantiza "so_config_get_int" que calculo tambien es un wrapper, y una vez llenamos los campos del struct del .config de este modulo iriamos con esta funcion para ver si dichos valores son validos y cumplen con las reestricciones. Entonces recibimos una instancia del .config del modulo 
static bool valores_validos(t_storage_config* config) {
   if ((int) log_level_from_string(config->log_level) == -1) {
      fprintf(stderr, "ERROR - LOG_LEVEL invalido: \"%s\". Validos: TRACE, DEBUG, INFO, WARNING, ERROR.\n", config->log_level);
      return false;
   }

   if (!es_potencia_de_dos(config->tam_bloque)) {  // le pasamos el campo "tam_bloque" de la instancia de config del modulo y hacemos uso de la funcion denotada anteriormente 
      fprintf(stderr, "ERROR - TAM_BLOQUE debe ser potencia de 2 y se recibio %u.\n", config->tam_bloque);  // mostramos por pantalla mediante el "stderr" ese mensaje y el valor del mismo
      return false;
   }

   if (config->tam_bloque < STORAGE_TAM_BLOQUE_MINIMO) {
      fprintf(stderr, "ERROR - TAM_BLOQUE debe ser >= %d porque una entrada de directorio ocupa %d bytes, y se recibio %u.\n", STORAGE_TAM_BLOQUE_MINIMO, STORAGE_TAM_BLOQUE_MINIMO, config->tam_bloque);
      return false;
   }

   if (config->cant_bloques == 0) {
      fprintf(stderr, "ERROR - CANT_BLOQUES debe ser mayor a 0.\n");
      return false;
   }

   if (config->bloques_directorio == 0) {
      fprintf(stderr, "ERROR - BLOQUES_DIRECTORIO debe ser mayor a 0.\n");
      return false;
   }

    // El bloque 0 es el superbloque y la FAT ocupa lo suyo: si ademas el directorio se lleva todo, no queda ni un bloque de datos.
   if (config->bloques_directorio >= config->cant_bloques) {
      fprintf(stderr, "ERROR - BLOQUES_DIRECTORIO (%u) debe ser menor a CANT_BLOQUES (%u).\n", config->bloques_directorio, config->cant_bloques);
      return false;
   }

   return true;
}

t_storage_config* storage_config_cargar(char* path_archivo) {  // primero recibe el path a donde ir a buscar el struct del .config del modulo 
   t_config* archivo = iniciar_config(path_archivo);  // en "archivo" que sea del tipo de un .config "general" (que se va a terminar amoldando a uno del tipo de este modulo Storage) le asignamos "iniciar_config(path)" la misma es un wrapper de los utils que lee el .config del disco y deja en memoria como pares claves-valor y si no existe el archivo corta el programa ahi mismo 
   //! fundamental: esto lo hacemos porque los "t_config" son un parser, unicamente saben abrir un archivo txt y a partir de cada linea guardar sus pares

   t_storage_config* config = malloc(sizeof(t_storage_config));  // a una instancia de un .config del modulo Storage le pedimos memoria del tamaño que tienen 

   // y al mismo le vamos llenando todos los campos con lo que se cargo en "archivo" que en principio es una instancia de un .config "general" pero al pasarle el path de un .config de este modulo ahora tiene los campos necesarios de uno de Storage
   //! es por eso que aca hacemos el "traspaso" donde hacemos que los pares en txt ahora si se vuelvan datos para nuestro .config del modulo Storage 
   config->puerto_escucha        = strdup(so_config_get_string(archivo, STORAGE_NOMBRE_MODULO, "PUERTO_ESCUCHA"));
   config->log_level             = strdup(so_config_get_string(archivo, STORAGE_NOMBRE_MODULO, "LOG_LEVEL"));
   config->path_storage          = strdup(so_config_get_string(archivo, STORAGE_NOMBRE_MODULO, "PATH_STORAGE"));

   config->cant_bloques          = (uint32_t) so_config_get_int(archivo, STORAGE_NOMBRE_MODULO, "CANT_BLOQUES");
   config->tam_bloque            = (uint32_t) so_config_get_int(archivo, STORAGE_NOMBRE_MODULO, "TAM_BLOQUE");
   config->bloques_directorio    = (uint32_t) so_config_get_int(archivo, STORAGE_NOMBRE_MODULO, "BLOQUES_DIRECTORIO");
   config->retardo_acceso_bloque = (uint32_t) so_config_get_int(archivo, STORAGE_NOMBRE_MODULO, "RETARDO_ACCESO_BLOQUE");
   config->retardo_commit        = (uint32_t) so_config_get_int(archivo, STORAGE_NOMBRE_MODULO, "RETARDO_COMMIT");

   config_destroy(archivo);  // una vez tenemos los datos guardados en la instancia que nos interesa la podemos liberar 

   config->path_volumen = juntar_path(config->path_storage, STORAGE_ARCHIVO_VOLUMEN);
   config->path_journal = juntar_path(config->path_storage, STORAGE_ARCHIVO_JOURNAL);

   // y con la funcion denotada anteriormente y los datos ya cargados hacemos una prueba que sean validos, en caso de no serlo volamos todo 
   if (!valores_validos(config)) {
        storage_config_destruir(config);
        return NULL;
   }

   return config;
}