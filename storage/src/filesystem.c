#include "filesystem.h"  // el header de arriba: constantes del formato, el struct del layout y las firmas que definimos aca (REPASAR DESPUES Y TERMINAR)

#include <stdio.h>           // entrada salida sobre archivos (fopen, fseek, fread, fwrite, fclose)
#include <stdlib.h>          // memoria (malloc, calloc, free)
#include <string.h>          // manejo de memoria y cadenas (memcpy, memcmp, memset, strerror)
#include <errno.h>           // la variable "errno" para saber POR QUE fallo una syscall
#include <unistd.h>          // estandares de UNIX, de aca sacamos "access()" y "usleep()"
#include "sincronizacion.h"  // es para reconocer el "mutex_filesystem", como vamos a tener que usar un mutex relacionado a leer bloques para distintos hilos 


//! ─────────────────────────── ESTADO DEL MODULO ─────────────────────────── (REPASAR DESPUES Y TERMINAR)
//! Un unico struct "static" con todo lo que el filesystem necesita recordar entre llamadas. "static" a nivel
//! de archivo quiere decir que este nombre NO se ve desde los otros .c: la unica forma de tocar el volumen
//! desde afuera es por las funciones del header. Es el mismo criterio de los mutex en sincronizacion.c, pero
//! al revés: aquellos se exportan con "extern" porque los comparten varios archivos, este se esconde
static struct {
   FILE*            volumen;  // el archivo real del SO anfitrion donde vive el filesystem entero
   t_storage_layout layout;   // el reparto de bloques ya calculado, para no recalcularlo en cada acceso
   uint32_t         retardo;  // RETARDO_ACCESO_BLOQUE en milisegundos, copiado del .config
   t_log*           logger;   // el logger del modulo, para poder reportar errores de disco desde adentro
} fs = { NULL, {0}, 0, NULL };


//! ─────────────────────────── HELPERS DE SERIALIZACION ─────────────────────────── (REPASAR DESPUES Y TERMINAR)

//! mete un uint32_t en un buffer de bytes, en la posicion "offset". Usamos memcpy y no un cast del tipo
//! "*(uint32_t*)(buffer + offset) = valor" porque ese cast asume que la direccion esta alineada a 4 bytes, y
//! nuestros offsets (11, 15, 19) justamente NO lo estan: en algunas arquitecturas eso es un crash directo y
//! en x86 es "comportamiento indefinido" igual. memcpy copia byte por byte y no le importa la alineacion
static void escribir_uint32(uint8_t* buffer, uint32_t offset, uint32_t valor) {
   memcpy(buffer + offset, &valor, sizeof(uint32_t));
}

// el camino de vuelta: saca un uint32_t de un buffer de bytes
static uint32_t leer_uint32(const uint8_t* buffer, uint32_t offset) {
   uint32_t valor;
   memcpy(&valor, buffer + offset, sizeof(uint32_t));
   return valor;
}

static void deserializar_entrada(const uint8_t* cruda, uint32_t nro_entrada, t_storage_entrada_directorio* entrada);


//! ─────────────────────────── FUNCIONES PURAS: LAS CUENTAS ─────────────────────────── (REPASAR DESPUES Y TERMINAR)

t_storage_layout storage_layout_calcular(uint32_t cant_bloques, uint32_t tam_bloque, uint32_t bloques_directorio) {
   t_storage_layout layout = {0};  // arrancamos todo en cero asi ningun campo queda con basura si salimos antes

   layout.tam_bloque   = tam_bloque;
   layout.cant_bloques = cant_bloques;

   layout.bloques_superbloque = 1;  // el bloque 0 y nada mas, pero lo dejamos con nombre para que las sumas de abajo se lean solas

   //! la FAT tiene una entrada de 4 bytes por CADA bloque del filesystem, y tiene que ocupar bloques
   //! enteros: si las entradas no llenan el ultimo bloque, igual se lo lleva completo y el resto queda en
   //! ceros. Por eso es un techo (redondeo para arriba) y no una division comun.
   //! El truco "(a + b - 1) / b" es el techo de a/b hecho con enteros, sin tocar numeros con coma.
   //! Y la cuenta va en uint64_t porque "cant_bloques * 4" con un CANT_BLOQUES muy grande se pasaria del
   //! maximo de un uint32_t y daria un numero chico y absurdo (lo que se llama "overflow")
   uint64_t bytes_fat = (uint64_t) cant_bloques * STORAGE_FAT_BYTES_ENTRADA;
   layout.bloques_fat = (uint32_t) ((bytes_fat + tam_bloque - 1) / tam_bloque);

   layout.bloques_directorio = bloques_directorio;  // este no se calcula, lo dice el .config tal cual

   //! los bloques reservados son los que se lleva la estructura del filesystem: el superbloque, la FAT y el
   //! directorio. Son exactamente los que en la FAT van marcados 0x0FFFFFFE para que nunca se los entregue
   uint64_t reservados = (uint64_t) layout.bloques_superbloque + layout.bloques_fat + layout.bloques_directorio;

   //! y los de datos son TODO lo que sobra. Ojo con esta resta: si la estructura se comio el volumen entero,
   //! "cant_bloques - reservados" con enteros SIN signo no da negativo, da un numero gigante (el uint32_t
   //! "da la vuelta"). Por eso comparamos antes de restar y dejamos 0, que es lo que "es_valido" va a cazar
   layout.bloques_datos = (reservados < cant_bloques) ? (uint32_t) (cant_bloques - reservados) : 0;

   // el mapa: en que bloque arranca cada region. Cada una empieza donde termino la anterior
   layout.primer_bloque_fat        = layout.bloques_superbloque;                                // 1
   layout.primer_bloque_directorio = layout.primer_bloque_fat + layout.bloques_fat;             // 1 + FAT
   layout.primer_bloque_datos      = layout.primer_bloque_directorio + layout.bloques_directorio;  // 1 + FAT + directorio

   layout.bloques_reservados = layout.primer_bloque_datos;  // es el mismo numero: los reservados son justo los que estan ANTES del primer bloque de datos

   //! cuantos checkpoints entran: la region del directorio en bytes, dividido los 32 bytes que pesa una
   //! entrada. La division es exacta porque TAM_BLOQUE es potencia de 2 y arranca en 32 (lo valida config.c)
   layout.entradas_directorio = (uint32_t) (((uint64_t) bloques_directorio * tam_bloque) / STORAGE_DIR_BYTES_ENTRADA);

   layout.tam_volumen = (uint32_t) ((uint64_t) cant_bloques * tam_bloque);  // el tamaño final del archivo

   return layout;  // devolvemos el struct por copia: es chico y asi no hay que liberar nada
}

