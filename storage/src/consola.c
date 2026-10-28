//! consola del Storage, un bucle que pide lineas por teclado y las procesa. Corre en su propio hilo para no trabar al servidor (el hilo del servidor va atendiendo las peticiones del Planificador)

#include "consola.h"
#include <stdio.h>               // printf
#include <stdlib.h>              // free
#include <readline/readline.h>   // readline(): pide una linea con prompt, edicion y flechitas
#include <readline/history.h>    // add_history(): guarda la linea para recuperarla despues
#include <stdbool.h>             
#include <strings.h>             // para "strcasecmp" 
#include <commons/string.h>      // funcionalidades de la commons para strings 
#include <string.h>              // para "strlen" que cuenta los caracteres de un texto 
#include "filesystem.h"          // para tener "STORAGE_DIR_BYTES_NOMBRE" una cte que dice cuantos bytes ocupa el nombre de una entrada del directorio (un "checkpoint" dentro del directorio, un indice del mismo que son 32 bytes dijos con 4 campos, ver en issue-24) 

// aca vamos a plantear los usos correctos de cada comando, como deben venir seguidos de cuantos parametros cuando los escribamos en la terminal
#define USO_INFO  "INFO"
#define USO_LS    "LS"
#define USO_CAT   "CAT <CheckpointName> <cantBytes>"
#define USO_WRITE "WRITE <CheckpointName> <hexaContent>"
#define USO_RM    "RM <CheckpointName>"

#define MAX_CARACTERES_NOMBRE (STORAGE_DIR_BYTES_NOMBRE - 1) // adecuandose a la cte de bytes de la entrada de un directorio, al nombre le alcanza como maximo 23 de esos 32 bytes, uno menos para contar con el "\0"

// estos prototipos son porque en C una funcion tiene que estar declarada antes para que alguien la llame en las funciones que declaramos aca. Son "static" porque son privadas de este archivo 
static void ejecutar_comando(char* linea);
static bool cantidad_valida(char** palabras, int esperada, const char* uso); 
static bool nombre_valido(const char* nombre);
static void mostrar_comandos(void);
static void comando_info(void);
static void comando_ls(void);
static void comando_cat(char** palabras);
static void comando_write(char** palabras);
static void comando_rm(char** palabras);

void* storage_consola_atender(void* sin_uso) {
   (void) sin_uso;  // esto es para "callar" el warning del compilador, le decimos al mismo que no usarlo es aproposito y -Wextra no tira warning 

   char* linea;  // aca vamos almacenando las lineas que va recibiendo el readline  

   while ((linea = readline(STORAGE_CONSOLA_PROMPT)) != NULL) {  // readline muestra "Storage>" y espera lineas, las devuelve sin el "\n" el final, cuando llega un EOF devuelve NULL y corta
      string_trim(&linea);  // "string_trim()" agarra la linea que leyo del readline y le corta los espacios del principio y del final 

      if(!string_is_empty(linea)) {  // de commons, si mandaron un enter sin querer, readline devuelve "" que no es NULL, pero no se le contesta ni se la guarda en el historial osea ni entra al if
         add_history(linea);  // lo demas si, lo manda a guardar en "add_history" donde estan las lineas que fue leyendo (no importa si el comando no lo conocemos igual lo guardamos)
         ejecutar_comando(linea);  // le damos lugar a "ejecutar_comando()" que ahi vamos a tener todos los posibles comandos de nuestra consola 
      }

      free(linea); // "readline" va pidiendo memoria pero nosotros tenemos que liberarla. La liberamos afuera del if porque si estuviera dentro unicamente abarcamos las lineas que fuimos leyendo, pero los enters sin nada que pudieran haber pasado no los abarcamos, tonc afuera abarcamos los 2 
   }

   return NULL;
}

