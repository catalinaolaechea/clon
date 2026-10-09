#!/usr/bin/env bash
# Prueba 01_instrucciones (checkpoint 2): FIFO · 1 core · multiprogramación 1.
# Se ejecutan los 9 archivos de scripts/individuales/ DE A UNO, sin launcher, bajando
# todo entre cada uno (./deploy/bajar.sh). El número elige el archivo:
#   ./deploy/distribuido/01_instrucciones.sh 05 --modulos storage,placa        -> individuales/05_loop.asm
#   ./deploy/distribuido/01_instrucciones.sh --listar-jobs
# Sin número usa el default del README (01_noop.asm).
source "$(dirname "$(readlink -f "${BASH_SOURCE[0]}")")/../lib/common.sh"
ESC_FIJO="01_instrucciones"

# --pruebas se lee primero: hace falta para traducir el número a un archivo
prev=""
for a in "$@"; do [[ "$prev" == "--pruebas" ]] && PRUEBAS="$a"; prev="$a"; done

ARGS=()
while [[ $# -gt 0 ]]; do
    case "$1" in
        [0-9]|[0-9][0-9])
            num="$(printf '%02d' "$((10#$1))")"
            resolver_checkpoint
            archivo=("$CP_DIR"/scripts/individuales/"$num"_*.asm)
            [[ -f "${archivo[0]}" ]] || morir "no hay un archivo ${num}_*.asm en scripts/individuales/"
            ARGS+=(--job "individuales/$(basename "${archivo[0]}")"); shift;;
        --listar-jobs)
            resolver_checkpoint
            echo "Archivos de 01_instrucciones (se corren de a uno):"
            ls "$CP_DIR"/scripts/individuales/ | sed 's/^/  /'
            exit 0;;
        *) ARGS+=("$1"); shift;;
    esac
done
correr_distribuido "$ESC_FIJO" "${ARGS[@]+"${ARGS[@]}"}"
