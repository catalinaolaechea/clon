#!/usr/bin/env bash
# Levanta en ESTA máquina una parte de los módulos de cualquier escenario, pasándolo por
# nombre. Para las pruebas del checkpoint hay un script por prueba al lado
# (02_fifo.sh, 04_rr_concurrente.sh...): esto es lo mismo pero genérico.
#
#   ./deploy/distribuido/levantar.sh 04_rr_dos_cores --modulos storage,placa
source "$(dirname "$(readlink -f "${BASH_SOURCE[0]}")")/../lib/common.sh"
correr_distribuido "$@"
