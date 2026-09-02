# `utils/` — Logger y Config compartidos (UTL-05)

Librería chica para que los 4 módulos de EntrenadOS (Planificador, Core, Placa, Storage) inicialicen su `t_log` y su `t_config` de la misma forma, sin repetir código y sin el bug clásico de leer una clave que no existe en el `.config`.

**Importante:** `so_log` y `so_config` son dos cosas *totalmente independientes*. No hay un struct que las una. Usás la que necesites, cuando la necesites, como ya lo veníamos haciendo cada uno en su módulo — esto solo saca el código repetido a un lugar común.

## Instalación / include

```c
#include "so_log.h"
#include "so_config.h"
```

(el Makefile de cada módulo tiene que linkear contra `utils/` — si no está armado avisen y lo vemos)

---

## 1. `so_log` — Logger

### Crear el logger

```c
t_log* iniciar_logger(char* path_log, char* nombre_modulo, t_log_level nivel_inicial);
```

Hace exactamente lo mismo que `log_create(...)`, con `true` fijo para que loguee **a archivo y por consola** (eso es obligatorio del enunciado: sin archivo el TP no es evaluable). Si no puede crear el archivo, corta la ejecución con un mensaje claro — no hace falta que cada módulo chequee `NULL` a mano.

```c
t_log* placa_logger = iniciar_logger("placa.log", "LOGGER_PLACA", LOG_LEVEL);
```

### Aplicar el nivel que viene del config

```c
void so_log_set_level(t_log* logger, char* log_level_str);
```

Wrapea `log_level_from_string` + `log_set_level`. Se llama **después** de leer `LOG_LEVEL` del config, para que cambiar esa clave en el `.config` y volver a correr el módulo cambie el nivel de log sin recompilar.

```c
so_log_set_level(placa_logger, LOG_LEVEL); // LOG_LEVEL ya leído del config
```

Para loguear, se sigue usando `log_info`, `log_debug`, `log_error`, etc. de siempre (`commons/log.h`), pasándole tu `t_log*`. `so_log` no reemplaza esas funciones, solo te ahorra la creación.

---

## 2. `so_config` — Config

### Crear el config

```c
t_config* iniciar_config(char* path_config);
```

Igual que `config_create(...)`, pero si el archivo no existe o no se puede parsear, corta con mensaje claro en vez de devolver `NULL` silenciosamente.

```c
t_config* placa_config = iniciar_config(archivo_config);
```

### Leer valores (¡esto es lo que cambia tu flujo de siempre!)

En vez de `config_get_string_value` / `config_get_int_value` directo, usá:

```c
char*  so_config_get_string(t_config* config, char* nombre_modulo, char* clave);
int    so_config_get_int(t_config* config, char* nombre_modulo, char* clave);
double so_config_get_double(t_config* config, char* nombre_modulo, char* clave);
```

Cada uno valida con `config_has_property` **antes** de leer. Si la clave falta, imprime algo como:

```
[PLACA] ERROR - Falta la clave obligatoria en el config: TAM_MEMORIA
```

y corta la ejecución ahí mismo (`exit(EXIT_FAILURE)`, código de salida ≠ 0). Así el error aparece apenas arrancás el módulo, no diez pantallas de log después con un `TAM_MEMORIA` en 0 que no tiene sentido.

El parámetro `nombre_modulo` es solo texto para el mensaje de error (ej: `"PLACA"`, `"PLANIFICADOR"`) — no cambia nada más.

### Ejemplo completo: adaptar tu `inicializar_config()`

Tu versión original:

```c
void inicializar_config(){
    placa_config = config_create(archivo_config);
    TAM_MEMORIA = config_get_int_value(placa_config,"TAM_MEMORIA");
    // ...
}
```

Con `so_config`, cambia solo la función que llamás en cada línea — la estructura es idéntica:

```c
void inicializar_config(){
    placa_config = iniciar_config(archivo_config);

    PUERTO_ESCUCHA      = so_config_get_string(placa_config, "PLACA", "PUERTO_ESCUCHA");
    LOG_LEVEL           = so_config_get_string(placa_config, "PLACA", "LOG_LEVEL");
    TAM_MEMORIA         = so_config_get_int(placa_config, "PLACA", "TAM_MEMORIA");
    TAM_PAGINA          = so_config_get_int(placa_config, "PLACA", "TAM_PAGINA");
    RETARDO_MEMORIA     = so_config_get_int(placa_config, "PLACA", "RETARDO_MEMORIA");
    ALGORITMO_REEMPLAZO = so_config_get_string(placa_config, "PLACA", "ALGORITMO_REEMPLAZO");
    PATH_INSTRUCCIONES  = so_config_get_string(placa_config, "PLACA", "PATH_INSTRUCCIONES");
    PATH_OFFLOAD        = so_config_get_string(placa_config, "PLACA", "PATH_OFFLOAD");
    TAM_OFFLOAD         = so_config_get_int(placa_config, "PLACA", "TAM_OFFLOAD");
    RETARDO_OFFLOAD     = so_config_get_int(placa_config, "PLACA", "RETARDO_OFFLOAD");

    so_log_set_level(placa_logger, LOG_LEVEL);
}
```

Cada módulo sigue teniendo sus propias variables globales y su propia lista de claves — eso no se generaliza porque cada módulo tiene un config distinto. Lo único que se comparte es *cómo* se crea el log/config y *cómo* se valida cada clave.

---

## 3. Checklist rápido para migrar tu módulo

1. `#include "so_log.h"` y/o `#include "so_config.h"` (los que necesites, no hace falta usar los dos).
2. Cambiar `log_create(...)` por `iniciar_logger(...)`.
3. Cambiar `config_create(...)` por `iniciar_config(...)`.
4. Cambiar cada `config_get_string_value` / `config_get_int_value` por `so_config_get_string` / `so_config_get_int` (agregando el nombre del módulo como segundo parámetro).
5. Si usás `LOG_LEVEL` del config, llamar `so_log_set_level(tu_logger, LOG_LEVEL)` después de leerlo.
6. Probar sacando una clave del `.config` a propósito → tiene que cortar con mensaje claro, no explotar más adelante con un valor en 0/basura.

## Dudas / pendientes

- Falta decidir el wrapper thread-safe para logging concurrente (Planificador/Placa/Storage con server multihilo).
- Falta `Makefile` de `utils/` como librería estática para que los 4 módulos la linkeen — coordinarlo con INF-03 (SIS-8).

Cualquier duda, tirarla en el hilo de UTL-05 en Linear.