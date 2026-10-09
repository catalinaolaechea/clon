#!/usr/bin/env bash
# Levanta cualquier escenario de la batería en ESTA máquina, pasándolo por nombre.
# Para las pruebas del checkpoint hay un script por prueba al lado (01_instrucciones.sh,
# 02_fifo.sh...): esto es lo mismo pero genérico, útil para escenarios nuevos.
#
#   ./deploy/local/probar.sh --listar
#   ./deploy/local/probar.sh 02_fifo_un_core --rapido
source "$(dirname "$(readlink -f "${BASH_SOURCE[0]}")")/../lib/common.sh"
correr_local "$@"