bool storage_layout_es_valido(t_storage_layout layout, t_log* logger) {
   //! el unico caso de verdad problematico: la estructura ocupa todo y no queda ni un bloque para guardar
   //! datos. Puede pasar con un .config perfectamente "valido" campo por campo (todos los numeros mayores a
   //! cero, TAM_BLOQUE potencia de 2) pero que no cierra cuando se hacen las cuentas juntas. Es justamente
   //! el tipo de error que config.c no puede ver solo, porque ahi todavia no se sabe cuanto pesa la FAT
   if (layout.bloques_datos == 0) {
      log_error(logger, "El layout no cierra: con CANT_BLOQUES=%u, TAM_BLOQUE=%u y BLOQUES_DIRECTORIO=%u la estructura se lleva %u bloques (1 superbloque + %u de FAT + %u de directorio) y no queda ninguno para datos.",
                layout.cant_bloques, layout.tam_bloque, layout.bloques_directorio,
                layout.bloques_reservados, layout.bloques_fat, layout.bloques_directorio);
      return false;
   }

   // si no entra ni un checkpoint, el directorio es inutil. No deberia pasar nunca (TAM_BLOQUE >= 32 y
   // BLOQUES_DIRECTORIO >= 1 lo garantizan), pero si alguien relaja esas validaciones queremos enterarnos
   if (layout.entradas_directorio == 0) {
      log_error(logger, "El layout no cierra: el directorio ocupa %u bloques de %u bytes y no entra ninguna entrada de %d bytes.",
                layout.bloques_directorio, layout.tam_bloque, STORAGE_DIR_BYTES_ENTRADA);
      return false;
   }

   return true;
}

void storage_layout_loguear(t_storage_layout layout, t_log* logger) {
   log_info(logger, "=== Layout del volumen FAT32_TRAIN ===");
   log_info(logger, "Tamaño de bloque:      %u bytes", layout.tam_bloque);
   log_info(logger, "Bloques totales:       %u", layout.cant_bloques);
   log_info(logger, "Tamaño del volumen:    %u bytes", layout.tam_volumen);
   log_info(logger, "Superbloque:           bloque 0 (%u bloque)", layout.bloques_superbloque);
   log_info(logger, "FAT:                   bloques %u..%u (%u bloques, %u entradas de %d bytes)",
            layout.primer_bloque_fat, layout.primer_bloque_fat + layout.bloques_fat - 1,
            layout.bloques_fat, layout.cant_bloques, STORAGE_FAT_BYTES_ENTRADA);
   log_info(logger, "Directorio:            bloques %u..%u (%u bloques, %u entradas de %d bytes)",
            layout.primer_bloque_directorio, layout.primer_bloque_directorio + layout.bloques_directorio - 1,
            layout.bloques_directorio, layout.entradas_directorio, STORAGE_DIR_BYTES_ENTRADA);
   log_info(logger, "Datos:                 bloques %u..%u (%u bloques)",
            layout.primer_bloque_datos, layout.cant_bloques - 1, layout.bloques_datos);
   log_info(logger, "Bloques reservados:    %u (van marcados 0x%08X en la FAT)", layout.bloques_reservados, STORAGE_FAT_RESERVADO);
}


//! ─────────────────────────── ACCESO A BLOQUE ───────────────────────────

//! el UNICO lugar del modulo que mueve el cursor del archivo y lee o escribe un bloque. Todo lo demas pasa
//! por aca, y por eso el retardo esta en un solo lugar. Si "escribir" es true manda el bloque al disco, si
//! es false lo trae a memoria
static bool acceder_bloque(uint32_t nro_bloque, void* buffer, bool escribir) {
   if (fs.volumen == NULL) {  // si nos llaman antes de "storage_filesystem_iniciar" no hay archivo que tocar
      return false;
   }

   if (nro_bloque >= fs.layout.cant_bloques) {  // pedir un bloque que no existe es un bug del que llama, no un error de disco
      log_error(fs.logger, "Acceso a bloque fuera de rango: se pidio el bloque %u y el volumen tiene %u (0..%u).",
                nro_bloque, fs.layout.cant_bloques, fs.layout.cant_bloques - 1);
      return false;
   }

   //! el retardo que pide el enunciado, ANTES de tocar el disco: simula que el acceso a un medio fisico
   //! cuesta tiempo. "usleep" recibe microsegundos y el .config nos da milisegundos, de ahi el * 1000
   if (fs.retardo > 0) {
      usleep(fs.retardo * 1000);
   }

   //! calculamos el byte donde arranca el bloque: numero de bloque por tamaño de bloque. La cuenta va en
   //! uint64_t y despues a "long" porque fseek pide un long: con volumenes grandes un uint32_t se pasaria
   long posicion = (long) ((uint64_t) nro_bloque * fs.layout.tam_bloque);

   if (fseek(fs.volumen, posicion, SEEK_SET) != 0) {  // SEEK_SET = la posicion se cuenta desde el arranque del archivo
      log_error(fs.logger, "No se pudo posicionar en el bloque %u (byte %ld): %s", nro_bloque, posicion, strerror(errno));
      return false;
   }

   // fread y fwrite devuelven CUANTOS elementos movieron, no un codigo de error: les pedimos 1 elemento de
   // tam_bloque bytes, entonces el resultado tiene que ser exactamente 1
   size_t movidos = escribir
                  ? fwrite(buffer, fs.layout.tam_bloque, 1, fs.volumen)
                  : fread(buffer, fs.layout.tam_bloque, 1, fs.volumen);

   if (movidos != 1) {
      log_error(fs.logger, "No se pudo %s el bloque %u: %s", escribir ? "escribir" : "leer", nro_bloque,
                feof(fs.volumen) ? "el archivo termina antes de lo esperado" : strerror(errno));
      return false;
   }

   //! si escribimos, forzamos el vaciado del buffer de la libreria al SO. Sin esto los bytes se quedarian en
   //! un buffer en memoria y un corte del proceso los perderia, que es exactamente lo que el journal del
   //! Check 4 va a necesitar que NO pase
   if (escribir && fflush(fs.volumen) != 0) {
      log_error(fs.logger, "No se pudo bajar a disco el bloque %u: %s", nro_bloque, strerror(errno));
      return false;
   }

   return true;
}

// los dos wrappers publicos. Son finitos a proposito: toda la logica (rango, retardo, posicionamiento,
// chequeo de error) esta una sola vez en "acceder_bloque"
bool storage_bloque_leer(uint32_t nro_bloque, void* destino) {
   return acceder_bloque(nro_bloque, destino, false);
}

bool storage_bloque_escribir(uint32_t nro_bloque, const void* origen) {
   //! el cast saca el "const" porque acceder_bloque usa el mismo puntero para leer y para escribir. En el
   //! camino de escritura el buffer no se modifica, y el "const" de la firma publica es justamente la
   //! promesa de eso a quien nos llama
   return acceder_bloque(nro_bloque, (void*) origen, true);
}


//! ─────────────────────────── FORMATEO ───────────────────────────

