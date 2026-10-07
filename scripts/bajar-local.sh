#!/usr/bin/env bash

set -uo pipefail

# Baja los modulos que levanto levantar-local.sh, en orden inverso al de arranque.

cd "$(dirname "$0")/.."

PIDS=runtime/local.pids

if [[ ! -s "$PIDS" ]]; then
    echo "No hay modulos levantados ($PIDS vacio o inexistente)"
    exit 0
fi

tac "$PIDS" | while read -r pid nombre; do
    kill "$pid" 2>/dev/null && echo "    $nombre (pid $pid) bajado"
done
rm -f "$PIDS"
