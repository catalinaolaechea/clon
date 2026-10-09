#!/usr/bin/env bash
# Baja todos los módulos de EntrenadOS de esta máquina y guarda los logs del escenario
# como evidencia (los README de la cátedra piden conservarlos antes de repetir).
#
#   ./deploy/bajar.sh                  # baja y mueve los .log a runtime/_deploy/<escenario>/evidencias/<fecha>/
#   ./deploy/bajar.sh --sin-evidencias # baja y deja los .log donde están
# No usa `set -e`: tiene que seguir aunque algún módulo ya no esté corriendo.
set -uo pipefail

DEPLOY_DIR="$(cd "$(dirname "$(readlink -f "${BASH_SOURCE[0]}")")" && pwd)"
REPO_ROOT="$(dirname "$DEPLOY_DIR")"
TMUX_SESSION="${TMUX_SESSION:-entrenados}"
GUARDAR=1
[[ "${1:-}" == "--sin-evidencias" ]] && GUARDAR=0

echo "==> Bajando los módulos de EntrenadOS..."
# Primero los clientes y al final los servidores, como al revés del arranque.
for m in core planificador placa storage; do
    if pkill -TERM -f "^$REPO_ROOT/$m/bin/$m( |\$)"; then
        echo "    $m: señal enviada"
    fi
done
sleep 1
for m in core planificador placa storage; do
    pkill -KILL -f "^$REPO_ROOT/$m/bin/$m( |\$)" 2>/dev/null && echo "    $m: forzado (no terminó con SIGTERM)"
done

if tmux kill-session -t "$TMUX_SESSION" 2>/dev/null; then
    echo "    sesión tmux '$TMUX_SESSION' cerrada"
fi

# Evidencias del último escenario levantado con deploy/
ESTADO="$DEPLOY_DIR/.ultimo_deploy"
if [[ -f "$ESTADO" ]]; then
    # shellcheck disable=SC1090
    source "$ESTADO"
    shopt -s nullglob
    # placa, planificador y cores escriben su .log en el directorio de trabajo;
    # el storage lo escribe en runtime/storage.log
    logs=("$CP_DIR"/*.log "$CP_DIR"/runtime/*.log)
    if [[ "$GUARDAR" == 1 && ${#logs[@]} -gt 0 ]]; then
        dest="$CP_DIR/runtime/_deploy/$ESC_ID/evidencias/$(date +%Y%m%d-%H%M%S)"
        mkdir -p "$dest"
        mv "${logs[@]}" "$dest/"
        # copia de las configs con las que corrió cada módulo, para saber qué se probó
        mkdir -p "$dest/configs-usadas"
        for c in "${CFGS_USADAS[@]}"; do cp "$c" "$dest/configs-usadas/" 2>/dev/null; done
        echo "==> Evidencias de $ESC_ID guardadas en:"
        echo "    $dest"
        for f in "$dest"/*.log; do echo "      $(basename "$f")  ($(wc -l < "$f") líneas)"; done
    elif [[ ${#logs[@]} -gt 0 ]]; then
        echo "==> Los logs quedaron en $CP_DIR (se van a seguir acumulando en la próxima corrida)"
    fi
    rm -f "$ESTADO"
fi
echo "==> Listo."