//! escribe un bloque SIN el retardo y SIN fseek, aprovechando que el formateo recorre el volumen en orden
//! del bloque 0 al ultimo.
//!
//! Por que nos salteamos el retardo acá: RETARDO_ACCESO_BLOQUE simula el costo de atender una peticion, y el
//! formateo no es una peticion, es la creacion del volumen por unica vez. Con el .config de ejemplo serian
//! 1024 bloques x 100 ms = mas de un minuto y medio en CADA primer arranque, y el modulo no podria ni
//! probarse. Es una decision nuestra, no algo que el enunciado diga: él habla del retardo "ante cada acceso
//! a cualquier bloque del volumen", y lo interpretamos como los accesos de la operatoria normal, que son los
//! que pasan por storage_bloque_leer / storage_bloque_escribir
static bool escribir_bloque_secuencial(const void* buffer, uint32_t nro_bloque) {
   if (fwrite(buffer, fs.layout.tam_bloque, 1, fs.volumen) != 1) {
      log_error(fs.logger, "Falló el formateo al escribir el bloque %u: %s", nro_bloque, strerror(errno));
      return false;
   }
   return true;
}

// arma el bloque 0 en memoria y lo manda al disco: los 23 bytes utiles y el resto en ceros
static bool formatear_superbloque(uint8_t* bloque) {
   memset(bloque, 0, fs.layout.tam_bloque);  //! primero TODO en ceros: asi el padding del final queda limpio sin tener que tocarlo aparte

   // los 11 bytes de la firma, sin el '\0': por eso memcpy con la cantidad exacta y no strcpy, que agregaria el terminador y nos correria un byte todo lo que viene atras
   memcpy(bloque + STORAGE_SB_OFF_FIRMA, STORAGE_FS_FIRMA, STORAGE_FS_FIRMA_BYTES);

   escribir_uint32(bloque, STORAGE_SB_OFF_TAM_BLOQUE,         fs.layout.tam_bloque);
   escribir_uint32(bloque, STORAGE_SB_OFF_CANT_BLOQUES,       fs.layout.cant_bloques);
   escribir_uint32(bloque, STORAGE_SB_OFF_BLOQUES_DIRECTORIO, fs.layout.bloques_directorio);

   return escribir_bloque_secuencial(bloque, 0);
}

// escribe la FAT entera: una entrada por bloque del filesystem, reservadas las de la estructura y libres las de datos
static bool formatear_fat(uint8_t* bloque) {
   uint32_t entradas_por_bloque = fs.layout.tam_bloque / STORAGE_FAT_BYTES_ENTRADA;  // con TAM_BLOQUE=64 son 16

   for (uint32_t i = 0; i < fs.layout.bloques_fat; i++) {  // vamos bloque de FAT por bloque de FAT
      memset(bloque, 0, fs.layout.tam_bloque);  //! en ceros de entrada: sirve para el padding del ultimo bloque (las entradas pueden no llenarlo) y, de paso, 0 es justo STORAGE_FAT_LIBRE

      for (uint32_t j = 0; j < entradas_por_bloque; j++) {
         uint32_t nro_entrada = i * entradas_por_bloque + j;  // la entrada "global", o sea de que bloque del filesystem habla

         if (nro_entrada >= fs.layout.cant_bloques) {
            break;  //! nos pasamos de la cantidad de bloques: lo que sobra del bloque es padding y ya quedo en ceros por el memset
         }

         //! la regla del formateo: las entradas de los bloques que ocupa la estructura (superbloque, FAT y
         //! directorio) van RESERVADO para que el dia que busquemos un bloque libre nunca nos los devuelva;
         //! las de los bloques de datos van LIBRE porque el volumen arranca vacio
         uint32_t valor = (nro_entrada < fs.layout.bloques_reservados) ? STORAGE_FAT_RESERVADO : STORAGE_FAT_LIBRE;
         escribir_uint32(bloque, j * STORAGE_FAT_BYTES_ENTRADA, valor);
      }

      if (!escribir_bloque_secuencial(bloque, fs.layout.primer_bloque_fat + i)) return false;
   }

   return true;
}

//! el directorio y los datos se formatean igual: bloques enteros en cero. En el directorio eso significa
//! "todas las entradas libres", porque el estado 0 es justamente STORAGE_DIR_ESTADO_LIBRE y los demas campos
//! de una entrada libre no importan. En los datos es simplemente dejar el archivo del tamaño que corresponde
static bool formatear_bloques_en_cero(uint8_t* bloque, uint32_t desde, uint32_t cantidad) {
   memset(bloque, 0, fs.layout.tam_bloque);  // una sola vez: el buffer no se modifica entre escrituras

   for (uint32_t i = 0; i < cantidad; i++) {
      if (!escribir_bloque_secuencial(bloque, desde + i)) return false;
   }

   return true;
}

// el formateo completo, en el orden fisico del volumen: superbloque, FAT, directorio y datos
static bool formatear_volumen(void) {
   log_info(fs.logger, "El volumen no existia: lo creamos y lo formateamos.");

   uint8_t* bloque = malloc(fs.layout.tam_bloque);  // un unico buffer del tamaño de un bloque, reusado para todo el formateo
   if (bloque == NULL) {
      log_error(fs.logger, "No hay memoria para el buffer de %u bytes del formateo.", fs.layout.tam_bloque);
      return false;
   }

   bool ok = formatear_superbloque(bloque)
          && formatear_fat(bloque)
          && formatear_bloques_en_cero(bloque, fs.layout.primer_bloque_directorio, fs.layout.bloques_directorio)
          && formatear_bloques_en_cero(bloque, fs.layout.primer_bloque_datos,      fs.layout.bloques_datos);
   //! el "&&" corta en la primera que falle (eso se llama "short-circuit"), asi no seguimos escribiendo
   //! arriba de un volumen que ya quedo a medio hacer

   free(bloque);

   if (!ok) return false;

   if (fflush(fs.volumen) != 0) {  // bajamos a disco todo el formateo de una sola vez
      log_error(fs.logger, "No se pudo bajar a disco el volumen formateado: %s", strerror(errno));
      return false;
   }

   log_info(fs.logger, "Volumen formateado: %u bytes, %u bloques de datos libres y %u entradas de directorio libres.",
            fs.layout.tam_volumen, fs.layout.bloques_datos, fs.layout.entradas_directorio);
   return true;
}


//! ─────────────────────────── VALIDACION DE UN VOLUMEN QUE YA EXISTIA ───────────────────────────

