#!/usr/bin/env bash
# Levanta en ESTA máquina parte de los módulos de un escenario de las pruebas propias del Check 3
# (pruebas/checkpoints/checkpoint_3/ de este repo). Es levantar.sh apuntado a esa carpeta.
#
#   ./deploy/distribuido/cp3.sh 01_mini_lru --modulos storage,placa
#   ./deploy/distribuido/cp3.sh 01_mini_lru --modulos planificador --ip-placa <IP> --ip-storage <IP>
#   ./deploy/distribuido/cp3.sh 01_mini_lru --modulos cores --ip-placa <IP> --ip-planificador <IP>
source "$(dirname "$(readlink -f "${BASH_SOURCE[0]}")")/../lib/common.sh"

PRUEBAS="$REPO_ROOT/pruebas"
CHECKPOINT="checkpoint_3"

[[ " $* " != *" --actualizar "* ]] || morir "--actualizar no aplica: las pruebas del Check 3 están en este repo"

correr_distribuido "$@"
