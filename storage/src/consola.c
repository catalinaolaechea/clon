//! consola del Storage, un bucle que pide lineas por teclado y las procesa. Corre en su propio hilo para no trabar al servidor (el hilo del servidor va atendiendo las peticiones del Planificador)

#include "consola.h"
#include <stdio.h>               // printf
#include <stdlib.h>              // free
#include <readline/readline.h>   // readline(): pide una linea con prompt, edicion y flechitas
#include <readline/history.h>    // add_history(): guarda la linea para recuperarla despues
#include <stdbool.h>             
#include <stdint.h>              // trae "uint32_t" (entero sin signo de 32bits) y "UNIT32_MAX" (el valor mas grande que entra en enteros de 32bits, unos 4 millones) 
#include <ctype.h>               // trae "isdigit" que dice si un caracter es un digito del 0 al 9 
#include <errno.h>               // trae "errno" q es una variable global donde las funciones dejan anotado que error paso 
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
#define MAX_BYTES_WRITE 20  // del enunciado nos piden que "WRITE" escriba hasta 20 bytes 

//! estos prototipos son porque en C una funcion tiene que estar declarada antes para que alguien la llame en las funciones que declaramos aca. Son "static" porque son privadas de este archivo 
static void ejecutar_comando(char* linea);
static bool cantidad_valida(char** palabras, int esperada, const char* uso); 
static bool nombre_valido(const char* nombre);
static bool cantidad_bytes_valida(const char* texto, uint32_t* cantidad);
static bool contenido_hexa_valido(const char* texto, uint8_t* bytes, uint32_t* cantidad);
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

//! aca vamos a tener los los posibles comandos de nuestra consola una vez vayamos leyendo las lineas que guarda el readline 
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

//! aca es donde validamos que la cantidad sea valida, recibimos la linea de palabras spliteadas, la cantidad esperada y el uso especifico que debe tener 
static bool cantidad_valida(char** palabras, int esperada, const char* uso) {
   if (string_array_size(palabras) != esperada) {  // comparamos y en caso de no tener la cantidad esperada devolvemos un mensaje con el uso especifico que debe tener 
      printf("Cantidad de argumentos invalida. Uso: %s\n", uso);  // aca, que es "USO_LS", "USO_WRITE" etc. 
      return false;
   }

   return true;  // caso que sea la cantidad esperada la dejamos pasar 
}

//! recibe el nombre del checkpoint, devuelve true si sirve
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

//! agregamos esta funcion que la necesitamos para poder ejecutar el comando de CAT, el comando seria algo como "CAT modelo7 12abc" en el parametro texto va a recibir "12abc" y evaluarlo 
static bool cantidad_bytes_valida(const char* texto, uint32_t* cantidad) {
   if (!isdigit((unsigned char) texto[0])) {  // primero como base miramos que NO arranque con un digito 
      printf("cantBytes invalido: \"%s\" no es un numero entero positivo\n", texto);  
      return false;
   }

   char* fin;
   errno = 0;
   unsigned long valor = strtoul(texto, &fin, 10);  // agarramos el texto y lo convertimos a base10 con "strtoul()", y el puntero "fin" guarda el ultimo caracter que leimos

   if (*fin != '\0') {  // si no leyo hasta el final, osea que fin no guardo el ultimo caracter este
      printf("cantBytes invalido: \"%s\" no es un numero entero positivo\n", texto);
      return false;
   }

   if (errno == ERANGE || valor > UINT32_MAX) {  // si errno posiblemente guardo un numero que no entraba en "unsigned long" osea es muy grande, o si no entro en el maximo de 32bits 
      printf("cantBytes invalido: \"%s\" es demasiado grande (maximo %u)\n", texto, UINT32_MAX);
      return false;
   }

   if (valor == 0) {
      printf("cantBytes invalido: tiene que ser mayor a 0\n");
      return false;
   }

   *cantidad = (uint32_t) valor;  // en cantidad que es valida la guardamos en esta variable, y si llega a darse que lo leimos en 64bits lo achicamos a 32bits 
   return true;
}

