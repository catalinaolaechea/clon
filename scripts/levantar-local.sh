#!/usr/bin/env bash

set -uo pipefail

# Levanta los 4 modulos en localhost: Placa y Storage -> Planificador -> 3 Cores.
# La salida de cada proceso va a runtime/logs/<nombre>.out

cd "$(dirname "$0")/.."

CONFIGS=configs/local
LOGS=runtime/logs
PIDS=runtime/local.pids   # lo usa bajar-local.sh
JOB_INICIAL=scripts/pseudocodigo/prueba_minima
ESPERA=1   # segundos entre etapas

lanzar() {
    local nombre="$1"; shift
    nohup "$@" > "$LOGS/$nombre.out" 2>&1 < /dev/null &
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
lanzar planificador ./planificador/bin/planificador "$CONFIGS/planificador.config" "$JOB_INICIAL"
sleep "$ESPERA"

echo ">>> Cores"
for id in 1 2 3; do
    lanzar "core$id" ./core/bin/core "$CONFIGS/core$id.config" "$id"
done

echo
echo ">>> OK. Salida de cada proceso en $LOGS/<nombre>.out"
echo "    para bajarlos: ./scripts/bajar-local.sh"
