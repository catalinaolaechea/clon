#!/usr/bin/env bash
# Test automático de la Placa por consola (SIS-40 / SIS-43), sin Planificador ni Core.
# Levanta la Placa con la config de un escenario, le escribe comandos de debug en la
# consola (CREAR / FETCH / FINALIZAR) y compara lo que responde con lo esperado.
#
#   ./deploy/local/test_placa.sh            # con el escenario 02_fifo_un_core
#   ./deploy/local/test_placa.sh 3          # con otro escenario (solo cambia la config)
# Termina con código 0 si pasa todo y 1 si algo falla (sirve para make).
source "$(dirname "$(readlink -f "${BASH_SOURCE[0]}")")/../lib/common.sh"

usage() { echo "Uso: $(basename "$0") [escenario] [--pruebas <dir>]" >&2; }

SESION="test-placa-$$"
parse_args 0 "$@"
ESCENARIO="${ESCENARIO:-02_fifo_un_core}"
MODULOS="placa"
resolver_escenario
preparar_configs
chequear_tmux
[[ -x "$(binario placa)" ]] || morir "falta $(binario placa). Compilá con: make placa"

# El test usa su propia copia de la config con un puerto libre: así corre aunque tengas
# otra placa levantada (por ejemplo con modulo.sh o probar.sh). El resto de la config es
# la del escenario, sin cambios. La copia y el log del test quedan en runtime/_deploy/_test/.
TEST_DIR="$CP_DIR/runtime/_deploy/_test"
mkdir -p "$TEST_DIR"
PUERTO_TEST=18081
while puerto_escuchando "$PUERTO_TEST"; do PUERTO_TEST=$((PUERTO_TEST + 1)); done
cp "${CFG[placa]}" "$TEST_DIR/placa.config"
cfg_set "$TEST_DIR/placa.config" PUERTO_ESCUCHA "$PUERTO_TEST"
CFG[placa]="$TEST_DIR/placa.config"
PUERTO_PLACA="$PUERTO_TEST"

# Datos esperados, sacados de los propios archivos de la cátedra (no de memoria)
JOB_A="individuales/05_loop.asm"
JOB_B="launchers/fifo.asm"
lineas() { grep -v '^[[:space:]]*$' "$CP_DIR/scripts/$1" | tr -d '\r'; }
mapfile -t A < <(lineas "$JOB_A")
mapfile -t B < <(lineas "$JOB_B")
N_A=${#A[@]}

# placa.log se escribe en el directorio de trabajo (compartido con otra placa que esté
# corriendo): no se borra, se mira solo lo que se agregue desde ahora.
LOG="$CP_DIR/placa.log"
LOG_DESDE=$(( $( [[ -f "$LOG" ]] && wc -l < "$LOG" || echo 0 ) + 1 ))
terminar() { tmux kill-session -t "$SESION" 2>/dev/null || true
             pkill -KILL -f "^$(binario placa) $TEST_DIR/placa.config" 2>/dev/null || true; }
trap terminar EXIT

info "Test de la Placa — config de $ESCENARIO (puerto de prueba $PUERTO_TEST)"
tmux new-session -d -s "$SESION" -x 200 -y 300 -c "$CP_DIR" "$(printf '%q ' "$(binario placa)" "${CFG[placa]}"); exec bash"
for ((i = 0; i < 40; i++)); do puerto_escuchando "$PUERTO_PLACA" && break; sleep 0.25; done
puerto_escuchando "$PUERTO_PLACA" || morir "la placa no arrancó. Probala a mano: ./deploy/local/modulo.sh placa $ESCENARIO"

# Manda un comando a la consola y devuelve SOLO lo que se imprimió después de él.
# Se cuentan solo las líneas con texto: la pantalla de tmux tiene líneas vacías al final.
# La última línea antes del comando es el prompt «Placa> », que pasa a ser «Placa> <comando>».
pantalla() { tmux capture-pane -p -S - -t "$SESION" | grep -v '^[[:space:]]*$' || true; }
consola() {
    local antes; antes="$(pantalla | wc -l)"
    tmux send-keys -t "$SESION" "$1" Enter
    sleep "${2:-0.6}"
    pantalla | tail -n +"$((antes))"
}

OK=0; FALLAS=0
# probar <descripción> <comando> <texto esperado en la respuesta>
probar() {
    local salida; salida="$(consola "$2")"
    if grep -qF -- "$3" <<< "$salida"; then
        printf '  \e[32m✔\e[0m %s\n' "$1"; OK=$((OK + 1))
    else
        printf '  \e[31m✘\e[0m %s\n      comando:  %s\n      esperaba: %s\n      salió:    %s\n' \
               "$1" "$2" "$3" "$(grep -v "^Placa> $2\$" <<< "$salida" | tr '\n' ' ' | cut -c1-150)"
        FALLAS=$((FALLAS + 1))
    fi
}

echo "SIS-40 — memoria de instrucciones y Fetch"
probar "crea un Job y lee todas sus líneas"       "CREAR 1 $JOB_A"           "Job 1 creado con $N_A instrucciones"
probar "PC 0 es la primera línea"                 "FETCH 1 0"                "PC 0: ${A[0]}"
probar "último PC válido"                         "FETCH 1 $((N_A - 1))"     "PC $((N_A - 1)): ${A[N_A - 1]}"
probar "PC fuera de rango da error, no basura"    "FETCH 1 $N_A"             "fuera de rango"
probar "otro Job con otro archivo"                "CREAR 2 $JOB_B"           "Job 2 creado con ${#B[@]} instrucciones"
probar "los Jobs no se pisan (Job 2)"             "FETCH 2 0"                "PC 0: ${B[0]}"
probar "los Jobs no se pisan (Job 1)"             "FETCH 1 0"                "PC 0: ${A[0]}"
probar "JID inexistente da error"                 "FETCH 77 0"               "JID 77 inexistente"

echo "SIS-43 — creación y finalización"
probar "archivo inexistente no crea el Job"       "CREAR 3 no_existe.asm"    "No se pudo crear el Job 3"
probar "finalizar un Job"                         "FINALIZAR 1"              "Job Finalizado"
probar "Fetch después de finalizar da error"      "FETCH 1 0"                "JID 1 inexistente"
probar "finalizar dos veces no rompe la placa"    "FINALIZAR 1"              "inexistente"
probar "la placa sigue viva (Job 2 intacto)"      "FETCH 2 0"                "PC 0: ${B[0]}"

echo "Logs obligatorios (placa.log)"
if tail -n +"$LOG_DESDE" "$LOG" 2>/dev/null | grep -q "## JID: 2 - Job Creado"; then
    printf '  \e[32m✔\e[0m «## JID: <JID> - Job Creado» al crear desde la consola\n'; OK=$((OK + 1))
else
    printf '  \e[33m!\e[0m «## JID: <JID> - Job Creado» no sale al crear desde la consola\n'
    printf '      (el log está en el case CREAR_JOB; si lo pasás a inicializar_job sale en los dos casos)\n'
fi

echo
if [[ $FALLAS -eq 0 ]]; then
    printf '\e[32m==> %d pruebas OK\e[0m\n' "$OK"
else
    printf '\e[31m==> %d OK, %d FALLARON\e[0m\n' "$OK" "$FALLAS"
fi
detalle "log de la placa: $CP_DIR/placa.log"
[[ $FALLAS -eq 0 ]]
