#!/usr/bin/env bash

set -uo pipefail

# Levanta los 4 modulos en localhost: Placa y Storage -> Planificador -> 3 Cores.
# La salida de cada proceso va a runtime/logs/<nombre>.out
# Uso: [PLANIFICADOR_CONFIG=<config>] ./scripts/levantar-local.sh [job_inicial]   (nombre relativo a PATH_INSTRUCCIONES de la Placa)

cd "$(dirname "$0")/.."

CONFIGS=configs/local
LOGS=runtime/logs
PIDS=runtime/local.pids   # lo usa bajar-local.sh
JOB_INICIAL="${1:-prueba_minima}"   # solo el nombre: la Placa le antepone su PATH_INSTRUCCIONES
ESPERA=1   # segundos entre etapas
PLANIFICADOR_CONFIG="${PLANIFICADOR_CONFIG:-$CONFIGS/planificador.config}"
TECLADO_PLANIFICADOR=runtime/planificador.in   # FIFO: lo que se escriba aca le llega al Labeler

# Uso: [ENTRADA=<archivo>] lanzar <nombre> <comando...>   (sin ENTRADA el proceso lee de /dev/null)
lanzar() {
    local nombre="$1"; shift
    # <> abre para leer y escribir: con un FIFO no se bloquea esperando a alguien que escriba,
    # y el proceso nunca ve fin de entrada aunque cada echo cierre su lado
    nohup "$@" > "$LOGS/$nombre.out" 2>&1 <> "${ENTRADA:-/dev/null}" &
    echo "$! $nombre" >> "$PIDS"
    echo "    $nombre (pid $!)"
}

mkdir -p "$LOGS"
> "$PIDS"

echo ">>> Placa y Storage"
lanzar placa   ./placa/bin/placa     "$CONFIGS/placa.config"
lanzar storage ./storage/bin/storage "$CONFIGS/storage.config"
sleep "$ESPERA"

echo ">>> Planificador"
rm -f "$TECLADO_PLANIFICADOR"
mkfifo "$TECLADO_PLANIFICADOR"
ENTRADA="$TECLADO_PLANIFICADOR" lanzar planificador ./planificador/bin/planificador "$PLANIFICADOR_CONFIG" "$JOB_INICIAL"
sleep "$ESPERA"

echo ">>> Cores"
for id in 1 2 3; do
    lanzar "core$id" ./core/bin/core "$CONFIGS/core$id.config" "$id"
done

echo
echo ">>> OK. Salida de cada proceso en $LOGS/<nombre>.out"
echo "    teclado del Labeler: echo 'texto' > $TECLADO_PLANIFICADOR"
echo "    para bajarlos: ./scripts/bajar-local.sh"
