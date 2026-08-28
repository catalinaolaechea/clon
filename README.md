# EntrenadOS — TP Sistemas Operativos 2C2026

Grupo **los cabuleros de operativos** — UTN FRBA.

Sistema distribuido en C que simula un cluster de entrenamiento de modelos de IA:
cuatro procesos que se comunican por sockets TCP.

| Módulo | Responsabilidad | A cargo de |
|---|---|---|
| `planificador` | Cola de Jobs, syscalls y servicios de I/O | Benitez Mingrone, Rodriguez |
| `core` | Ciclo de instrucción y MMU | Olaechea |
| `placa` | Memoria de usuario, paginación y Offload | Pacheco |
| `storage` | Filesystem FAT32_TRAIN y journaling | Torrado Figueroa |
| `utils` | Biblioteca compartida: sockets, serialización, protocolo | Dueños: los del Planificador |

## Orden de levantado

El orden lo fija el enunciado: cada módulo asume que sus servidores ya están arriba.

```
1) placa + storage     (servidores, independientes entre sí)
2) planificador        (cliente de placa y storage; servidor de cores)
3) core 1..N           (clientes de planificador y placa)
```

```bash
./bin/placa         [Archivo Config]
./bin/storage       [Archivo Config]
./bin/planificador  [Archivo Config] [Path Job Inicial]
./bin/core          [Archivo Config] [Identificador]
```

Los N Cores son N procesos del **mismo** binario, con configs e identificadores
distintos. En una sola máquina los tres servidores no pueden compartir puerto: la
convención del grupo es **planificador 8080, placa 8081, storage 8082**.

## Estructura

Además de un proyecto por módulo, el repo tiene:

```
configs/local/          # un .config por módulo, todo en 127.0.0.1
configs/distribuido/    # un .config por módulo con las IPs de las VMs
pseudocodigo/           # los programas que ejecutan los Jobs (PATH_INSTRUCCIONES)
scripts/                # levantar-local.sh, bajar-local.sh, build.sh
runtime/                # artefactos de ejecución (ignorado por git)
```

Los `.config` y los archivos de pseudocódigo **se versionan**: son los que se editan
durante la corrección. Todo lo que un módulo escribe en tiempo de ejecución —el
Offload, el volumen y el journal del Storage, los reportes del Logger— va bajo
`runtime/` y no se versiona.

La documentación técnica (`protocolo.md`, los diseños y la evidencia de cada check)
va en `docs/`, que se crea junto con el documento de protocolo.

## Dependencias

Para poder compilar y ejecutar el proyecto, es necesario tener instalada la
biblioteca [so-commons-library] de la cátedra:

```bash
git clone https://github.com/sisoputnfrba/so-commons-library
cd so-commons-library
make debug
make install
```

## Compilación y ejecución

Cada módulo del proyecto se compila de forma independiente a través de un
archivo `makefile`. Para compilar un módulo, es necesario ejecutar el comando
`make` desde la carpeta correspondiente.

El ejecutable resultante de la compilación se guardará en la carpeta `bin` del
módulo. Ejemplo:

```sh
cd core
make
./bin/core
```

## Importar desde Visual Studio Code

Para importar el workspace, debemos abrir el archivo `tp.code-workspace` desde
la interfaz o ejecutando el siguiente comando desde la carpeta raíz del
repositorio:

```bash
code tp.code-workspace
```

## Checkpoint

Para cada checkpoint de control obligatorio, se debe crear un tag en el
repositorio con el siguiente formato:

```
checkpoint-{número}
```

Donde `{número}` es el número del checkpoint, ejemplo: `checkpoint-1`.

Para crear un tag y subirlo al repositorio, podemos utilizar los siguientes
comandos:

```bash
git tag -a checkpoint-{número} -m "Checkpoint {número}"
git push origin checkpoint-{número}
```

> [!WARNING]
> Asegúrense de que el código compila y cumple con los requisitos del checkpoint
> antes de subir el tag.

## Entrega

Para desplegar el proyecto en una máquina Ubuntu Server, podemos utilizar el
script [so-deploy] de la cátedra:

```bash
git clone https://github.com/sisoputnfrba/so-deploy.git
cd so-deploy
./deploy.sh -r=release -p=utils -p=planificador -p=core -p=placa -p=storage "tp-{año}-{cuatri}-{grupo}"
```

El mismo se encargará de instalar las Commons, clonar el repositorio del grupo
y compilar el proyecto en la máquina remota.

> [!NOTE]
> Ante cualquier duda, pueden consultar la documentación en el repositorio de
> [so-deploy], o utilizar el comando `./deploy.sh --help`.

## Guías útiles

- [Cómo interpretar errores de compilación](https://docs.utnso.com.ar/primeros-pasos/primer-proyecto-c#errores-de-compilacion)
- [Cómo utilizar el debugger](https://docs.utnso.com.ar/guias/herramientas/debugger)
- [Cómo configuramos Visual Studio Code](https://docs.utnso.com.ar/guias/herramientas/code)
- **[Guía de despliegue de TP](https://docs.utnso.com.ar/guías/herramientas/deploy)**

[so-commons-library]: https://github.com/sisoputnfrba/so-commons-library
[so-deploy]: https://github.com/sisoputnfrba/so-deploy