//! si el archivo ya estaba NO se formatea: el enunciado pide que el contenido sobreviva entre ejecuciones, y
//! reformatear seria borrar todos los checkpoints guardados. Lo que si hacemos es desconfiar: leemos el
//! superbloque y chequeamos que sea un volumen nuestro y que describa el MISMO layout que el .config con el
//! que arrancamos ahora
static bool validar_volumen_existente(void) {
   log_info(fs.logger, "El volumen ya existia: no lo formateamos, validamos su superbloque.");

   uint8_t* bloque = malloc(fs.layout.tam_bloque);
   if (bloque == NULL) {
      log_error(fs.logger, "No hay memoria para leer el superbloque (%u bytes).", fs.layout.tam_bloque);
      return false;
   }

   //! este si pasa por storage_bloque_leer, con retardo incluido: es un acceso a bloque de la operatoria
   //! normal, uno solo y al arrancar
   if (!storage_bloque_leer(0, bloque)) {
      log_error(fs.logger, "No se pudo leer el superbloque del volumen existente. Si el archivo es mas chico que un bloque, no es un volumen valido.");
      free(bloque);
      return false;
   }

   // 1) la firma: 11 bytes comparados con memcmp, que compara memoria cruda sin buscar un '\0' al final
   if (memcmp(bloque + STORAGE_SB_OFF_FIRMA, STORAGE_FS_FIRMA, STORAGE_FS_FIRMA_BYTES) != 0) {
      log_error(fs.logger, "El archivo del volumen no tiene la firma \"%s\": no es un filesystem FAT32_TRAIN valido. Revisar PATH_STORAGE o borrar el archivo para que se formatee de nuevo.", STORAGE_FS_FIRMA);
      free(bloque);
      return false;
   }

   // 2) los tres numeros del superbloque contra los del .config
   uint32_t tam_bloque_disco         = leer_uint32(bloque, STORAGE_SB_OFF_TAM_BLOQUE);
   uint32_t cant_bloques_disco       = leer_uint32(bloque, STORAGE_SB_OFF_CANT_BLOQUES);
   uint32_t bloques_directorio_disco = leer_uint32(bloque, STORAGE_SB_OFF_BLOQUES_DIRECTORIO);

   free(bloque);  // ya sacamos los 3 numeros, el buffer no se usa mas

   //! por que esto importa: si alguien cambia TAM_BLOQUE en el .config y el volumen de la corrida anterior
   //! sigue ahi, las cuentas del layout darian distinto y leeriamos el directorio donde en realidad hay
   //! datos. Es mejor frenar con un error claro que trabajar sobre un volumen mal interpretado
   if (tam_bloque_disco != fs.layout.tam_bloque || cant_bloques_disco != fs.layout.cant_bloques || bloques_directorio_disco != fs.layout.bloques_directorio) {
      log_error(fs.logger, "El superbloque no coincide con el .config. En disco: TAM_BLOQUE=%u, CANT_BLOQUES=%u, BLOQUES_DIRECTORIO=%u. En el .config: TAM_BLOQUE=%u, CANT_BLOQUES=%u, BLOQUES_DIRECTORIO=%u.",
                tam_bloque_disco, cant_bloques_disco, bloques_directorio_disco,
                fs.layout.tam_bloque, fs.layout.cant_bloques, fs.layout.bloques_directorio);
      return false;
   }

   log_info(fs.logger, "Superbloque validado: firma \"%s\", TAM_BLOQUE=%u, CANT_BLOQUES=%u, BLOQUES_DIRECTORIO=%u.",
            STORAGE_FS_FIRMA, tam_bloque_disco, cant_bloques_disco, bloques_directorio_disco);
   return true;
}


//! ─────────────────────────── PUNTO DE ENTRADA ───────────────────────────

bool storage_filesystem_iniciar(t_storage_config* config, t_log* logger) {
   //! 1) las cuentas primero, que son puras y no tocan el disco: si el reparto no cierra no tiene sentido ni
   //! abrir el archivo
   t_storage_layout layout = storage_layout_calcular(config->cant_bloques, config->tam_bloque, config->bloques_directorio);

   if (!storage_layout_es_valido(layout, logger)) {
      return false;
   }

   storage_layout_loguear(layout, logger);  // los numeros al log: son el sanity check de todo el modulo

   //! 2) guardamos el estado ANTES de abrir el archivo, porque "acceder_bloque" necesita el layout y el
   //! logger para poder trabajar y para poder reportar errores
   fs.layout  = layout;
   fs.retardo = config->retardo_acceso_bloque;
   fs.logger  = logger;

   //! 3) el archivo existe o no? "access(path, F_OK)" pregunta nada mas que eso, sin abrirlo. De la
   //! respuesta depende todo: si no existe lo creamos y formateamos, y si existe lo respetamos
   bool existia = (access(config->path_volumen, F_OK) == 0);

   //! los modos de fopen: "r+b" abre para leer Y escribir un archivo que ya existe sin tocar su contenido;
   //! "w+b" crea el archivo (y si existiera lo vaciaria, de ahi que nos importe tanto el "existia" de
   //! arriba). La "b" es de binario: en Linux no cambia nada, pero deja dicho que esto no es texto
   fs.volumen = fopen(config->path_volumen, existia ? "r+b" : "w+b");

   if (fs.volumen == NULL) {
      log_error(logger, "No se pudo abrir el archivo del volumen \"%s\": %s", config->path_volumen, strerror(errno));
      fs.logger = NULL;  // dejamos el estado como estaba para no quedar a mitad de camino
      return false;
   }

   // 4) y segun el caso, formateamos o validamos
   bool ok = existia ? validar_volumen_existente() : formatear_volumen();

   if (!ok) {
      storage_filesystem_finalizar();  // si algo fallo cerramos el archivo: no dejamos el modulo arrancado a medias
      return false;
   }

   log_info(logger, "Filesystem listo en \"%s\".", config->path_volumen);
   return true;
}

void storage_filesystem_finalizar(void) {
   if (fs.volumen != NULL) {
      fclose(fs.volumen);  // fclose ya hace el fflush de lo que quede pendiente
      fs.volumen = NULL;   //! lo dejamos en NULL para que un acceso posterior devuelva false en vez de usar un puntero muerto
   }
   fs.logger = NULL;
}

t_storage_layout storage_filesystem_layout(void) {
   return fs.layout;  // por copia: el que llama puede mirarlo todo lo que quiera sin poder modificar el nuestro
}

