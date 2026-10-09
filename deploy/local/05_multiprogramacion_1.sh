#!/usr/bin/env bash
# Prueba 05_multiprogramacion/grado_1 (checkpoint 2): RR · 1 core · multiprogramación 1 · Job inicial launchers/rr.asm
# Levanta storage, placa, planificador y cores en esta máquina con las configs del escenario.
#   ./deploy/local/05_multiprogramacion_1.sh            # con los tiempos de la cátedra (retardo 500 ms)
#   ./deploy/local/05_multiprogramacion_1.sh --rapido   # retardo 10 ms y quantum 20 ms
source "$(dirname "$(readlink -f "${BASH_SOURCE[0]}")")/../lib/common.sh"
ESC_FIJO="05_multiprogramacion/grado_1"
correr_local "$ESC_FIJO" "$@"
