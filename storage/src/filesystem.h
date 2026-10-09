// header del filesystem "FAT32_TRAIN". Aca vive el "contrato" de todo lo que tiene que ver con el
// volumen: las constantes que fija el enunciado (firma, valores de la FAT, tamaños de las estructuras),
// el struct con el layout ya calculado, y las firmas de las funciones que definimos en el .c

#ifndef STORAGE_FILESYSTEM_H_  // "include guards" igual que en config.h
#define STORAGE_FILESYSTEM_H_

#include <stdbool.h>      // el tipo bool, que en C no viene por default
#include <stdint.h>       // los tipos de ancho fijo uint8_t y uint32_t
#include <commons/log.h>  // el tipo "t_log" para poder loguear
#include "config.h"       // el tipo "t_storage_config", porque de ahi salen TAM_BLOQUE, CANT_BLOQUES y demas


//! ─────────────────────────── SUPERBLOQUE ───────────────────────────
// El enunciado pide una estructura CONTIGUA y EXACTA en el bloque 0: 11 bytes de firma + 3 enteros de
// 4 bytes = 23 bytes, y el resto del bloque relleno con ceros (reservado para uso futuro)

#define STORAGE_FS_FIRMA        "FAT32_TRAIN"  // "magic number": sirve para reconocer que el archivo ES un volumen nuestro y no cualquier otra cosa
#define STORAGE_FS_FIRMA_BYTES  11             // ojo: son 11 bytes EXACTOS y SIN el '\0' del final, por eso no usamos sizeof() que daria 12

// offsets (la posicion en bytes contando desde el arranque del bloque) de cada campo dentro del superbloque
#define STORAGE_SB_OFF_FIRMA               0   // bytes 0..10
#define STORAGE_SB_OFF_TAM_BLOQUE          11  // bytes 11..14
#define STORAGE_SB_OFF_CANT_BLOQUES        15  // bytes 15..18
#define STORAGE_SB_OFF_BLOQUES_DIRECTORIO  19  // bytes 19..22
#define STORAGE_SB_BYTES_UTILES            23  // 11 + 4 + 4 + 4, lo que de verdad se usa; de aca hasta TAM_BLOQUE va padding en ceros

//! por que escribimos el superbloque a mano con "memcpy" a estos offsets y NO declaramos un struct de C
//! que se escriba de una? Porque el compilador mete "padding" (bytes de relleno invisibles) entre la firma
//! de 11 bytes y el primer uint32_t, para que el entero arranque en una direccion multiplo de 4. O sea que
//! un struct comun pesaria 24 bytes y no 23, y el entero caeria en el byte 12 en vez del 11: el layout en
//! disco dejaria de ser el que pide el enunciado. Se podria forzar con __attribute__((packed)), pero los
//! offsets a mano son explicitos y no dependen de ninguna extension del compilador


//! ─────────────────────────── FILE ALLOCATION TABLE ───────────────────────────
// Una entrada uint32_t por CADA bloque del filesystem. Cada entrada dice cual es el bloque siguiente del
// checkpoint al que pertenece: es una lista enlazada, pero guardada en una tabla aparte en vez de dentro
// de los datos

#define STORAGE_FAT_BYTES_ENTRADA  4  // sizeof(uint32_t), lo dejamos con nombre para que se lea el "por que" en las cuentas

#define STORAGE_FAT_LIBRE       0x00000000u  // el bloque esta libre y se puede entregar
#define STORAGE_FAT_EOF         0x0FFFFFFFu  // "end of file": ultimo bloque del checkpoint, la cadena termina aca
#define STORAGE_FAT_RESERVADO   0x0FFFFFFEu  // bloque de la estructura (superbloque, FAT o directorio): NUNCA se puede considerar libre

//! de los 32 bits de cada entrada, solo los 28 MENOS significativos son el numero de bloque: los 4 mas
//! altos quedan reservados. Esta mascara saca el numero de bloque de una entrada, y la definimos desde
//! ahora aunque todavia no haya cadenas que recorrer, porque es la regla del formato
#define STORAGE_FAT_MASCARA     0x0FFFFFFFu


//! ─────────────────────────── ENTRADA DE DIRECTORIO ───────────────────────────
// Un unico directorio raiz, con una entrada por checkpoint, pegadas una atras de la otra desde el primer
// byte de la region. Cada entrada pesa 32 bytes EXACTOS

#define STORAGE_DIR_OFF_ESTADO        0   // byte 0:      uint8_t, 0 = libre / 1 = ocupada
#define STORAGE_DIR_OFF_NOMBRE        1   // bytes 1..23: cadena terminada en '\0'
#define STORAGE_DIR_OFF_TAMANIO       24  // bytes 24..27: uint32_t, tamaño del checkpoint en bytes
#define STORAGE_DIR_OFF_PRIMER_BLOQUE 28  // bytes 28..31: uint32_t, primer bloque de datos de la cadena