//! esta funcion recibe el texto escrito despues del nombre en comando WRITE, que viene en forma "WRITE modelo7 0A1B2C", se fija que el numero sea hexa valido, de longitud par y 20 bytes como maximo, y lo convierte en la cantidad de bytes que significa dicho numero en hexa 
static bool contenido_hexa_valido(const char* texto, uint8_t* bytes, uint32_t* cantidad) {
   size_t largo = strlen(texto);  // consigue largo 

   if (largo == 0) {
      printf("hexaContent invalido: no puede estar vacio\n");  // caso que sea vacio que no tenga longitud 
      return false;
   }

   for (size_t i = 0; i < largo; i++) {  // recorre todos los caracteres uno por uno 
      if (!isxdigit((unsigned char) texto[i])) {  // "isxdigit()" mira si dicho caracter es 0-9, a-f o A-F, caso de no serlo directamente no es hexa
         printf("hexaContent invalido: '%c' (caracter %zu) no es hexadecimal (0-9, A-F)\n", texto[i], i + 1);
         return false;
      }
   }

   if (largo % 2 != 0) {  // caso que longitud no sea par 
      printf("hexaContent invalido: tiene %zu caracteres y tiene que ser par (2 por byte)\n", largo);
      return false;
   }

   if (largo / 2 > MAX_BYTES_WRITE) {  // caso que la cantidad de bytes sobre pase la maxima 
      printf("hexaContent invalido: son %zu bytes y el maximo es %d\n", largo / 2, MAX_BYTES_WRITE);
      return false;
   }

   // una vez todo validado si no es uno de los casos anteriores vamos a convertir a bytes 
   for (size_t i = 0; i < largo / 2; i++) {  // lo recorre "de a un byte" osea la cadena cada 2 caracteres es un byte
      char par[3] = { texto[2 * i], texto[2 * i + 1], '\0' };  // arma un texto mostrando cada 2 caracteres 
      bytes[i] = (uint8_t) strtoul(par, NULL, 16);  // terminamos guardando en un array cada byte usando tambien "strtoul()" pero en base16 
   }

   *cantidad = (uint32_t) (largo / 2);  // aca guardamos la cantidad de bytes quedaron y lo guardamos en esta variable 
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

//! aca vamos a ir teniendo los comandos definidos que hace cada uno 
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

   uint32_t cantidad;

   if (!cantidad_bytes_valida(palabras[2], &cantidad)) {  // hacemos uso de la funcion que definimos para ver si la cantidad de bytes leida en las cadena spliteada es valida 
      return;
   }

   printf("CAT %s %u bytes: pendiente (SIS-79)\n", palabras[1], cantidad);
}

static void comando_write(char** palabras) {
   // lo mismo aca 
   if (!nombre_valido(palabras[1])) {
      return;
   }

   uint8_t bytes[MAX_BYTES_WRITE];  // tenemos un array para guardar la mayor cantidad posible de bytes que tenemos (que son 20 lugares en total)
   uint32_t cantidad;  // aca vamos a tener cuantos de esos 20 lugares se van a usar para escribir en el WRITE

   if (!contenido_hexa_valido(palabras[2], bytes, &cantidad)) {  // verificamos que lo que queremos escribir con WRITE tenga contenido hexa valido con la funcion q definimos arriba 
      return;
   }

   printf("WRITE %s: se escribirian %u bytes:", palabras[1], cantidad);  // si esta todo ok antes de escribir nada mostramos un "placeholder" con lo que escribiria, para ver si la conversion anduvo bien

   for (uint32_t i = 0; i < cantidad; i++) {  // recorremos unicamente hasta la cantidad posible que tome aquello en hexa que queremos escribir 
      printf(" %02X", bytes[i]);  // imprimimos entonces cada byte con las siguientes condiciones, sale en hexa, en mayuscula, con 2 digitos y separado por un espacio 
   }

   printf(" (pendiente SIS-81)\n");
}

static void comando_rm(char** palabras) {
   // y aca 
   if (!nombre_valido(palabras[1])) {
      return;
   }

   printf("RM %s: pendiente (SIS-81)\n", palabras[1]);
}
