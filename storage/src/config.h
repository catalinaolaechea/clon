// arrancamos por el header de config del modulo, aca tendremos el "contrato" de los tipos de datos que necesita el modulo para laburar. Recordemos que el compilador la lee por unica vez al inicio asi ve el contrato digamos y sigue 

#ifndef STORAGE_CONFIG_H_  // "include guards"  
#define STORAGE_CONFIG_H_ 

#include <stdbool.h>          // importamos los headers del tipo bool (en C no viene default)
#include <stdint.h>           // importamos los headers del tipo uint32_t 
// #include <commons/config.h>   importamos los headers del tipo de dato "t_config" asi la podemos implementar. Al final fuimos por la alternativa de liberar todos los strings en vez de  
#include <commons/log.h>      //  y headers del tipo "t_log"

#define STORAGE_NOMBRE_MODULO "STORAGE"        // nombre con el cual el modulo se identifica en el log y los mensajes de error
#define STORAGE_TAM_BLOQUE_MINIMO 32           // una entrada de directorio pesa 32 bytes, entonces como minimo todos los bloques deben respetar ese tamaño
#define STORAGE_ARCHIVO_VOLUMEN "volumen.dat"  // aca vamos a guardar los checkpoints (se van a escribir los datos, actualizar la FAT encadenando los bloques de datos, actualiza la entrada del directorio)
#define STORAGE_ARCHIVO_JOURNAL "journal.dat"  // es del check 4, es un archivo aparte del volumen donde se anota lo que se va a hacer antes de hacerlo

// en el siguiente struct tendremos lugar para los 8 valores asociados a las keys del archivo .config (string = char*), despues la iremos asignando con las funciones de las common a cada campo. Y despues los 2 anteultimos son nuestros para donde tendremos el path del volumen y journal 
typedef struct {
   char*    puerto_escucha;           
   char*    log_level;
   char*    path_storage;

   uint32_t cant_bloques;
   uint32_t tam_bloque;
   uint32_t bloques_directorio;
   uint32_t retardo_acceso_bloque;    // milisegundos
   uint32_t retardo_commit;           // milisegundos

   char*    path_volumen;             // <path_storage>/volumen.dat
   char*    path_journal;             // <path_storage>/journal.dat  (recordemos que despues tenemos que liberar todas las memorias reservadas de los char*)
} t_storage_config;  

//! aca abajo vamos a declarar 4 funciones que despues definimos en el .c 
t_storage_config* storage_config_cargar(char* path_archivo);  // recibe el path del archivo || va a leer y validar el archivo .config, si algun valor es invalido devuelve NULL con el motivo por "stderr", terminamos liberando con "storage_config_destruir" q despues definimos nosotros. Notar que es del tipo del struct que representa el .config de este Modulo Storage

bool storage_config_asegurar_path(t_storage_config* config, t_log* logger);  // recibe una instancia del .config y una instancia de un logger || crea "PATH_STORAGE" si no existe y verifica si se puede escribir en el, dado cualquiera de los 2 casos vamos a querer mostrarlo por pantalla y ademas que quede guardado entonces para eso recibimos una instancia de un logger

void storage_config_loguear(t_storage_config* config, t_log* logger);  // lo mismo que arriba || logueamos el archivo .config del modulo con el que arrancamos 

void storage_config_destruir(t_storage_config* config); // recibe una instancia del .config del modulo ya creada || la vuela 

#endif // para cerrar el #ifndef 