#define STORAGE_DIR_BYTES_ENTRADA  32  // 1 + 23 + 4 + 4. Es el mismo 32 de STORAGE_TAM_BLOQUE_MINIMO en config.h: si un bloque fuera mas chico, una entrada no entraria entera en un bloque
#define STORAGE_DIR_BYTES_NOMBRE   23  // incluyendo el '\0', o sea 22 caracteres utiles como maximo

#define STORAGE_DIR_ESTADO_LIBRE    0
#define STORAGE_DIR_ESTADO_OCUPADA  1


//! ─────────────────────────── LAYOUT DEL VOLUMEN ───────────────────────────
// Todas las cuentas del reparto de bloques, resueltas UNA sola vez al arrancar y guardadas acá. La idea es
// no andar recalculando "donde empieza el directorio" cada vez que se lo necesita, y sobre todo no
// hardcodear ningun numero: todo sale del .config

typedef struct {
   // lo que vino del .config, lo guardamos adentro para que el layout se explique solo
   uint32_t tam_bloque;
   uint32_t cant_bloques;

   // cuantos bloques se lleva cada region
   uint32_t bloques_superbloque;  // siempre 1 (el bloque 0), pero con nombre para que las cuentas se lean
   uint32_t bloques_fat;          // techo(cant_bloques * 4 / tam_bloque)
   uint32_t bloques_directorio;   // tal cual lo pide BLOQUES_DIRECTORIO
   uint32_t bloques_datos;        // todo lo que sobra

   // en que numero de bloque arranca cada region (el "mapa" para poder hacer fseek)
   uint32_t primer_bloque_fat;         // 1
   uint32_t primer_bloque_directorio;  // 1 + bloques_fat
   uint32_t primer_bloque_datos;       // 1 + bloques_fat + bloques_directorio

   uint32_t bloques_reservados;    // 1 + bloques_fat + bloques_directorio, o sea el primer_bloque_datos. Son los que van marcados 0x0FFFFFFE en la FAT
   uint32_t entradas_directorio;   // cuantos checkpoints como maximo entran: bloques_directorio * tam_bloque / 32
   uint32_t tam_volumen;           // cant_bloques * tam_bloque, el tamaño final del archivo en bytes
} t_storage_layout;


//! ─────────────────────────── FUNCIONES PURAS (las cuentas) ───────────────────────────
//! "pura" quiere decir que con los mismos 3 numeros de entrada devuelve siempre lo mismo, sin leer ni
//! escribir nada de afuera: ni archivos, ni variables globales, ni el .config. Eso la hace trivial de
//! testear a mano (le paso 1024/64/64 y tiene que darme 64 de FAT y 895 de datos) y es el motivo por el que
//! recibe los valores por parametro en vez de ir a buscarlos al config

t_storage_layout storage_layout_calcular(uint32_t cant_bloques, uint32_t tam_bloque, uint32_t bloques_directorio);

// nos dice si ese reparto cierra, o si el superbloque + la FAT + el directorio se comieron todo el volumen
// y no quedo ni un bloque de datos. Si da false, loguea el motivo concreto
bool storage_layout_es_valido(t_storage_layout layout, t_log* logger);

// deja el reparto escrito en el log: son los numeros que sirven de "sanity check" de todo el modulo
void storage_layout_loguear(t_storage_layout layout, t_log* logger);


//! ─────────────────────────── FUNCIONES CON ESTADO (el disco) ───────────────────────────

// Punto de entrada del modulo. Si el archivo del volumen no existe lo crea y lo formatea (superbloque +
// FAT con los datos libres + directorio con todas las entradas libres). Si YA existe NO lo toca: valida la
// firma y que el .config coincida con lo que dice el superbloque. Devuelve false si algo no cierra
bool storage_filesystem_iniciar(t_storage_config* config, t_log* logger);

// cierra el archivo del volumen y deja el modulo como al principio
void storage_filesystem_finalizar(void);

// devuelve una COPIA del layout que quedo vigente, para que el resto del modulo pueda consultarlo sin
// tocar el estado interno
t_storage_layout storage_filesystem_layout(void);

//! el UNICO acceso a bloque del modulo pasa por estas dos funciones, y las dos aplican RETARDO_ACCESO_BLOQUE.
//! El enunciado pide ese retardo ante cada acceso a cualquier bloque del volumen, sea de datos, del
//! directorio o de la FAT: si dejaramos fseek/fread desparramados por todo el modulo, el dia que haya que
//! tocar el retardo (o agregar un log de acceso) habria que acordarse de todos los lugares
bool storage_bloque_leer(uint32_t nro_bloque, void* destino);
bool storage_bloque_escribir(uint32_t nro_bloque, const void* origen);

#endif // para cerrar el #ifndef
