#include "filesystem.h"  // el header de arriba: constantes del formato, el struct del layout y las firmas que definimos aca

#include <stdio.h>    // entrada salida sobre archivos (fopen, fseek, fread, fwrite, fclose)
#include <stdlib.h>   // memoria (malloc, calloc, free)
#include <string.h>   // manejo de memoria y cadenas (memcpy, memcmp, memset, strerror)
#include <errno.h>    // la variable "errno" para saber POR QUE fallo una syscall
#include <unistd.h>   // estandares de UNIX, de aca sacamos "access()" y "usleep()"


//! ─────────────────────────── ESTADO DEL MODULO ───────────────────────────
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


//! ─────────────────────────── HELPERS DE SERIALIZACION ───────────────────────────

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


//! ─────────────────────────── FUNCIONES PURAS: LAS CUENTAS ───────────────────────────

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
