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

//! devuelve "<directorio>/<archivo>" teniendo en cuenta que el "PATH_STORAGE" puede venir del .config con o sin la barra final. El string que devuelve es nuevo entonces quien lo llama lo tiene que liberar
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

   return config;  // camino feliz: el .config quedo cargado y validado
}
   
//! basicamente agarra una instancia de .config del modulo y liberamos todos sus campos
void storage_config_destruir(t_storage_config* config) {
      if (config == NULL) return;  // si nos pasan NULL no podemos ni leer sus campos, entonces cortamos antes de tocarlo

      // liberamos todos los campos
      free(config->puerto_escucha);
      free(config->log_level);
      free(config->path_storage);
      free(config->path_volumen);
      free(config->path_journal);

      free(config);  //! el struct SIEMPRE se libera ultimo: si lo liberaramos primero, los punteros de adentro se irian con el y no podriamos liberarlos tonc memory leak
}

//! deja el directorio de la constante "PATH_STORAGE" listo, si no existe lo crea, verifica si es un directorio de verdad y que tenga permisos de escritura, si alguno de los casos falla devuelve false
bool storage_config_asegurar_path(t_storage_config* config, t_log* logger) {  // recibe una instancia de .config del modulo y un logger creado anteriormente 
   // 
   if (mkdir(config -> path_storage, 0755) == 0) {  // "mkdit" es una syscall para pedirle al SO que cree un directorio en el disco, especificamente con el path que le asiganmos nosotros a nuestro modulo, el octal 0755 es el modo, permite que el dueño lee/escribe/entra y el resto solo lee y entra
      log_info(logger, "PATH_STORAGE creado: %s", config -> path_storage);  // logueamos y mostramos por pantalla el siguiente mensaje 
   
   } else if (errno == EEXIST) {  // en caso de haber error mkdir guarda un -1 en "errno", y especificamente nos preguntamos cual fue el motivo del error
      struct stat info;  // es un struct de POSIX donde "stat()" nos deja ver metadatos del archivo 

      if (stat(config -> path_storage, &info) != 0) {  // basicamente si no devuelve 0 es que no pudimos consultar dicho path por algun motivo, permisos probablemente
         log_error(logger, "No se pudo consultar PATH_STORAGE \"%s\": %s", config -> path_storage, strerror(errno));  // lo anotamos en el log 
         return false; 
      }

      if (!S_ISDIR(info.st_mode)) {  // pregunta basicamente si NO es un directorio (tranquilamente puede ser un archivo que se llama igual)
         log_error(logger, "PATH_STORAGE \"%s\" existe pero no es un directorio.", config->path_storage);
         return false;
      }

      log_info(logger, "PATH_STORAGE ya existia: %s", config -> path_storage);  // si no bueno vamos por el primer posible error que era que ya existia 
   
   } else {  // cualquier otro caso que no sea de los denotados anteriormente 
      log_error(logger, "No se pudo crear PATH_STORAGE \"%s\": %s", config -> path_storage, strerror(errno));
      return false; 
   } 

   //! ya abarcamos las posibilidades donde creamos el directorio y sus posibles errores, ahora por ultimo tenemos que mirar que se le pueda escribir 
   if (access(config -> path_storage, W_OK | X_OK) != 0) {  // "access()" nos permite sabe si dicho directorio es accesible
      log_error(logger, "PATH_STORAGE \"%s\" no es escribible: %s", config -> path_storage, strerror(errno));  // si no bueno mostramos en pantalla y logueamos el error
      return false; 
   }

   return true; // dado que se pudo crear el directorio y se puede escribir en el entonces devolvemos true finalmente 
}

//! basicamente logueamos todo el archivo .config del modulo 
void storage_config_loguear(t_storage_config* config, t_log* logger) {
   log_info(logger, "=== Configuracion de Storage ===");
   log_info(logger, "PUERTO_ESCUCHA:        %s", config->puerto_escucha);
   log_info(logger, "LOG_LEVEL:             %s", config->log_level);
   log_info(logger, "PATH_STORAGE:          %s", config->path_storage);
   log_info(logger, "CANT_BLOQUES:          %u", config->cant_bloques);        // %u porque uint32_t es un entero SIN signo
   log_info(logger, "TAM_BLOQUE:            %u bytes", config->tam_bloque);
   log_info(logger, "BLOQUES_DIRECTORIO:    %u", config->bloques_directorio);
   log_info(logger, "RETARDO_ACCESO_BLOQUE: %u ms", config->retardo_acceso_bloque);
   log_info(logger, "RETARDO_COMMIT:        %u ms", config->retardo_commit);
   log_info(logger, "Archivo de volumen:    %s", config->path_volumen);        // rutas derivadas, no vienen del .config
   log_info(logger, "Archivo de journal:    %s", config->path_journal);
}