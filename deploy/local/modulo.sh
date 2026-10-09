#!/usr/bin/env bash
# Corre UN solo módulo en primer plano, en esta terminal, con la config de un
# escenario (la ruta original de escenarios/<escenario>/configs/). Sirve para probar
# un módulo aislado y usar su consola (INFO, TLS...), o para levantar los módulos a mano en terminales separadas, como en los README.
#
#   ./deploy/local/modulo.sh placa 02_fifo_un_core
#   ./deploy/local/modulo.sh core 04_rr_dos_cores --id 2
#   ./deploy/local/modulo.sh planificador 03_rr_un_core --rapido
source "$(dirname "$(readlink -f "${BASH_SOURCE[0]}")")/../lib/common.sh"

usage() {
    cat >&2 <<EOF
Uso: ./deploy/local/modulo.sh <storage|placa|planificador|core> <escenario> [opciones]

Corre un solo módulo en primer plano (Ctrl-C lo corta), con directorio de trabajo
checkpoints/<checkpoint>/ y las configs del escenario.

Opciones:
  --id <n>                 identificador del Core (default 1)
  --valgrind               corre el módulo dentro de valgrind (fugas de memoria);
                           el informe sale al terminar el módulo
EOF
    opciones_comunes
}

MODULO="${1:-}"
case "$MODULO" in
    storage|placa|planificador|core) shift;;
    -h|--help) usage; exit 0;;
    *) error "primer argumento: storage, placa, planificador o core"; usage; exit 1;;
esac

CORE_ID=1
VALGRIND=0
ARGS=()
while [[ $# -gt 0 ]]; do
    case "$1" in
        --id) CORE_ID="${2:?falta valor para --id}"; shift 2;;
        --valgrind) VALGRIND=1; shift;;
        *)    ARGS+=("$1"); shift;;
    esac
done
parse_args 0 "${ARGS[@]+"${ARGS[@]}"}"
resolver_escenario
preparar_configs
preparar_runtime

[[ -x "$(binario "$MODULO")" ]] || morir "falta $(binario "$MODULO"). Compilá con ./scripts/build.sh"

case "$MODULO" in
    storage)      puerto="$PUERTO_STORAGE";      args=("${CFG[storage]}");;
    placa)        puerto="$PUERTO_PLACA";        args=("${CFG[placa]}");;
    planificador) puerto="$PUERTO_PLANIFICADOR"; args=("${CFG[planificador]}" "$JOB_ARG");;
    core)
        [[ -n "${CFG[core-$CORE_ID]:-}" ]] || morir "$ESCENARIO no tiene core-$CORE_ID.config"
        puerto="";                               args=("${CFG[core-$CORE_ID]}" "$CORE_ID");;
esac
if [[ -n "$puerto" ]] && puerto_escuchando "$puerto"; then
    morir "el puerto $puerto ya está ocupado (¿quedó otro $MODULO corriendo?). Probá ./deploy/bajar.sh"
fi

info "$MODULO — escenario $ESCENARIO"
detalle "directorio de trabajo: $CP_DIR"
detalle "comando: $(binario "$MODULO") ${args[*]}"
detalle "log: $CP_DIR/$([[ "$MODULO" == core ]] && echo "core$CORE_ID" || echo "$MODULO").log"
echo
cd "$CP_DIR"
if [[ "$VALGRIND" == 1 ]]; then
    command -v valgrind >/dev/null || morir "falta valgrind: sudo apt-get install -y valgrind"
    detalle "con valgrind: el informe de fugas sale cuando el módulo TERMINA"
    echo
    exec valgrind --leak-check=full --show-leak-kinds=definite,indirect --track-origins=yes \
        "$(binario "$MODULO")" "${args[@]}"
fi
exec "$(binario "$MODULO")" "${args[@]}"
