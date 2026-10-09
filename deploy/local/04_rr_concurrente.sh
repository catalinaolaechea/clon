#!/usr/bin/env bash
# Prueba 04_rr_dos_cores (checkpoint 2): RR · 2 cores · multiprogramación 5 · Job inicial launchers/rr.asm
# Levanta storage, placa, planificador y cores en esta máquina con las configs del escenario.
#   ./deploy/local/04_rr_concurrente.sh            # con los tiempos de la cátedra (retardo 500 ms)
#   ./deploy/local/04_rr_concurrente.sh --rapido   # retardo 10 ms y quantum 20 ms
source "$(dirname "$(readlink -f "${BASH_SOURCE[0]}")")/../lib/common.sh"
ESC_FIJO="04_rr_dos_cores"
correr_local "$ESC_FIJO" "$@"
