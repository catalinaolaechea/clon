# Deploy y pruebas — EntrenadOS

Herramientas para levantar los módulos del TP con las pruebas de la cátedra
([sisoputnfrba/entrenados-pruebas](https://github.com/sisoputnfrba/entrenados-pruebas)):
**todo en tu PC** o **repartido en varias PCs**, como en Plug & Pray.

## Índice

1. [Arranque rápido](#1-arranque-rápido)
2. [Antes de empezar](#2-antes-de-empezar)
3. [Los comandos (`make`)](#3-los-comandos-make)
4. [Ver los módulos: moverse en tmux](#4-ver-los-módulos-moverse-en-tmux)
5. [Las pruebas](#5-las-pruebas)
6. [Un módulo solo, con su consola](#6-un-módulo-solo-con-su-consola)
7. [Deploy en varias PCs](#7-deploy-en-varias-pcs)
8. [Qué config usa cada módulo](#8-qué-config-usa-cada-módulo)
9. [Logs y evidencias](#9-logs-y-evidencias)
10. [Problemas comunes](#10-problemas-comunes)
11. [Referencia: scripts y opciones](#11-referencia-scripts-y-opciones)

---

## 1. Arranque rápido

En cualquier terminal (la de VS Code o la de Ubuntu), **parado en la raíz del repo**:

```bash
cd ~/Desktop/tp-2026-2c-los-cabuleros-de-operativos
make compilar                # compila todo
make prueba fifo rapido      # levanta storage, placa, planificador y core con la prueba FIFO
make ver                     # mirar los módulos (clic en la barra de abajo para cambiar)
                             # salir sin cortar nada: Ctrl+b, soltar, d
make bajar                   # cortar todo y guardar los logs
```

O un solo módulo, con su consola:

```bash
make modulo placa fifo       # la placa con la config de la prueba FIFO
```

`make` sin nada muestra la ayuda con todos los comandos.

---

## 2. Antes de empezar

| Qué | Cómo |
|---|---|
| **Estar en la raíz del repo** | `cd ~/Desktop/tp-2026-2c-los-cabuleros-de-operativos` (en VS Code la terminal puede abrirse dentro de `placa/`) |
| **TP compilado** | `make compilar` |
| **tmux** | ya viene en la VM; si no: `sudo apt-get install -y tmux` |
| **Repo de pruebas** | se descarga solo la primera vez en `~/Desktop/entrenados-pruebas` (al lado del TP) |

El repo de pruebas no hay que tocarlo. Para traer lo último que publique la cátedra (por
ejemplo las pruebas del checkpoint 3), usá `--actualizar` con los scripts o
`git -C ~/Desktop/entrenados-pruebas pull`.

---

## 3. Los comandos (`make`)

Las palabras que van después del comando son sus argumentos.

### Una prueba completa en tu PC

```bash
make prueba fifo             # los 4 módulos con la config de la prueba FIFO
make prueba rr rapido        # con retardo 10 ms y quantum 20 ms (pasada rápida)
make prueba 1 5              # instrucciones aisladas con el archivo 05_loop.asm
```

Levanta todos los módulos **en segundo plano** (dentro de tmux) y te devuelve el prompt.
Para verlos: `make ver`. Para cortarlos: `make bajar`.

### Un módulo solo

```bash
make modulo placa            # la placa con la config de la prueba 2 (fifo)
make modulo placa rr         # la placa con la config de la prueba 3
make modulo storage fifo
make modulo planificador rr
make modulo core concurrente 2   # el core 2 de la prueba 4
make modulo placa fifo valgrind  # dentro de valgrind
```

El módulo corre **en tu terminal** (la ocupa). Ver [sección 6](#6-un-módulo-solo-con-su-consola).

### En varias PCs

```bash
make deploy 4 storage,placa                                        # PC A
make deploy 4 planificador PLACA=192.168.0.10 STORAGE=192.168.0.10 # PC B
make deploy 4 cores PLACA=192.168.0.10 PLANI=192.168.0.11          # PC C
```

Ver [sección 7](#7-deploy-en-varias-pcs).

### Otros

| Comando | Qué hace |
|---|---|
| `make ver` | entra a ver los módulos que están corriendo |
| `make bajar` | corta todos los módulos y guarda los logs |
| `make listar` | muestra las pruebas disponibles |
| `make compilar` | compila utils y los 4 módulos |
| `make compilar placa` | compila solo utils y la placa |

### Nombres de las pruebas

Se pueden escribir por **número** o por **nombre**, es lo mismo:

| Número | Nombre | Prueba |
|---|---|---|
| `1` | `instrucciones` | instrucciones aisladas |
| `2` | `fifo` | FIFO, 1 core |
| `3` | `rr` | Round Robin, 1 core |
| `4` | `concurrente` | Round Robin, 2 cores |
| `5.1` | `multi1` | multiprogramación grado 1 |
| `5.2` | `multi2` | multiprogramación grado 2 |

### Palabras extra

| Palabra | Dónde | Qué hace |
|---|---|---|
| `rapido` | `prueba`, `modulo`, `deploy` | retardo de memoria 10 ms y quantum 20 ms (con los valores de la cátedra, 500 ms, una prueba dura ~12 minutos) |
| `valgrind` | `modulo` | corre el módulo dentro de valgrind; el informe de fugas sale **cuando el módulo termina** |

---

## 4. Ver los módulos: moverse en tmux

`make prueba` levanta cada módulo en una "ventana" de **tmux**. `make ver` te mete adentro:
ves un módulo por vez, y en la **barra verde de abajo** están todos con su número:

```
[entrenados] 0:storage  1:placa  2:planificador  3:core1*
```

El `*` marca el que estás viendo.

### Con el mouse (lo más fácil)

- **Clic en un nombre de la barra verde** → cambia a ese módulo.
- **Rueda del mouse** → sube por el log para leer lo anterior (`q` vuelve abajo).

### Con el teclado

Siempre en **dos pasos**: apretás `Ctrl+b`, **soltás todo**, y recién ahí apretás la otra
tecla **sola**.

| Querés | Teclas |
|---|---|
| ir directo a un módulo | `Ctrl+b`, soltar, el número: `0` storage · `1` placa · `2` planificador · `3` core1 |
| módulo siguiente / anterior | `Ctrl+b`, soltar, `n` / `p` |
| lista de módulos (elegís con flechas y Enter) | `Ctrl+b`, soltar, `w` |
| subir por el log sin mouse | `Ctrl+b`, soltar, `[` · flechas o RePág · `q` para volver |
| **salir sin cortar nada** | `Ctrl+b`, soltar, `d` |

> Si ves aparecer `^B^N` en la pantalla, apretaste `Ctrl+b` y la letra **juntos**. No rompe
> nada: soltá `Ctrl` antes de la letra.

Al salir con `d` los módulos **siguen corriendo**. Volvés a entrar con `make ver` y los cortás
con `make bajar`. Si un módulo termina o crashea, su ventana queda abierta con el error a la
vista.

---

## 5. Las pruebas

Las del checkpoint 2 (los valores salen de los README de la cátedra):

| Prueba | Escenario | Algoritmo | Cores | Multiprog. | Job inicial |
|---|---|---|---:|---:|---|
| `1` instrucciones | 01_instrucciones | FIFO | 1 | 1 | `individuales/<n>_*.asm`, **de a uno** |
| `2` fifo | 02_fifo_un_core | FIFO | 1 | 5 | `launchers/fifo.asm` |
| `3` rr | 03_rr_un_core | RR | 1 | 5 | `launchers/rr.asm` |
| `4` concurrente | 04_rr_dos_cores | RR | 2 | 5 | `launchers/rr.asm` |
| `5.1` multi1 | 05_multiprogramacion/grado_1 | RR | 1 | 1 | `launchers/rr.asm` |
| `5.2` multi2 | 05_multiprogramacion/grado_2 | RR | 1 | 2 | `launchers/rr.asm` |

La prueba **1** se corre **un archivo por vez**, cortando todo entre cada uno:

```bash
./deploy/local/01_instrucciones.sh --listar-jobs   # los 9 archivos
make prueba 1 1        # 01_noop.asm
make bajar
make prueba 1 5        # 05_loop.asm
make bajar
```

Qué hay que mirar en cada prueba y los resultados esperados están en el repo de pruebas:
`checkpoints/checkpoint_2/escenarios/<prueba>/README.md` y `checkpoints/checkpoint_2/resultados/`.

---

## 6. Un módulo solo, con su consola

```bash
make modulo placa fifo
```

Arranca **solo** ese módulo, en tu terminal, con la config de esa prueba. Sirve para:

- probar la placa sin los demás módulos, usando su consola;
- levantar los módulos **a mano**, cada uno en su terminal (en orden: storage, placa,
  planificador, core), como en los README de la cátedra.

Comandos de la consola de la placa (`Placa>`):

| Comando | Qué hace |
|---|---|
| `INFO` | estado de la memoria (enunciado) |
| `TLS` | Jobs registrados (enunciado) |
| `CREAR <jid> <archivo>` | crea un Job como si lo pidiera el planificador (debug). Ej: `CREAR 1 individuales/05_loop.asm` |
| `FETCH <jid> <pc>` | pide una instrucción como si fuera el core (debug). Ej: `FETCH 1 0` |
| `FINALIZAR <jid>` | finaliza un Job (debug) |

**Para salir:** la placa **no** se corta con `Ctrl+C`. Cerrá esa terminal (en VS Code, el
ícono de la papelera) o corré `make bajar` desde **otra** terminal. Los otros módulos sí se
cortan con `Ctrl+C`.

Para un test automático de la placa por consola (Fetch, crear, finalizar...):
`./deploy/local/test_placa.sh`.

---

## 7. Deploy en varias PCs

Como en Plug & Pray: **todas las PCs corren el mismo comando de la prueba**, y cada una dice
qué módulos levanta y dónde están los demás.

### Qué necesita cada PC

- El repo del TP clonado y compilado (`make compilar`).
- Estar en la misma red que las otras.
- El repo de pruebas se descarga solo.

### Paso a paso (ejemplo: prueba 4 con 3 PCs)

**1. Averiguá la IP de las PCs** que van a tener servidores: `hostname -I` (la primera).
También la muestra el script al terminar.

**2. Levantá en orden** — primero los servidores, después sus clientes:

```bash
# PC A (192.168.0.10): placa y storage — son servidores, no se conectan a nadie
make deploy 4 storage,placa

# PC B (192.168.0.11): planificador — se conecta a la placa y al storage de A
make deploy 4 planificador PLACA=192.168.0.10 STORAGE=192.168.0.10

# PC C: los cores — se conectan al planificador de B y a la placa de A
make deploy 4 cores PLACA=192.168.0.10 PLANI=192.168.0.11
```

**3. Para ver y cortar:** `make ver` y `make bajar` en **cada** PC.

### Qué módulos se pueden elegir

`storage`, `placa`, `planificador`, `cores` (todos los de la prueba) o `core1`, `core2`...
para poner cada core en una PC distinta. Se combinan con coma: `storage,placa`.

### Qué IP pasar

| Si en esta PC corre… | y en otra PC está… | pasá |
|---|---|---|
| planificador | la placa | `PLACA=<ip>` |
| planificador | el storage | `STORAGE=<ip>` |
| un core | el planificador | `PLANI=<ip>` |
| un core | la placa | `PLACA=<ip>` |

Si te olvidás una, el script **corta antes de levantar nada** y te dice cuál falta.

---

## 8. Qué config usa cada módulo

Cada módulo arranca con la **config original** de la carpeta de la prueba en el repo de la
cátedra. **No se copia ni se modifica nada**, ni en el repo del TP ni en el de pruebas:

```
make prueba 1
   │
   ├─ storage       entrenados-pruebas/checkpoints/checkpoint_2/escenarios/01_instrucciones/configs/storage.config
   ├─ placa         entrenados-pruebas/checkpoints/checkpoint_2/escenarios/01_instrucciones/configs/placa.config
   ├─ planificador  entrenados-pruebas/checkpoints/checkpoint_2/escenarios/01_instrucciones/configs/planificador.config  individuales/01_noop.asm
   └─ core 1        entrenados-pruebas/checkpoints/checkpoint_2/escenarios/01_instrucciones/configs/core-1.config  1

   directorio de trabajo: entrenados-pruebas/checkpoints/checkpoint_2/
     → PATH_INSTRUCCIONES=./scripts/ toma los Jobs de checkpoints/checkpoint_2/scripts/
```

- Del **repo del TP** solo se usan los **binarios** (`<modulo>/bin/<modulo>`). Sus configs
  (`placa/placa.config`, `configs/local/`) no se leen.
- Los **Jobs** salen de `checkpoints/checkpoint_2/scripts/` del repo de pruebas.
- **Única excepción:** si hay que cambiar un valor, *ese* archivo se usa desde una copia en
  `runtime/_deploy/<prueba>/configs/` (ignorada por git). Pasa solo:
  - en **varias PCs**: el planificador y los cores necesitan la IP real de las otras PCs, y
    las configs de la cátedra traen `127.0.0.1`;
  - con **`rapido`** (o `--log-level`), si lo pedís.

  El script siempre muestra qué módulos usan el original y cuáles una copia.

---

## 9. Logs y evidencias

**Mientras corre** la prueba, los logs están en `~/Desktop/entrenados-pruebas/checkpoints/checkpoint_2/`:
`placa.log`, `planificador.log`, `core1.log`… (el del storage en `runtime/storage.log`).
Los podés abrir en VS Code como cualquier archivo.

**Al hacer `make bajar`** se mueven a
`checkpoints/checkpoint_2/runtime/_deploy/<prueba>/evidencias/<fecha>/`, junto con las configs
que se usaron. Así cada corrida empieza con logs limpios y no se pierde ninguna (los README
de la cátedra piden conservar las evidencias antes de repetir). `./deploy/bajar.sh --sin-evidencias`
corta sin moverlos.

---

## 10. Problemas comunes

| Mensaje o síntoma | Qué hacer |
|---|---|
| `make: *** No rule to make target` | no estás en la raíz del repo → `cd ~/Desktop/tp-2026-2c-los-cabuleros-de-operativos` |
| `ya hay procesos escuchando en: placa:8081` | quedó algo corriendo (otra prueba, o una placa en otra terminal) → `make bajar` |
| `ya hay una sesión tmux 'entrenados'` | ya hay una prueba corriendo → `make bajar` |
| `falta .../bin/placa` | falta compilar → `make compilar` |
| `X no empezó a escuchar en :puerto` | el módulo falló al arrancar → `make ver` y mirá su ventana |
| `'rr' coincide con varios escenarios` | usá el número o el nombre corto de la tabla de la [sección 3](#nombres-de-las-pruebas) |
| aparece `^B^N` en tmux | `Ctrl+b` se suelta antes de la letra ([sección 4](#4-ver-los-módulos-moverse-en-tmux)) |
| la placa no se corta con `Ctrl+C` | cerrá la terminal o `make bajar` desde otra |
| `Connection refused` en varias PCs | el orden: primero la PC con placa/storage; revisá las IPs y que estén en la misma red |
| Job no encontrado | revisá el nombre con `make listar` / `--listar-jobs`; no uses `--job-absoluto` |

---

## 11. Referencia: scripts y opciones

Los comandos `make` llaman a estos scripts. Se pueden usar directo si necesitás una opción
que `make` no tiene.

```
deploy/
├── local/                       todo en esta PC
│   ├── 01_instrucciones.sh …    un script por prueba (make prueba)
│   ├── probar.sh                cualquier prueba por nombre o número
│   ├── modulo.sh                un solo módulo en primer plano (make modulo)
│   └── test_placa.sh            test automático de la placa por consola
├── distribuido/                 cada PC levanta algunos módulos (make deploy)
│   ├── 01_instrucciones.sh …
│   └── levantar.sh
├── bajar.sh                     make bajar
├── attach.sh                    make ver
└── lib/common.sh                lógica compartida (no se ejecuta directo)
```

| `make` | Script equivalente |
|---|---|
| `make prueba fifo` | `./deploy/local/probar.sh fifo` |
| `make prueba rr rapido` | `./deploy/local/probar.sh rr --rapido` |
| `make prueba 1 5` | `./deploy/local/01_instrucciones.sh 5` |
| `make modulo core concurrente 2` | `./deploy/local/modulo.sh core concurrente --id 2` |
| `make deploy 4 cores PLACA=a PLANI=b` | `./deploy/distribuido/levantar.sh 4 --modulos cores --ip-placa a --ip-planificador b` |
| `make ver` / `make bajar` | `./deploy/attach.sh` / `./deploy/bajar.sh` |

Opciones de los scripts:

| Opción | Qué hace |
|---|---|
| `--rapido` | `RETARDO_MEMORIA=10` y `RR_QUANTUM=20` (usa una copia de las configs) |
| `--job <archivo>` | otro Job inicial, relativo a `scripts/` (ej. `individuales/08_corto.asm`) |
| `--job-absoluto` | pasa el Job como ruta absoluta, como los README de la cátedra |
| `--log-level <nivel>` | pisa `LOG_LEVEL` (ej. `DEBUG`; usa una copia) |
| `--actualizar` | trae lo último del repo de pruebas |
| `--pruebas <dir>` | otra ubicación del repo de pruebas (o `export ENTRENADOS_PRUEBAS=<dir>`) |
| `--checkpoint <nombre>` | carpeta de `checkpoints/` (default `checkpoint_2`) |
| `--listar` | pruebas disponibles |
| `--id <n>` | solo `modulo.sh`: número de core |
| `--valgrind` | solo `modulo.sh` |
| `--modulos <lista>` | solo distribuido (obligatorio) |
| `--ip-placa / --ip-storage / --ip-planificador <ip>` | solo distribuido |

### Cómo arranca cada módulo, por dentro

- **Directorio de trabajo:** `checkpoints/checkpoint_2/` del repo de pruebas, como piden sus
  README: ahí se resuelven `./scripts/` y `./runtime/...`, y ahí quedan los `.log`.
- **Orden:** storage → placa → planificador → cores. Antes de cada cliente se espera a que su
  servidor esté escuchando (se mira la tabla de sockets con `ss`, sin conectarse, para no
  dejar un error de handshake en los logs).
- **Job inicial:** relativo a `PATH_INSTRUCCIONES` (`launchers/rr.asm`), que es lo que acepta
  la placa. La guía de la cátedra permite las dos formas.
- **tmux:** sesión `entrenados`, una ventana por módulo, mouse activado. Si un módulo termina,
  su ventana queda abierta para leer el error.
