#!/usr/bin/env bash
# Prueba 02_fifo_un_core (checkpoint 2) repartida en varias máquinas: FIFO · 1 core · multiprogramación 5 · Job inicial launchers/fifo.asm
# Se corre en cada máquina con los módulos que le tocan, en orden (servidores primero):
#   ./deploy/distribuido/02_fifo.sh --modulos storage,placa
#   ./deploy/distribuido/02_fifo.sh --modulos planificador --ip-placa <IP> --ip-storage <IP>
#   ./deploy/distribuido/02_fifo.sh --modulos cores --ip-placa <IP> --ip-planificador <IP>
source "$(dirname "$(readlink -f "${BASH_SOURCE[0]}")")/../lib/common.sh"
ESC_FIJO="02_fifo_un_core"
correr_distribuido "$ESC_FIJO" "$@"
