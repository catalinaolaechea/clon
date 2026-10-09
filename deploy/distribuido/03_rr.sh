#!/usr/bin/env bash
# Prueba 03_rr_un_core (checkpoint 2) repartida en varias máquinas: RR (quantum 2000 ms) · 1 core · multiprogramación 5 · Job inicial launchers/rr.asm
# Se corre en cada máquina con los módulos que le tocan, en orden (servidores primero):
#   ./deploy/distribuido/03_rr.sh --modulos storage,placa
#   ./deploy/distribuido/03_rr.sh --modulos planificador --ip-placa <IP> --ip-storage <IP>
#   ./deploy/distribuido/03_rr.sh --modulos cores --ip-placa <IP> --ip-planificador <IP>
source "$(dirname "$(readlink -f "${BASH_SOURCE[0]}")")/../lib/common.sh"
ESC_FIJO="03_rr_un_core"
correr_distribuido "$ESC_FIJO" "$@"
