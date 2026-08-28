#!/usr/bin/env bash

set -uo pipefail

# utils va primero: los 4 modulos enlazan contra lib/libutils.a
MODULOS=(utils planificador core placa storage)

PERFIL="${1:-debug}"
case "$PERFIL" in
    debug|release|clean) ;;
    *)
        echo "uso: $0 [debug|release|clean]" >&2
        exit 2
        ;;
esac

cd "$(dirname "$0")/.."

for modulo in "${MODULOS[@]}"; do
    echo ">>> $modulo ($PERFIL)"
    if ! make -C "$modulo" "$PERFIL"; then
        echo >&2
        echo "!!! FALLO: $modulo no compilo ($PERFIL). Se corta la build." >&2
        echo "!!! Si el que fallo es utils, los otros 4 no van a compilar tampoco." >&2
        exit 1
    fi
done

echo
echo ">>> OK: utils + 4 modulos ($PERFIL)"
