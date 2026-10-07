#!/usr/bin/env bash

set -uo pipefail

# Baja los modulos que levanto levantar-local.sh: Cores -> Planificador -> Placa y Storage.
# uso: ./scripts/bajar-local.sh

cd "$(dirname "$0")/.."

PIDS=runtime/local.pids
GRACIA=3   # segundos que se le dan a cada proceso antes de forzarlo con SIGKILL

# Manda SIGTERM, espera hasta GRACIA segundos y si sigue vivo lo fuerza
bajar() {
    local pid="$1" nombre="$2"
    kill -0 "$pid" 2>/dev/null || return 0
    kill "$pid" 2>/dev/null
    for _ in $(seq $((GRACIA * 10))); do
        kill -0 "$pid" 2>/dev/null || { echo "    $nombre (pid $pid) bajado"; return 0; }
        sleep 0.1
    done
    kill -9 "$pid" 2>/dev/null
    echo "    $nombre (pid $pid) forzado con SIGKILL"
}

if [[ -s "$PIDS" ]]; then
    echo ">>> Bajando modulos de $PIDS"
    # tac: en orden inverso al de arranque
    tac "$PIDS" | while read -r pid nombre; do
        bajar "$pid" "$nombre"
    done
    rm -f "$PIDS"
else
    # sin archivo de PIDs: se buscan los procesos lanzados con los configs locales
    echo ">>> No hay $PIDS, buscando procesos con configs de configs/local/"
    pgrep -af "bin/(placa|storage|planificador|core) configs/local/" | tac | while read -r pid comando; do
        bajar "$pid" "$(basename "${comando%% *}")"
    done
fi

echo ">>> OK"