//todo issue-79
//! esta funcion lo que hace es agarrar los datos que leimos del superbloque en disco para traducirlo hacia el superbloque en memoria y el CPU pueda laburar con el. Lo termina ejecutando el comando "INFO" en la consola de storage 
bool storage_superbloque_leer(t_storage_superbloque* superbloque) {
   uint8_t* bloque = malloc(fs.layout.tam_bloque);  // pedimos un buffer del tamaño de un bloque entero, guiandonos del tamaño final que tendra pidiendoselo a la estructura del layout (calculo final). Ademas la funcion "storage_bloque_leer()" siempre lee de a bloques enteros usando dentro la funcion "acceder_bloque" que es del issue-29   

   if (bloque == NULL) {  // caso puramente del malloc segun lo que devolvio, si es NULL (proceso tiene tope de memoria, o si no queda RAM), ya q preguntando a layout tamaño siempre habra un valor
      log_error(fs.logger, "No hay memoria para leer el superbloque (%u bytes).", fs.layout.tam_bloque);  
      return false;
   }

   //! en Storage tenemos varios hilos tocando "volumen.dat" (cond. carrera), el de la consola ejecutando el comando "INFO" y uno por cada conexion del Planificador, ademas que leer un bloque son 2 pasos primero fseek y fread, podria quedar el cursor mostrando basura. Solucionamos con un candado con una sola llave ("mutex_filesystem" todo lo relacionado a superbloque, FAT, directorio, datos)
   pthread_mutex_lock(&mutex_filesystem);  // pedimos la llave, si esta libre la agarramos y vamos a seccion critica, si otro la tiene nos quedamos esperando sin gastar CPU hasta que se libere 
   bool ok = storage_bloque_leer(0, bloque);  // entramos a seccion critica, ejecutamos "storage_bloque_leer()" y ningun otro hilo puede entrar hasta que liberemos. LEEMOS DE DISCO 
   pthread_mutex_unlock(&mutex_filesystem);  // devuelvo la llave, si habia alguien esperando entra el 

   // finalmente de lo que leimos en disco habiendolo sincronizado con un mutex por tener varios hilos en el mismo archivo, y lo tenemos guardado en buffer "bloque" lo terminamos traduciendo a la estructura de "superbloque" que tenemos en memoria y definimos en el header de filesystem 
   if (ok) {
      // al atributo "firma" le decimos "copia 11 bytes (fs_firma) empezando en bloque + 0 (sb_off)". Usamos "memcpy()" justamente porque tenemos que copiar un bloque de memoria
      memcpy(superbloque->firma, bloque + STORAGE_SB_OFF_FIRMA, STORAGE_FS_FIRMA_BYTES);  
      superbloque->firma[STORAGE_FS_FIRMA_BYTES] = '\0';  // una vez copiado los 11 bytes, volvemos a apuntar a la firma y le agregamos al lugar 12 un '\0' para que C lo trate como texto 
      // demas atributos vamos leyendo del bloque y asignando usando "leer_uint32()" (que va leyendo de a bloques, x eso especificos de bloque, cant. bloques y bloques de directorio)
      superbloque->tam_bloque = leer_uint32(bloque, STORAGE_SB_OFF_TAM_BLOQUE);  
      superbloque->cant_bloques = leer_uint32(bloque, STORAGE_SB_OFF_CANT_BLOQUES);
      superbloque->bloques_directorio = leer_uint32(bloque, STORAGE_SB_OFF_BLOQUES_DIRECTORIO);
   }

   free(bloque);  // finalmente como el buffer que pedimos lo tenemos que liberar una vez que ya lo asignamos a la estructura que tenemos para guardarlo en memoria lo que leimos del disco, liberamos
   return ok;
}

//! Comando LS tiene que mostrar todos los checkpoints guardados (info que vive en el directorio) entonces recorre los 128 bloques y se queda unicamente con las ocupadas, las deserializa (traduce de bytes crudos a valores que guardamos en un struct para tener en memoria) y las devuelva juntas en una lista. Termina haciendo lo mismo que la de arriba pero para entradas de directorio 
t_list* storage_directorio_listar(void) {
   uint8_t* bloque = malloc(fs.layout.tam_bloque);  // buffer de bloque que usaremos para ir leyendo de a bloques del disco, listando de a bloques y asi 
   
   // caso que directamente malloc falle, por lo denotado arriba 
   if (bloque == NULL) {
      log_error(fs.logger, "No hay memoria para leer el directorio (%u bytes).", fs.layout.tam_bloque);
      return NULL;
   }

   t_list* ocupadas = list_create();  // creamos una instancia de lista de las commons donde vamos a almacenar las entradas ocupadas que leemos del disco, crece con cada "list_add()"
   uint32_t entradas_por_bloque = fs.layout.tam_bloque / STORAGE_DIR_BYTES_ENTRADA;  // cuantas entradas de 32 bytes (BYTES_ENTRADA) entran en un bloque => 64/32 = 2. Lo planteamos asi para no tener nada hardcodeado, si el tamaño cambiara a 128 mas tarde seria otro el resultado entonces pero mismo calculo 
   bool ok = true;  // lo vamos a necesitar despues para cambiarle el valor a "false" incluso (linea 472), usando los datos booleanos crudo no nos va a servir (bandera de error se llama)

   //! tenemos los mismos varios hilos tocando "volumen.dat" en Storage, solo q aca el comando es LS. Solucionamos igualmente con un mutex asi evitamos errores de sincronizacion y cond. carrera 
   pthread_mutex_lock(&mutex_filesystem);

   // entramos a la seccion critica, recorro todos los bloques del directorio (0 a 63), la condicion "ok && ..." es una forma de cortar sin return, si algo falla en una iteracion directamente corta aca y no vuelve a iterar 
   for (uint32_t i = 0; ok && i < fs.layout.bloques_directorio; i++) {
      // en cada iteracion mandamos a leer dicho bloque de directorio pero salteando directamente el superbloque (0) y la FAT (1..64), pasando unicamente al bloque de directorio a leer (asi es la estructura de la particion). repasar "storage_bloque_leer()" (aplica "RETARDO", valida rango, hace fseek+fread y devuelve true, si falla loguea y devuelve false) 
      ok = storage_bloque_leer(fs.layout.primer_bloque_directorio + i, bloque);  

      // del bloque de directorio recorro todas sus entradas 
      for (uint32_t j = 0; ok && j < entradas_por_bloque; j++) {

         // por cada bloque de entrada usando "aritmetica de punteros" hacemos que nuestro buffer "bloque" que primero apunta al primer byte vaya apuntando a los demas bloques sumandole n cantidad de bytes (tamaño de un bloque de entrada)
         const uint8_t* cruda = bloque + j * STORAGE_DIR_BYTES_ENTRADA; 

         // el byte 0 de cada entrada es el estado (0 libre, 1 ocupada), de "cruda" que esta apuntando a cada entrada miramos si su estado NO es ocupada seguimos nomas ya que no representa un checkpoint osea un archivo en el bloque de directorio, cuando encontremos una que sea ocupada entonces ahi es que vamos a intentar listarla
         if (cruda[STORAGE_DIR_OFF_ESTADO] != STORAGE_DIR_ESTADO_OCUPADA) {
            continue;
         }

         // con la entrada ocupada entonces creamos una instancia de entrada de directorio y pedimos memoria del tamaño de la misma
         t_storage_entrada_directorio* entrada = malloc(sizeof(t_storage_entrada_directorio));
         
         // raro pero lo mismo, por las dudas que el malloc no responda bien 
         if (entrada == NULL) {  
            log_error(fs.logger, "No hay memoria para la entrada %u del directorio.", i * entradas_por_bloque + j);
            ok = false;
            break;
         }

         // y es aca donde terminamos de pasar la entrada de directorio "en disco" para poder almacenarla en la estructura preparada "en memoria", nomas que la dejamos en otra funcion porque posiblemente la tengamos que reutilizar en el proximo check 
         deserializar_entrada(cruda, i * entradas_por_bloque + j, entrada);
         list_add(ocupadas, entrada);  // y dicha instancia la guardamos en la lista de entradas de la commons (unicamente las ocupadas)
      }
   }

   pthread_mutex_unlock(&mutex_filesystem);  //! cerramos el candado, salimos de la seccion critica, soltamos la llave para que el que este esperando si algun hilo lo hay pueda entrar 
   free(bloque);  // liberamos el buffer que usamos para recorrer el directorio y las entradas del directorio 

   // al final de todo nuestra flag deberia estar todo ok, en caso de no estar todo bien signfica que lo recopilado esta corrupto, mostrar un comando LS estaria mostrando mal, entonces directamente matamos todos los elementos de la lista de "ocupadas" y no devolvemos nada 
   if (!ok) {
      list_destroy_and_destroy_elements(ocupadas, free);
      return NULL;
   }

   return ocupadas;  // si todo funco bien devolvemos la lista, le damos ownership al que llama la funcion, no le liberamos la lista de entradas ocupadas, que la libere el usuario cuando lo considere 
}

