#!/usr/bin/env bash
# Prueba 04_rr_dos_cores (checkpoint 2) repartida en varias máquinas: RR · 2 cores · multiprogramación 5 · Job inicial launchers/rr.asm
# Se corre en cada máquina con los módulos que le tocan, en orden (servidores primero):
#   ./deploy/distribuido/04_rr_concurrente.sh --modulos storage,placa
#   ./deploy/distribuido/04_rr_concurrente.sh --modulos planificador --ip-placa <IP> --ip-storage <IP>
#   ./deploy/distribuido/04_rr_concurrente.sh --modulos cores --ip-placa <IP> --ip-planificador <IP>
source "$(dirname "$(readlink -f "${BASH_SOURCE[0]}")")/../lib/common.sh"
ESC_FIJO="04_rr_dos_cores"
correr_distribuido "$ESC_FIJO" "$@"
