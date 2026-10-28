//! consola del Storage, un bucle que pide lineas por teclado y las procesa. Corre en su propio hilo para no trabar al servidor (el hilo del servidor va atendiendo las peticiones del Planificador)

#include "consola.h"
#include <stdio.h>               // printf
#include <stdlib.h>              // free
#include <readline/readline.h>   // readline(): pide una linea con prompt, edicion y flechitas
#include <readline/history.h>    // add_history(): guarda la linea para recuperarla despues 

void* storage_consola_atender(void* sin_uso) {
   (void) sin_uso;  // esto es para "callar" el warning del compilador, le decimos al mismo que no usarlo es aproposito y -Wextra no tira warning 

   char* linea;  // aca vamos almacenando las lineas que va recibiendo el readline  

   while ((linea = readline(STORAGE_CONSOLA_PROMPT)) != NULL) {  // readline muestra "Storage>" y espera lineas, las devuelve sin el "\n" el final, cuando llega un EOF devuelve NULL y corta
      if(linea[0] != '\0') {  // si mandaron un enter sin querer sin nada, readline devuelve "" que no es NULL, pero no se le contesta ni se la guarda en el historial osea ni entra al if
         add_history(linea);  // lo demas si, lo manda a guardar en "add_history" donde estan las lineas que fue leyendo (no importa si el comando no lo conocemos igual lo guardamos)
         printf("Comando desconocido: %s\n", linea);  // provisorio: por ahora la consola no conoce ningun comando, asi que a todo le contesta esto. En el commit 3 se reemplaza por la funcion que reparte los comandos
      }

      free(linea); // "readline" va pidiendo memoria pero nosotros tenemos que liberarla. La liberamos afuera del if porque si estuviera dentro unicamente abarcamos las lineas que fuimos leyendo, pero los enters sin nada que pudieran haber pasado no los abarcamos, tonc afuera abarcamos los 2 
   }

   return NULL;
}