//! lo llevamos a una funcion aparte para que no se alargue tanto la funcion de arriba, y creo que se usa en siguiente Check 4 
static void deserializar_entrada(const uint8_t* cruda, uint32_t nro_entrada, t_storage_entrada_directorio* entrada) {
   entrada->nro_entrada = nro_entrada;
   entrada->ocupada     = (cruda[STORAGE_DIR_OFF_ESTADO] == STORAGE_DIR_ESTADO_OCUPADA);

   memcpy(entrada->nombre, cruda + STORAGE_DIR_OFF_NOMBRE, STORAGE_DIR_BYTES_NOMBRE);
   entrada->nombre[STORAGE_DIR_BYTES_NOMBRE - 1] = '\0';

   entrada->tamanio       = leer_uint32(cruda, STORAGE_DIR_OFF_TAMANIO);
   entrada->primer_bloque = leer_uint32(cruda, STORAGE_DIR_OFF_PRIMER_BLOQUE);
}


static t_storage_resultado_lectura buscar_entrada(const char* nombre, uint8_t* bloque, t_storage_entrada_directorio* encontrada, uint32_t* libre) {
   uint32_t entradas_por_bloque = fs.layout.tam_bloque / STORAGE_DIR_BYTES_ENTRADA;

   if (libre != NULL) {
      *libre = fs.layout.entradas_directorio;
   }

   for (uint32_t i = 0; i < fs.layout.bloques_directorio; i++) {
      if (!storage_bloque_leer(fs.layout.primer_bloque_directorio + i, bloque)) {
         return STORAGE_LECTURA_ERROR;
      }

      for (uint32_t j = 0; j < entradas_por_bloque; j++) {
         const uint8_t* cruda = bloque + j * STORAGE_DIR_BYTES_ENTRADA;

         if (cruda[STORAGE_DIR_OFF_ESTADO] != STORAGE_DIR_ESTADO_OCUPADA) {
            if (libre != NULL && *libre == fs.layout.entradas_directorio) {
               *libre = i * entradas_por_bloque + j;
            }
            continue;
         }

         deserializar_entrada(cruda, i * entradas_por_bloque + j, encontrada);

         if (strcmp(encontrada->nombre, nombre) == 0) {
            return STORAGE_LECTURA_OK;
         }
      }
   }

   return STORAGE_LECTURA_NO_EXISTE;
}

static void ubicar_entrada_fat(uint32_t nro_entrada, uint32_t* bloque_fat, uint32_t* offset) {
   uint64_t byte_en_fat = (uint64_t) nro_entrada * STORAGE_FAT_BYTES_ENTRADA;
   *bloque_fat = fs.layout.primer_bloque_fat + (uint32_t) (byte_en_fat / fs.layout.tam_bloque);
   *offset     = (uint32_t) (byte_en_fat % fs.layout.tam_bloque);
}

static bool leer_entrada_fat(uint32_t nro_entrada, uint8_t* bloque, uint32_t* valor) {
   uint32_t bloque_fat;
   uint32_t offset;
   ubicar_entrada_fat(nro_entrada, &bloque_fat, &offset);

   if (!storage_bloque_leer(bloque_fat, bloque)) {
      return false;
   }

   *valor = leer_uint32(bloque, offset);
   log_info(fs.logger, "## Acceso FAT - Entrada: %u - Valor: 0x%08X", nro_entrada, *valor);
   return true;
}

static bool es_bloque_de_datos(uint32_t nro_bloque) {
   return nro_bloque >= fs.layout.primer_bloque_datos && nro_bloque < fs.layout.cant_bloques;
}

static bool seguir_cadena(const t_storage_entrada_directorio* entrada, uint8_t* bloque, uint8_t* destino, uint32_t a_leer) {
   uint32_t actual   = entrada->primer_bloque;
   uint32_t copiados = 0;
   uint32_t vueltas  = 0;

   while (copiados < a_leer) {
      if (!es_bloque_de_datos(actual)) {
         log_error(fs.logger, "Cadena rota en el checkpoint \"%s\": el bloque %u no es un bloque de datos (%u..%u).",
                   entrada->nombre, actual, fs.layout.primer_bloque_datos, fs.layout.cant_bloques - 1);
         return false;
      }

      vueltas++;
      if (vueltas > fs.layout.cant_bloques) {
         log_error(fs.logger, "Cadena rota en el checkpoint \"%s\": se recorrieron mas de %u bloques sin llegar al final (hay un ciclo en la FAT).",
                   entrada->nombre, fs.layout.cant_bloques);
         return false;
      }

      if (!storage_bloque_leer(actual, bloque)) {
         return false;
      }
      log_info(fs.logger, "## Acceso Bloque - Checkpoint: %s - Bloque: %u", entrada->nombre, actual);

      uint32_t faltan         = a_leer - copiados;
      uint32_t de_este_bloque = (faltan < fs.layout.tam_bloque) ? faltan : fs.layout.tam_bloque;
      memcpy(destino + copiados, bloque, de_este_bloque);
      copiados += de_este_bloque;

      if (copiados == a_leer) {
         break;
      }

      uint32_t valor;
      if (!leer_entrada_fat(actual, bloque, &valor)) {
         return false;
      }

      if ((valor & STORAGE_FAT_MASCARA) == STORAGE_FAT_EOF) {
         log_error(fs.logger, "Cadena rota en el checkpoint \"%s\": la cadena termina en el bloque %u y todavia faltan %u bytes de los %u que dice el directorio.",
                   entrada->nombre, actual, a_leer - copiados, entrada->tamanio);
         return false;
      }

      actual = valor & STORAGE_FAT_MASCARA;
   }

   return true;
}

