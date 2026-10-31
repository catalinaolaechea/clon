#!/usr/bin/env bash
# Levanta un escenario de las pruebas propias del Check 3 (pruebas/checkpoints/checkpoint_3/ de
# este repo) en ESTA máquina. Es probar.sh apuntado a esa carpeta en vez de al repo de la cátedra.
#
#   ./deploy/local/cp3.sh --listar
#   ./deploy/local/cp3.sh 01_mini_lru            # o por número: ./deploy/local/cp3.sh 1
#   ./deploy/local/cp3.sh 4 --log-level DEBUG
source "$(dirname "$(readlink -f "${BASH_SOURCE[0]}")")/../lib/common.sh"

PRUEBAS="$REPO_ROOT/pruebas"
CHECKPOINT="checkpoint_3"

# --actualizar hace git pull del repo de pruebas: acá ese repo es el del TP
[[ " $* " != *" --actualizar "* ]] || morir "--actualizar no aplica: las pruebas del Check 3 están en este repo"

correr_local "$@"
