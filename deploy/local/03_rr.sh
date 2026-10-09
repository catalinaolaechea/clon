#!/usr/bin/env bash
# Prueba 03_rr_un_core (checkpoint 2): RR (quantum 2000 ms) · 1 core · multiprogramación 5 · Job inicial launchers/rr.asm
# Levanta storage, placa, planificador y cores en esta máquina con las configs del escenario.
#   ./deploy/local/03_rr.sh            # con los tiempos de la cátedra (retardo 500 ms)
#   ./deploy/local/03_rr.sh --rapido   # retardo 10 ms y quantum 20 ms
source "$(dirname "$(readlink -f "${BASH_SOURCE[0]}")")/../lib/common.sh"
ESC_FIJO="03_rr_un_core"
correr_local "$ESC_FIJO" "$@"
