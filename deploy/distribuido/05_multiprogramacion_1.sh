#!/usr/bin/env bash
# Prueba 05_multiprogramacion/grado_1 (checkpoint 2) repartida en varias máquinas: RR · 1 core · multiprogramación 1 · Job inicial launchers/rr.asm
# Se corre en cada máquina con los módulos que le tocan, en orden (servidores primero):
#   ./deploy/distribuido/05_multiprogramacion_1.sh --modulos storage,placa
#   ./deploy/distribuido/05_multiprogramacion_1.sh --modulos planificador --ip-placa <IP> --ip-storage <IP>
#   ./deploy/distribuido/05_multiprogramacion_1.sh --modulos cores --ip-placa <IP> --ip-planificador <IP>
source "$(dirname "$(readlink -f "${BASH_SOURCE[0]}")")/../lib/common.sh"
ESC_FIJO="05_multiprogramacion/grado_1"
correr_distribuido "$ESC_FIJO" "$@"
