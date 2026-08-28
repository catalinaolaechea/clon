# Protocolo de comunicación — EntrenadOS

Contrato de mensajes entre los 4 módulos. **Este documento manda**: si el código y esta tabla no
coinciden, el bug está en el código. Su implementación vive en `utils/src/utils/protocolo.h`.

> **Estado:** el Check 1 (12/09) está definido y es implementable. Los mensajes de los checks 2 a 4
> tienen **el op_code reservado** para que nadie colisione, pero su payload se define en el check
> que corresponda. Ver "Op_codes reservados".

---

## 1. Header de todo mensaje

Todo mensaje que viaja por un socket tiene la misma forma:

```
┌──────────────┬────────────────────┬─────────────────────────┐
│ op_code      │ payload_size       │ payload                 │
│ uint8_t (1B) │ uint32_t (4B)      │ payload_size bytes      │
└──────────────┴────────────────────┴─────────────────────────┘
       └────── header: 5 bytes ──────┘
```

El receptor lee **siempre** los 5 bytes del header primero; ahí ya sabe exactamente cuántos bytes
faltan y hace un `recv` de tamaño conocido.

- Un mensaje sin payload viaja con `payload_size = 0` y **nada** después del header.
- Al enviar, el stream se arma en **un buffer contiguo y se manda con un solo `send`**, no con tres
  `send` separados.
- **Por qué un header fijo:** TCP es un stream de bytes sin fronteras de mensaje. Sin un tamaño
  explícito por delante no hay forma de saber dónde termina un mensaje y empieza el siguiente.

## 2. Tipos base y serialización

El payload se serializa **campo por campo**, en el orden que fija la tabla de mensajes.

| Tipo | En el stream |
|---|---|
| `uint8_t` | 1 byte |
| `uint32_t` | 4 bytes, orden nativo |
| `string` | `uint32_t longitud` + `longitud` bytes |
| bloque de bytes | `uint32_t tamanio` + `tamanio` bytes |

**Strings:** la `longitud` **incluye el `'\0'`**. Así el receptor hace `malloc(longitud)` +
`memcpy` y ya tiene una cadena válida, sin sumar uno ni acordarse de terminarla. El emisor manda
`strlen(str) + 1`.

**Nunca** se envía un struct de C en crudo (`send(&s, sizeof(s), 0)`): viajarían el padding de
alineación que mete el compilador y cualquier puntero interno como dirección de memoria, que del
otro lado no significa nada. Además se rompe apenas el struct tiene un string o una lista de
longitud variable — que es exactamente lo que necesitan las syscalls de memoria.

**Simetría:** se lee en el mismo orden en que se escribió. Un campo leído fuera de orden **no da
error**: da basura y el bug aparece tres capas más arriba.

## 3. Endianness

Los enteros viajan en **orden nativo**. No se usan `htonl`/`ntohl`.

**Justificación:** todas las máquinas del TP — las de desarrollo y las VMs de la cátedra — son
x86-64 little endian, así que la conversión sería una operación nula en cada campo. Es una
simplificación **consciente**, con su condición de validez explícita: *vale mientras todos los
hosts sean de la misma arquitectura*. Si alguna vez un módulo corriera en un host big endian,
este es el punto exacto que hay que cambiar.

## 4. Reglas de uso del socket

- **Una operación en vuelo por socket.** El que envía una petición espera su respuesta antes de
  mandar otra por el mismo socket. Cuando varios hilos comparten un socket (el Planificador habla
  con la Placa desde los hilos de atención de cada Core), ese socket va protegido por un **mutex
  propio**: si dos hilos intercalan mensajes, las respuestas se cruzan y no hay forma de saber
  cuál es de quién.
- **Cada petición tiene su op_code de respuesta propio** (`ALLOC` → `ALLOC_OK` / `ALLOC_ERROR`).
  No hay un "OK" genérico ni identificadores de correlación: no hacen falta mientras se respete la
  regla de arriba.
- **Desconexión:** un `recv` que devuelve 0 es el peer cerrando ordenadamente. No es un error del
  protocolo: es la señal de desconexión y se propaga como tal.

## 5. Módulos y canales

```c
typedef enum {
    MODULO_INVALIDO     = 0,
    MODULO_PLANIFICADOR = 1,
    MODULO_CORE         = 2,
    MODULO_PLACA        = 3,
    MODULO_STORAGE      = 4
} t_modulo;
```

El `0` es `MODULO_INVALIDO` a propósito: una variable sin inicializar o un buffer en cero se
detectan como módulo inválido en vez de hacerse pasar por el Planificador.

```c
typedef enum {
    CANAL_UNICO     = 0,   // todos los módulos salvo el Core hacia el Planificador
    CANAL_DISPATCH  = 1,   // Core -> Planificador: despacho, syscalls y sus respuestas
    CANAL_INTERRUPT = 2    // Core -> Planificador: sólo interrupciones
} t_canal;
```