t_storage_resultado_lectura storage_checkpoint_leer(const char* nombre, uint32_t max_bytes, uint8_t** contenido, uint32_t* leidos) {
   uint8_t* bloque = malloc(fs.layout.tam_bloque);
   if (bloque == NULL) {
      log_error(fs.logger, "No hay memoria para leer el checkpoint \"%s\" (%u bytes).", nombre, fs.layout.tam_bloque);
      return STORAGE_LECTURA_ERROR;
   }

   uint8_t* datos  = NULL;
   uint32_t a_leer = 0;
   t_storage_entrada_directorio entrada;

   pthread_mutex_lock(&mutex_filesystem);

   t_storage_resultado_lectura resultado = buscar_entrada(nombre, bloque, &entrada, NULL);

   if (resultado == STORAGE_LECTURA_OK) {
      a_leer = (max_bytes < entrada.tamanio) ? max_bytes : entrada.tamanio;
      datos  = malloc(a_leer > 0 ? a_leer : 1);

      if (datos == NULL) {
         log_error(fs.logger, "No hay memoria para los %u bytes del checkpoint \"%s\".", a_leer, nombre);
         resultado = STORAGE_LECTURA_ERROR;
      } else if (!seguir_cadena(&entrada, bloque, datos, a_leer)) {
         resultado = STORAGE_LECTURA_ERROR;
      }
   }

   pthread_mutex_unlock(&mutex_filesystem);
   free(bloque);

   if (resultado != STORAGE_LECTURA_OK) {
      free(datos);
      return resultado;
   }

   *contenido = datos;
   *leidos    = a_leer;
   return STORAGE_LECTURA_OK;
}


static void serializar_entrada(const t_storage_entrada_directorio* entrada, uint8_t* cruda) {
   memset(cruda, 0, STORAGE_DIR_BYTES_ENTRADA);

   cruda[STORAGE_DIR_OFF_ESTADO] = entrada->ocupada ? STORAGE_DIR_ESTADO_OCUPADA : STORAGE_DIR_ESTADO_LIBRE;
   memcpy(cruda + STORAGE_DIR_OFF_NOMBRE, entrada->nombre, strnlen(entrada->nombre, STORAGE_DIR_BYTES_NOMBRE - 1));

   escribir_uint32(cruda, STORAGE_DIR_OFF_TAMANIO, entrada->tamanio);
   escribir_uint32(cruda, STORAGE_DIR_OFF_PRIMER_BLOQUE, entrada->primer_bloque);
}

static bool escribir_entrada_directorio(const t_storage_entrada_directorio* entrada, uint8_t* bloque) {
   uint32_t entradas_por_bloque = fs.layout.tam_bloque / STORAGE_DIR_BYTES_ENTRADA;
   uint32_t nro_bloque          = fs.layout.primer_bloque_directorio + entrada->nro_entrada / entradas_por_bloque;
   uint32_t offset              = (entrada->nro_entrada % entradas_por_bloque) * STORAGE_DIR_BYTES_ENTRADA;

   if (!storage_bloque_leer(nro_bloque, bloque)) {
      return false;
   }

   serializar_entrada(entrada, bloque + offset);
   return storage_bloque_escribir(nro_bloque, bloque);
}

static bool escribir_entrada_fat(uint32_t nro_entrada, uint32_t valor, uint8_t* bloque) {
   uint32_t bloque_fat;
   uint32_t offset;
   ubicar_entrada_fat(nro_entrada, &bloque_fat, &offset);

   if (!storage_bloque_leer(bloque_fat, bloque)) {
      return false;
   }

   escribir_uint32(bloque, offset, valor);

   if (!storage_bloque_escribir(bloque_fat, bloque)) {
      return false;
   }

   log_info(fs.logger, "## Acceso FAT - Entrada: %u - Valor: 0x%08X", nro_entrada, valor);
   return true;
}

static bool recorrer_cadena(const t_storage_entrada_directorio* entrada, uint8_t* bloque, uint32_t* cadena, uint32_t* largo) {
   *largo = 0;

   if (entrada->tamanio == 0) {
      return true;
   }

   uint32_t actual = entrada->primer_bloque;

   while (true) {
      if (!es_bloque_de_datos(actual)) {
         log_error(fs.logger, "Cadena rota en el checkpoint \"%s\": el bloque %u no es un bloque de datos (%u..%u).",
                   entrada->nombre, actual, fs.layout.primer_bloque_datos, fs.layout.cant_bloques - 1);
         return false;
      }

      if (*largo == fs.layout.bloques_datos) {
         log_error(fs.logger, "Cadena rota en el checkpoint \"%s\": se recorrieron mas de %u bloques sin llegar al final (hay un ciclo en la FAT).",
                   entrada->nombre, fs.layout.bloques_datos);
         return false;
      }

      cadena[(*largo)++] = actual;

      uint32_t valor;
      if (!leer_entrada_fat(actual, bloque, &valor)) {
         return false;
      }

      if ((valor & STORAGE_FAT_MASCARA) == STORAGE_FAT_EOF) {
         return true;
      }

      actual = valor & STORAGE_FAT_MASCARA;
   }
}

static bool buscar_bloques_libres(uint8_t* bloque, uint32_t cantidad, uint32_t* libres, uint32_t* encontrados) {
   *encontrados = 0;

   for (uint32_t nro = fs.layout.primer_bloque_datos; nro < fs.layout.cant_bloques && *encontrados < cantidad; nro++) {
      uint32_t bloque_fat;
      uint32_t offset;
      ubicar_entrada_fat(nro, &bloque_fat, &offset);

      if ((nro == fs.layout.primer_bloque_datos || offset == 0) && !storage_bloque_leer(bloque_fat, bloque)) {
         return false;
      }

      uint32_t valor = leer_uint32(bloque, offset);
      log_info(fs.logger, "## Acceso FAT - Entrada: %u - Valor: 0x%08X", nro, valor);

      if ((valor & STORAGE_FAT_MASCARA) == STORAGE_FAT_LIBRE) {
         libres[(*encontrados)++] = nro;
      }
   }

   return true;
}

static t_storage_operacion* operacion_crear(void) {
   t_storage_operacion* operacion = calloc(1, sizeof(t_storage_operacion));

   if (operacion == NULL) {
      return NULL;
   }

   operacion->fat     = list_create();
   operacion->bloques = list_create();
   return operacion;
}

static void mod_bloque_destruir(void* elemento) {
   t_storage_mod_bloque* mod = elemento;
   free(mod->contenido);
   free(mod);
}

static void operacion_destruir(t_storage_operacion* operacion) {
   if (operacion == NULL) {
      return;
   }

   list_destroy_and_destroy_elements(operacion->fat, free);
   list_destroy_and_destroy_elements(operacion->bloques, mod_bloque_destruir);
   free(operacion);
}

