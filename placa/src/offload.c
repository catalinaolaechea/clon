#include "offload.h"
#include <fcntl.h>   // open() y O_RDWR, O_CREAT, O_TRUNC
#include <unistd.h>  // ftruncate() y close()
#include <errno.h>   // errno
#include <string.h>  // strerror()


//offload
int archivo_offload;
t_bitarray* slots_offload;
uint32_t cant_slots = 0;

pthread_mutex_t mutex_offload = PTHREAD_MUTEX_INITIALIZER;


static bool crear_archivo_offload(){
    
    // open() le pide al sistema operativo que abra el archivo de PATH_OFFLOAD.
    // Devuelve un número entero (file descriptor) que identifica al archivo abierto,
    // o -1 si falló. Lo guardamos en el global para usarlo después en leer/escribir.
    //   O_RDWR  -> se abre para leer y escribir
    //   O_CREAT -> si el archivo no existe, lo crea
    //   O_TRUNC -> si ya existía, lo vacía (queda en 0 bytes): se descarta la corrida anterior
    //   0644    -> permisos del archivo si lo crea (dueño lee/escribe, el resto solo lee)
    archivo_offload = open(PATH_OFFLOAD, O_RDWR | O_CREAT | O_TRUNC, 0644);
    
    //si open devuelve un numero negativo, no se pudo abrir
    if(archivo_offload < 0){

        log_error(placa_logger, "No se pudo abrir el Offload %s: %s", PATH_OFFLOAD, strerror(errno));

        return false;
    }

    //ftruncate() le cambia el tamaño a TAM_OFFLOAD bytes; los bytes nuevos valen 0

    if(ftruncate(archivo_offload, TAM_OFFLOAD) < 0){
        log_error(placa_logger, "No se pudo dimensionar el Offload a %d bytes: %s", TAM_OFFLOAD, strerror(errno));
        close(archivo_offload);
        return false;
    }

    return true;
}

static bool crear_registro_slots(){
    //cuantos slots entran en el archivo, uno por pagina 
    //65536/ 64 = 1024

    cant_slots = TAM_OFFLOAD/TAM_PAGINA;

    //bytes necesarios para 1 bit por slot, redondeado para arriba

    size_t bytes_bitarray = (cant_slots + 7)/8;

    //calloc lo deja en 0 todos los slots libres
    char* bits = calloc(bytes_bitarray, 1);

    if(bits == NULL){
        log_error(placa_logger,"No se pudo reservar el registro de slots del Offload");
        return false;
    }

    //LSB_FIRST, el slots 0 es el bit menos significativo del primer byte

    slots_offload = bitarray_create_with_mode(bits,bytes_bitarray,LSB_FIRST);
    return true;

}

// [Enunciado pág. 22 - El Offload] + [pág. 25 - PATH_OFFLOAD / TAM_OFFLOAD]
// Crea el archivo de Offload al arrancar la placa, con TAM_OFFLOAD bytes.
// Devuelve false si no se pudo; en ese caso la placa no arranca.

bool inicializador_offload(){
    
    if(!crear_archivo_offload()) return false;

    //el archivo ya esta creado, falta el registro de que slots están ocupados
    if(!crear_registro_slots()){
        close(archivo_offload);
        return false;
    }

    log_debug(placa_logger, "Offload: %s, %d bytes", PATH_OFFLOAD, TAM_OFFLOAD);
    return true;

    

}


// [Enunciado pág. 22 - El Offload] + [pág. 25 - RETARDO_OFFLOAD]
// Copia una página (TAM_PAGINA bytes) desde "origen" al slot indicado del archivo.

bool escribir_pagina_offload(uint32_t slot, void* origen){

    //un slot fuera de rango escribiria mas alla del final del archivo
    if(slot >= cant_slots){
        log_error(placa_logger, "Offload - Escritura en slot inexistente: %u (hay %u)", slot, cant_slots);
        return false;
    }

    // dónde empieza el slot dentro del archivo: slot 0 -> byte 0, slot 1 -> byte 64, ...
    // el (off_t) evita que la multiplicación desborde un int si el Offload es grande
    off_t posicion = (off_t)slot * TAM_PAGINA;

    //un solo acceso al Offload a la vez 
    pthread_mutex_lock(&mutex_offload);

        log_debug(placa_logger, "Offload - Escritura - Slot: %u - inicio", slot);

        // simula que el disco es lento; va adentro del mutex para que el siguiente espere también esto
        usleep(RETARDO_OFFLOAD * 1000);

        // escribe TAM_PAGINA bytes de "origen" en el archivo, a partir de "posicion"
        // pwrite devuelve cuántos bytes escribió, o -1 si falló
        ssize_t escritos = pwrite(archivo_offload, origen, TAM_PAGINA, posicion);

        log_debug(placa_logger, "Offload - Escritura - Slot: %u - fin", slot);

    pthread_mutex_unlock(&mutex_offload);

    // si no se escribió la página entera, se avisa con el motivo
    if(escritos != TAM_PAGINA){
        log_error(placa_logger, "Offload - No se pudo escribir el slot %u: %s", slot, strerror(errno));
        return false;
    }

    return true;

}

//Copia una página (TAM_PAGINA bytes) desde el slot indicado del archivo hacia "destino".
//"destino" tiene que tener lugar para TAM_PAGINA bytes.
//es lo mismo que escribir
bool leer_pagina_offload(uint32_t slot, void* destino){

    if(slot >= cant_slots){
        log_error(placa_logger, "Offload - Lectura de slot inexistente: %u (hay %u)", slot, cant_slots);
        return false;
    }

    off_t posicion = (off_t)slot * TAM_PAGINA;

    // un solo acceso al Offload a la vez
    pthread_mutex_lock(&mutex_offload);

        log_debug(placa_logger, "Offload - Lectura - Slot: %u - inicio", slot);

        usleep(RETARDO_OFFLOAD * 1000);

        ssize_t leidos = pread(archivo_offload, destino, TAM_PAGINA, posicion);

        log_debug(placa_logger, "Offload - Lectura - Slot: %u - fin", slot);

    pthread_mutex_unlock(&mutex_offload);

    if(leidos != TAM_PAGINA){
        log_error(placa_logger, "Offload - No se pudo leer el slot %u: %s", slot, strerror(errno));
        return false;
    }

    return true;
}