El Core mantiene **dos conexiones** al Planificador, al mismo puerto, distinguidas por el canal
que declara en el handshake. El paso *Check Interrupt* del ciclo de instrucción tiene que poder
preguntar "¿me llegó una interrupción?" sin bloquearse, y puede necesitar hacerlo mientras el
canal principal está esperando la respuesta de una syscall.

## 6. Handshake

Todo cliente se identifica apenas se conecta, **antes** de cualquier otro mensaje:

```
Cliente → Servidor : HANDSHAKE        payload = { uint8 modulo, uint8 canal, string identificador }
Servidor → Cliente : HANDSHAKE_OK     payload vacío
                   | HANDSHAKE_ERROR  payload = { string motivo }
```

- El payload del handshake es **siempre el mismo**, para todos los módulos. El `identificador`
  sólo lo usa el Core (es su `[Identificador]` de línea de comandos); los demás mandan **string
  vacío** — `longitud = 1` y un solo byte `'\0'`. Un payload uniforme evita que cada módulo
  parsee un formato distinto.
- Los módulos que no son el Core mandan `canal = CANAL_UNICO`.
- El servidor **decide con esto qué hilo de atención lanza**: la Placa atiende distinto al
  Planificador que a un Core; el Planificador separa el canal de dispatch del de interrupt.
- Quien recibe `HANDSHAKE_ERROR`, o un módulo que no esperaba, **loguea y aborta**. Seguir con un
  socket inservible sólo mueve el error 20 minutos más adelante.

Quién acepta a quién:

| Servidor | Acepta | Rechaza |
|---|---|---|
| Planificador | `MODULO_CORE` (canal dispatch o interrupt) | todo lo demás |
| Placa | `MODULO_PLANIFICADOR`, `MODULO_CORE` | todo lo demás |
| Storage | `MODULO_PLANIFICADOR` | todo lo demás |

El handshake produce los **logs obligatorios** de conexión. Texto exacto del enunciado (págs. 14,
21, 26 y 32), sin los `<>`:

- Planificador / Placa / Storage: `## Módulo: <NOMBRE_MODULO_CONECTADO>`
- Core: `## Conectado a <Planificador/Placa> exitosamente`

El enunciado no fija el formato de `<NOMBRE_MODULO_CONECTADO>`, pero sí escribe los nombres de
módulo **capitalizados** en el log del Core (`Conectado a Planificador exitosamente`). Para que no
haya dos criterios, el nombre sale siempre de `modulo_to_string()` en `utils/src/utils/protocolo.c`:
`Planificador`, `Core`, `Placa`, `Storage`.

## 7. Rangos de op_code

Cada par de módulos tiene su rango, así dos personas pueden agregar mensajes en paralelo sin
pisarse:

| Rango | Uso |
|---|---|
| `0 – 9` | Handshake y control (común a todos los enlaces) |
| `10 – 39` | Planificador ↔ Core |
| `40 – 69` | Core ↔ Placa |
| `70 – 99` | Planificador ↔ Placa |
| `100 – 129` | Planificador ↔ Storage |
| `130 – 255` | Libre |

## 8. Mensajes del Check 1

| op_code | Nombre | Emisor → Receptor | Payload |
|---|---|---|---|
| `1` | `HANDSHAKE` | cliente → servidor | `uint8 modulo`, `uint8 canal`, `string identificador` |
| `2` | `HANDSHAKE_OK` | servidor → cliente | vacío |
| `3` | `HANDSHAKE_ERROR` | servidor → cliente | `string motivo` |
| `4` | `MENSAJE_PRUEBA` | cualquiera → cualquiera | `uint8 origen`, `uint32 secuencia`, `uint32 tamanio_relleno`, `string texto`, `bloque relleno` |
| `5` | `MENSAJE_PRUEBA_ECO` | receptor → emisor | mismos campos, sin modificar |

`MENSAJE_PRUEBA` es el paquete que exige el Check 1. El payload es **deliberadamente mixto** —
un `uint8`, dos `uint32`, un string y un bloque de bytes — porque un mensaje de un solo entero no
prueba nada: no detecta un offset mal avanzado ni una longitud de string mal calculada.

El campo `relleno` existe para poder mandar un payload **> 4 KB** y verificar que el `recv`
loopea hasta completar. Es el caso que se rompe en las VMs y no en localhost, donde el MTU real
parte los paquetes grandes.

El receptor devuelve `MENSAJE_PRUEBA_ECO` con **los mismos campos sin modificar**; el emisor los
compara uno por uno.

## 9. Op_codes reservados para los próximos checks

Los números están **reservados desde ahora** para que nadie los reutilice. El payload se define en
el check correspondiente, salvo los que el enunciado ya fija (marcados con ✔).