static bool agregar_mod_fat(t_storage_operacion* operacion, uint32_t nro_entrada, uint32_t valor) {
   t_storage_mod_fat* mod = malloc(sizeof(t_storage_mod_fat));

   if (mod == NULL) {
      log_error(fs.logger, "No hay memoria para anotar la modificacion de la entrada %u de la FAT.", nro_entrada);
      return false;
   }

   mod->nro_entrada = nro_entrada;
   mod->valor       = valor;
   list_add(operacion->fat, mod);
   return true;
}

static bool agregar_mod_bloque(t_storage_operacion* operacion, uint32_t nro_bloque, const uint8_t* datos, uint32_t cantidad) {
   t_storage_mod_bloque* mod = malloc(sizeof(t_storage_mod_bloque));
   uint8_t* contenido        = calloc(1, fs.layout.tam_bloque);

   if (mod == NULL || contenido == NULL) {
      log_error(fs.logger, "No hay memoria para anotar la escritura del bloque %u (%u bytes).", nro_bloque, fs.layout.tam_bloque);
      free(mod);
      free(contenido);
      return false;
   }

   memcpy(contenido, datos, cantidad);
   mod->nro_bloque = nro_bloque;
   mod->contenido  = contenido;
   list_add(operacion->bloques, mod);
   return true;
}

static bool aplicar_operacion(const t_storage_operacion* operacion, uint8_t* bloque) {
   for (int i = 0; i < list_size(operacion->bloques); i++) {
      t_storage_mod_bloque* mod = list_get(operacion->bloques, i);

      if (!storage_bloque_escribir(mod->nro_bloque, mod->contenido)) {
         return false;
      }
      log_info(fs.logger, "## Acceso Bloque - Checkpoint: %s - Bloque: %u", operacion->entrada.nombre, mod->nro_bloque);
   }

   for (int i = 0; i < list_size(operacion->fat); i++) {
      t_storage_mod_fat* mod = list_get(operacion->fat, i);

      if (!escribir_entrada_fat(mod->nro_entrada, mod->valor, bloque)) {
         return false;
      }
   }

   return escribir_entrada_directorio(&operacion->entrada, bloque);
}

static t_storage_resultado_escritura planificar_guardado(const char* nombre, const uint8_t* contenido, uint32_t tamanio,
                                                         uint8_t* bloque, uint32_t* cadena, t_storage_operacion* operacion) {
   t_storage_entrada_directorio entrada;
   uint32_t libre;
   uint32_t viejos = 0;

   t_storage_resultado_lectura busqueda = buscar_entrada(nombre, bloque, &entrada, &libre);

   if (busqueda == STORAGE_LECTURA_ERROR) {
      return STORAGE_ESCRITURA_ERROR;
   }

   if (busqueda == STORAGE_LECTURA_OK) {
      if (!recorrer_cadena(&entrada, bloque, cadena, &viejos)) {
         return STORAGE_ESCRITURA_ERROR;
      }
   } else if (libre == fs.layout.entradas_directorio) {
      log_error(fs.logger, "## ERROR - Directorio lleno al intentar guardar: %s", nombre);
      return STORAGE_ESCRITURA_DIRECTORIO_LLENO;
   } else {
      entrada.nro_entrada = libre;
   }

   uint32_t necesarios = tamanio / fs.layout.tam_bloque + (tamanio % fs.layout.tam_bloque != 0);

   if (necesarios > viejos) {
      uint32_t encontrados;

      if (!buscar_bloques_libres(bloque, necesarios - viejos, cadena + viejos, &encontrados)) {
         return STORAGE_ESCRITURA_ERROR;
      }

      if (encontrados < necesarios - viejos) {
         log_error(fs.logger, "## ERROR - Falta de espacio en FAT al intentar guardar: %s", nombre);
         return STORAGE_ESCRITURA_SIN_ESPACIO;
      }
   }

   for (uint32_t i = 0; i < necesarios; i++) {
      uint32_t desde     = i * fs.layout.tam_bloque;
      uint32_t cantidad  = (tamanio - desde < fs.layout.tam_bloque) ? tamanio - desde : fs.layout.tam_bloque;
      uint32_t siguiente = (i + 1 < necesarios) ? cadena[i + 1] : STORAGE_FAT_EOF;

      if (!agregar_mod_bloque(operacion, cadena[i], contenido + desde, cantidad) ||
          !agregar_mod_fat(operacion, cadena[i], siguiente)) {
         return STORAGE_ESCRITURA_ERROR;
      }
   }

   for (uint32_t i = necesarios; i < viejos; i++) {
      if (!agregar_mod_fat(operacion, cadena[i], STORAGE_FAT_LIBRE)) {
         return STORAGE_ESCRITURA_ERROR;
      }
   }

   entrada.ocupada = true;
   memset(entrada.nombre, 0, STORAGE_DIR_BYTES_NOMBRE);
   memcpy(entrada.nombre, nombre, strnlen(nombre, STORAGE_DIR_BYTES_NOMBRE - 1));
   entrada.tamanio       = tamanio;
   entrada.primer_bloque = (necesarios > 0) ? cadena[0] : 0;

   operacion->entrada = entrada;
   return STORAGE_ESCRITURA_OK;
}

t_storage_resultado_escritura storage_checkpoint_guardar(const char* nombre, const uint8_t* contenido, uint32_t tamanio) {
   if (strlen(nombre) > STORAGE_DIR_BYTES_NOMBRE - 1) {
      log_error(fs.logger, "No se puede guardar \"%s\": el nombre tiene %zu caracteres y el maximo es %d.",
                nombre, strlen(nombre), STORAGE_DIR_BYTES_NOMBRE - 1);
      return STORAGE_ESCRITURA_ERROR;
   }

   uint8_t* bloque                = malloc(fs.layout.tam_bloque);
   uint32_t* cadena               = malloc(fs.layout.bloques_datos * sizeof(uint32_t));
   t_storage_operacion* operacion = operacion_crear();

   if (bloque == NULL || cadena == NULL || operacion == NULL) {
      log_error(fs.logger, "No hay memoria para guardar el checkpoint \"%s\".", nombre);
      free(bloque);
      free(cadena);
      operacion_destruir(operacion);
      return STORAGE_ESCRITURA_ERROR;
   }

   pthread_mutex_lock(&mutex_filesystem);

   t_storage_resultado_escritura resultado = planificar_guardado(nombre, contenido, tamanio, bloque, cadena, operacion);

   if (resultado == STORAGE_ESCRITURA_OK && !aplicar_operacion(operacion, bloque)) {
      resultado = STORAGE_ESCRITURA_ERROR;
   }

   pthread_mutex_unlock(&mutex_filesystem);

   free(bloque);
   free(cadena);
   operacion_destruir(operacion);
   return resultado;
}
