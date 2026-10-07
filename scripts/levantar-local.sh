#!/usr/bin/env bash

set -uo pipefail

# Levanta los 4 modulos en localhost: Placa y Storage -> Planificador -> 3 Cores.
# uso: ./scripts/levantar-local.sh [pseudocodigo del job inicial]

cd "$(dirname "$0")/.."

CONFIGS=configs/local
LOGS=runtime/logs
PIDS=runtime/local.pids
JOB_INICIAL="${1:-scripts/pseudocodigo/prueba_minima}"
ESPERA=1   # segundos entre etapas

declare -A PID

abortar() {
    echo "!!! $1" >&2
    exit 1
}

# Lanza un proceso en segundo plano con la salida a runtime/logs/<nombre>.out
lanzar() {
    local nombre="$1"; shift
    nohup "$@" > "$LOGS/$nombre.out" 2>&1 < /dev/null &
    PID[$nombre]=$!
    echo "$! $nombre" >> "$PIDS"
    echo "    $nombre (pid $!)"
}

# Espera y verifica que los procesos de la etapa sigan vivos; si alguno murio, baja todo
verificar() {
    sleep "$ESPERA"
    for nombre in "$@"; do
        if ! kill -0 "${PID[$nombre]}" 2>/dev/null; then
            echo >&2
            echo "!!! $nombre murio al arrancar. Ultimas lineas de $LOGS/$nombre.out:" >&2
            tail -n 15 "$LOGS/$nombre.out" >&2
            echo >&2
            ./scripts/bajar-local.sh
            exit 1
        fi
    done
}

puerto_escucha() {
    grep -E '^PUERTO_ESCUCHA' "$1" | cut -d= -f2 | tr -d ' '
}

# --- chequeos previos ---

for modulo in placa storage planificador core; do
    [[ -x "$modulo/bin/$modulo" ]] || abortar "falta $modulo/bin/$modulo. Corre ./scripts/build.sh primero."
done

[[ -f "$JOB_INICIAL" ]] || abortar "no existe el pseudocodigo $JOB_INICIAL"

if [[ -s "$PIDS" ]]; then
    while read -r pid nombre; do
        kill -0 "$pid" 2>/dev/null && abortar "ya hay modulos levantados ($nombre, pid $pid). Corre ./scripts/bajar-local.sh primero."
    done < "$PIDS"
    rm -f "$PIDS"   # quedo de una corrida anterior que ya murio
fi

for config in placa storage planificador; do
    puerto=$(puerto_escucha "$CONFIGS/$config.config")
    if ss -ltn "sport = :$puerto" | grep -q LISTEN; then
        abortar "el puerto $puerto ($config) ya esta ocupado"
    fi
done

mkdir -p "$LOGS"

# --- arranque ---

echo ">>> Placa y Storage"
lanzar placa   ./placa/bin/placa     "$CONFIGS/placa.config"
lanzar storage ./storage/bin/storage "$CONFIGS/storage.config"
verificar placa storage

echo ">>> Planificador"
lanzar planificador ./planificador/bin/planificador "$CONFIGS/planificador.config" "$JOB_INICIAL"
verificar planificador

echo ">>> Cores"
for id in 1 2 3; do
    lanzar "core$id" ./core/bin/core "$CONFIGS/core$id.config" "$id"
done
verificar core1 core2 core3

echo
echo ">>> OK: 4 modulos levantados (job inicial: $JOB_INICIAL)"
echo "    salida de cada proceso: $LOGS/<nombre>.out"
echo "    ej: tail -f $LOGS/planificador.out"
echo "    para bajarlos: ./scripts/bajar-local.sh"