### Planificador ↔ Core (10 – 39)

| op_code | Nombre | Sentido | Payload |
|---|---|---|---|
| `10` | `DISPATCH_JOB` | Planif → Core | ✔ `uint32 jid`, `contexto` (14 × `uint32`) |
| `11` | `DEVOLUCION_JOB` | Core → Planif | ✔ `uint32 jid`, `contexto`, `uint8 motivo` |
| `12` | `INTERRUPCION` | Planif → Core | ✔ vacío (canal interrupt) |
| `20 – 29` | `SYSCALL_*` | Core → Planif | una por syscall, en el orden del enunciado |
| `30 – 39` | respuestas de syscall | Planif → Core | — |

`motivo` de la devolución: fin de quantum, page fault, syscall bloqueante, `EXIT`.

### Core ↔ Placa (40 – 69)

| op_code | Nombre | Sentido | Payload |
|---|---|---|---|
| `40` | `FETCH_INSTRUCCION` | Core → Placa | ✔ `uint32 jid`, `uint32 pc` |
| `41` | `RESPUESTA_INSTRUCCION` | Placa → Core | ✔ `string instruccion` |
| `42` | `OBTENER_MARCO` | Core → Placa | ✔ `uint32 jid`, `uint32 nro_pagina` |
| `43` | `RESPUESTA_MARCO` | Placa → Core | ✔ `uint32 nro_marco` (la página queda **lockeada**) |
| `44` | `PAGE_FAULT` | Placa → Core | ✔ `uint32 jid`, `uint32 nro_pagina` |
| `45 – 48` | lectura y escritura de memoria | Core ↔ Placa | — |
| `49` | `DESLOCKEAR_PAGINAS` | Core → Placa | ✔ `uint32 jid` (deslockea **todas** las del Job) |

### Planificador ↔ Placa (70 – 99)

| op_code | Nombre | Sentido |
|---|---|---|
| `70` | `CREAR_JOB` | Planif → Placa (`uint32 jid`, `string archivo_pseudocodigo`) |
| `71` | `FINALIZAR_JOB` | Planif → Placa |
| `72 – 75` | `ALLOC` / `FREE` y sus respuestas | Planif ↔ Placa |
| `76` | `CARGAR_PAGINA` | Planif → Placa (resolución del page fault) |
| `77 – 79` | lectura y escritura de memoria del Job | Planif ↔ Placa |
| `80` | `DESLOCKEAR_PAGINAS` | Planif → Placa (fin de syscall de memoria) |

### Planificador ↔ Storage (100 – 129)

| op_code | Nombre | Sentido |
|---|---|---|
| `100 – 102` | `SAVE_CHECKPOINT` y su respuesta | Planif ↔ Storage |
| `103 – 105` | `LOAD_CHECKPOINT` y su respuesta | Planif ↔ Storage |
| `106 – 107` | `DELETE_CHECKPOINT` y su respuesta | Planif ↔ Storage |

## 10. Estructuras grandes anticipadas

Dos formas se repiten en varios mensajes y conviene que tengan **una sola** implementación en
`utils/`, en lugar de una por módulo.

**Contexto de ejecución** — 14 registros `uint32_t`, **56 bytes**, siempre en este orden:

```
PC, P0, P1, P2, P3, P4, P5, AX, BX, CX, DX, E1, E2, E3
```

Es el mensaje que más veces cruza la red en todo el TP y viaja en los dos sentidos. Se serializa
campo por campo como todo el resto: **no** se hace `memcpy` del struct, aunque sean 14 enteros
homogéneos y tiente.

**Lista de pares `[DirecciónFísica; Tamaño]`** — la usan las syscalls que operan sobre memoria
del Job (`LOAD_BATCH`, `LABEL`, `REPORT`, `SAVE_CHECKPOINT`, `LOAD_CHECKPOINT`), porque una
petición puede abarcar varias páginas y el Core ya tradujo cada una:

```
uint32 cantidad_pares
  por cada par: uint32 direccion_fisica, uint32 tamanio
```

## 11. Cómo agregar un mensaje nuevo

1. Elegir un op_code **libre dentro del rango** del par de módulos que lo usa.
2. Agregarlo al `enum` de `utils/src/utils/protocolo.h`.
3. Documentarlo en la tabla de este archivo: emisor, receptor, campos **en orden** y tipo.
4. Implementar el `serializar` y el `deserializar` en el **mismo commit**.

Agregar un mensaje nuevo no rompe a nadie. **Cambiar uno existente sí**: se avisa al equipo antes
de mergear, porque del otro lado hay alguien leyendo esos campos en ese orden.

## 12. Registro de cambios

| Fecha | Cambio |
|---|---|
| 2026-08-27 | Versión inicial. Header, tipos, handshake y mensajes del Check 1 definidos; rangos y op_codes de checks 2–4 reservados |