// aca vamos a tener los los posibles comandos de nuestra consola una vez vayamos leyendo las lineas que guarda el readline 
static void ejecutar_comando(char* linea) {
   char** palabras = string_split(linea, " ");  // agarra la linea por ejemplo "CAT modelo7 16" y las splitea en {"CAT", "modelo7", "16", NULL}
   char* comando = palabras[0];  // para no andar repitiendo "palabras[0]" que sabemos es donde debe venir el comando en orden, entonces directamente lo guardamos aca 

   // una por una vamos comparando con los posibles comandos con "strcasecmp" que no le importa mayusculas para primero ver que tipo de comando se trata 
   if (strcasecmp(comando, "INFO") == 0) {  
      if (cantidad_valida(palabras, 1, USO_INFO)) {  // validamos que tenga la cantidad necesaria con esta funcion, le pasamos la linea spliteada, la cantidad esperada y el uso adecuado que guardamos en las constantes arriba de todo para que despues valide 
         comando_info();  // una vez confirmado el comando y validado su cantidad adecuada podemos mandar a ejecutar dicho comando, aca especificamente para info 
      }
   } else if (strcasecmp(comando, "LS") == 0) {  // vemos comando 
      if (cantidad_valida(palabras, 1, USO_LS)) {  // validamos pasando la spliteada, la cantidad esperada y el uso adecuado para comparar en esa funcion 
         comando_ls();  // aca el comando para ls 
      }
   } else if (strcasecmp(comando, "CAT") == 0) {  // aca por ejemplo si validamos que es un comando CAT 
      if (cantidad_valida(palabras, 3, USO_CAT)) {  // los comandos CAT deben venir con la cantidad minima de 3 con los parametros <CheckpointName> <cantBytes>
         comando_cat(palabras);
      }
   } else if (strcasecmp(comando, "WRITE") == 0) {
      if (cantidad_valida(palabras, 3, USO_WRITE)) {
         comando_write(palabras);
      }
   } else if (strcasecmp(comando, "RM") == 0) {
      if (cantidad_valida(palabras, 2, USO_RM)) {
         comando_rm(palabras);
      }
   } else {
      printf("Comando desconocido: %s\n", comando);  // devolvemos ese supuesto comando (que es el palabra[0] pero justamente si llega aca no es ningun comando conocido)
      mostrar_comandos();  // mostramos lista de comandos que manejamos 
   }

   string_array_destroy(palabras);  // liberamos las lineas spliteadas que leimos 
}

// aca es donde validamos que la cantidad sea valida, recibimos la linea de palabras spliteadas, la cantidad esperada y el uso especifico que debe tener 
static bool cantidad_valida(char** palabras, int esperada, const char* uso) {
   if (string_array_size(palabras) != esperada) {  // comparamos y en caso de no tener la cantidad esperada devolvemos un mensaje con el uso especifico que debe tener 
      printf("Cantidad de argumentos invalida. Uso: %s\n", uso);  // aca, que es "USO_LS", "USO_WRITE" etc. 
      return false;
   }

   return true;  // caso que sea la cantidad esperada la dejamos pasar 
}

// recibe el nombre del checkpoint, devuelve true si sirve
static bool nombre_valido(const char* nombre) {
   if (nombre[0] == '\0') {  // miramos si el nombre es vacio
      printf("Nombre de checkpoint invalido: no puede estar vacio\n");
      return false;
   }

   size_t largo = strlen(nombre);  // conseguimos la longitud de dicho nombre 

   if (largo > MAX_CARACTERES_NOMBRE) {  // comparamos que no se pase del maximo que vimos en esta constante que es 22 (por ahora, si en algun momento se cambia se adecua a cte que viene de filesystem)
      printf("Nombre de checkpoint invalido: \"%s\" tiene %zu caracteres y el maximo es %d\n", nombre, largo, MAX_CARACTERES_NOMBRE);
      return false;
   }

   return true;
}

// mostramos los posibles comandos con sus usos especificos 
static void mostrar_comandos(void) {
   printf("Comandos disponibles:\n");
   printf("  %s\n", USO_INFO);
   printf("  %s\n", USO_LS);
   printf("  %s\n", USO_CAT);
   printf("  %s\n", USO_WRITE);
   printf("  %s\n", USO_RM);
}

//! por ahora no tenemos definido que hace cada comando, pero lo dejamos para el sis-79 que ahi los definimos, por ahora tenemos todo el esqueleto anterior
static void comando_info(void) {
   printf("INFO: pendiente (SIS-79)\n");
}

static void comando_ls(void) {
   printf("LS: pendiente (SIS-79)\n");
}

static void comando_cat(char** palabras) {
   // solo agregamos una validacion en caso que el nombre no sea valido, justamente en aquellos comandos que deben venir de la mano con un nombre 
   if (!nombre_valido(palabras[1])) { 
      return;
   }

   printf("CAT %s %s: pendiente (SIS-79)\n", palabras[1], palabras[2]);
}

static void comando_write(char** palabras) {
   // lo mismo aca 
   if (!nombre_valido(palabras[1])) {
      return;
   }

   printf("WRITE %s %s: pendiente (SIS-81)\n", palabras[1], palabras[2]);
}

static void comando_rm(char** palabras) {
   // y aca 
   if (!nombre_valido(palabras[1])) {
      return;
   }

   printf("RM %s: pendiente (SIS-81)\n", palabras[1]);
}
