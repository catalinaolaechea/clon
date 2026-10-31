#!/usr/bin/env bash
# Arma los archivos que lee el Loader (LOAD_BATCH) en datos/. Son binarios: 32 uint32_t en
# little endian (el orden de los enteros en x86), igual que los lee el Core en E1/E2/E3.
#
#   pesos1: 32 veces el 2            -> 02 00 00 00  02 00 00 00 ...
#   lote1:  1, 2, 3, ..., 32         -> 01 00 00 00  02 00 00 00 ...
#
# Valores chicos a propósito, para poder seguir las cuentas a mano (resultados/*.md).
# Uso: ./generar_datos.sh   (los archivos ya están versionados: solo hace falta si se cambian)
set -euo pipefail
cd "$(dirname "$(readlink -f "${BASH_SOURCE[0]}")")"
mkdir -p datos

# un uint32_t como 4 bytes little endian. Devuelve el TEXTO "\x02\x00\x00\x00": recién el printf
# de abajo lo convierte en bytes (un byte 0 no sobrevive a pasar por $(...))
u32() {
    local v=$1
    printf '\\x%02x\\x%02x\\x%02x\\x%02x' $((v & 255)) $(((v >> 8) & 255)) $(((v >> 16) & 255)) $(((v >> 24) & 255))
}

: > datos/pesos1
: > datos/lote1
for i in $(seq 1 32); do
    printf "$(u32 2)"  >> datos/pesos1
    printf "$(u32 "$i")" >> datos/lote1
done

echo "datos/pesos1: $(stat -c %s datos/pesos1) bytes   datos/lote1: $(stat -c %s datos/lote1) bytes"
