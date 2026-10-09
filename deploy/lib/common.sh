#!/usr/bin/env bash
# =============================================================================
# common.sh - Librería compartida de los scripts de deploy de EntrenadOS.
#
# NO se ejecuta directo: la cargan con `source` los scripts de deploy/local y
# deploy/distribuido. Toma los escenarios del repo de pruebas de la cátedra
# (sisoputnfrba/entrenados-pruebas) y levanta los binarios del TP con esas configs.
#
# Cómo trabaja, igual que piden los README de la cátedra:
#   - El directorio de trabajo de TODOS los módulos es checkpoints/<checkpoint>/ del
#     repo de pruebas: ahí se resuelven las rutas relativas de las configs
#     (./scripts/, ./runtime/<escenario>/...) y ahí escriben sus .log los módulos.
#   - Cada módulo arranca con la ruta a la config ORIGINAL de la carpeta del escenario
#     (escenarios/<escenario>/configs/<modulo>.config). No se copia ni se modifica nada.
#     Solo si hace falta cambiar un valor (IP de otra máquina, --rapido, --log-level) se
#     usa una copia de ESE archivo en runtime/_deploy/ (ignorado por git).
#   - Orden de arranque: storage -> placa -> planificador -> cores, esperando a que
#     cada servidor esté escuchando antes de levantar a sus clientes.
# =============================================================================

set -euo pipefail

# Raíz del repo del TP = dos niveles arriba de deploy/lib/
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
DEPLOY_DIR="$REPO_ROOT/deploy"

# ---- Defaults (se pisan por parámetro o variable de entorno) -----------------
# Clon del repo de pruebas. Por defecto, al lado del repo del TP.
PRUEBAS="${ENTRENADOS_PRUEBAS:-$(dirname "$REPO_ROOT")/entrenados-pruebas}"
CHECKPOINT="${CHECKPOINT:-checkpoint_2}"
ESCENARIO=""
JOB=""                 # vacío = el «Job inicial por defecto» del README del escenario
JOB_ABSOLUTO=0         # 0: se pasa relativo a PATH_INSTRUCCIONES (launchers/rr.asm)
RAPIDO=0               # 1: RETARDO_MEMORIA=10 y RR_QUANTUM=20 (pasada rápida de la guía)
LOG_LEVEL_OVERRIDE=""
IP_PLACA="127.0.0.1"
IP_STORAGE="127.0.0.1"
IP_PLANIFICADOR="127.0.0.1"
MODULOS_TODOS="storage placa planificador cores"
MODULOS="$MODULOS_TODOS"
TIMEOUT_ESCUCHA=20     # segundos que se espera a que un servidor local escuche
TMUX_SESSION="${TMUX_SESSION:-entrenados}"

# ---- Mensajes ----------------------------------------------------------------
info()  { echo "==> $*"; }
detalle() { echo "    $*"; }
error() { echo "ERROR: $*" >&2; }
morir() { error "$*"; exit 1; }

# ---- Ayuda (cada script agrega su propio encabezado antes de llamar a esto) ---
opciones_comunes() {
    cat >&2 <<EOF
  --pruebas <dir>          clon de entrenados-pruebas
                           (default: $PRUEBAS)
  --checkpoint <nombre>    carpeta dentro de checkpoints/ (default: $CHECKPOINT)
  --job <archivo>          Job inicial relativo a scripts/ (ej. launchers/fifo.asm,
                           individuales/05_loop.asm). Default: el del README del escenario
  --job-absoluto           pasa el Job como ruta absoluta, como en los README de la
                           cátedra (requiere que la Placa acepte rutas absolutas)
  --rapido                 RETARDO_MEMORIA=10 y RR_QUANTUM=20 (pasada rápida de la guía;
                           usa una copia de las configs en vez de las originales)
  --log-level <nivel>      pisa LOG_LEVEL (ej. DEBUG; también usa una copia)
  --actualizar             trae lo último del repo de pruebas antes de arrancar
  --listar                 muestra los escenarios disponibles y sale
  -h | --help              esta ayuda
EOF
}

opciones_red() {
    cat >&2 <<EOF
  --ip-placa <ip>          IP de la máquina donde corre la Placa        (default 127.0.0.1)
  --ip-storage <ip>        IP de la máquina donde corre el Storage      (default 127.0.0.1)
  --ip-planificador <ip>   IP de la máquina donde corre el Planificador (default 127.0.0.1)
  --modulos <lista>        módulos que levanta ESTA máquina, separados por coma:
                           storage, placa, planificador, cores (todos los cores del
                           escenario) o core1, core2... (uno puntual). Default: todos
EOF
}

# parse_args <permite_red:0|1> "$@"
parse_args() {
    local permite_red="$1"; shift
    while [[ $# -gt 0 ]]; do
        case "$1" in
            --pruebas)       PRUEBAS="${2:?falta valor para --pruebas}"; shift 2;;
            --checkpoint)    CHECKPOINT="${2:?falta valor para --checkpoint}"; shift 2;;
            --job)           JOB="${2:?falta valor para --job}"; shift 2;;
            --job-absoluto)  JOB_ABSOLUTO=1; shift;;
            --rapido)        RAPIDO=1; shift;;
            --log-level)     LOG_LEVEL_OVERRIDE="${2:?falta valor para --log-level}"; shift 2;;
            --actualizar)    ACTUALIZAR=1; shift;;
            --listar)        LISTAR=1; shift;;
            -h|--help)       usage; exit 0;;
            --ip-placa|--ip-storage|--ip-planificador|--modulos)
                [[ "$permite_red" == 1 ]] || morir "$1 solo se usa en deploy/distribuido/levantar.sh"
                case "$1" in
                    --ip-placa)        IP_PLACA="${2:?falta valor para --ip-placa}";;
                    --ip-storage)      IP_STORAGE="${2:?falta valor para --ip-storage}";;
                    --ip-planificador) IP_PLANIFICADOR="${2:?falta valor para --ip-planificador}";;
                    --modulos)         MODULOS="${2:?falta valor para --modulos}"; MODULOS="${MODULOS//,/ }"
                                       [[ "$MODULOS" == "todos" ]] && MODULOS="$MODULOS_TODOS";;
                esac
                shift 2;;
            -*)  error "opción desconocida: $1"; usage; exit 1;;
            *)   [[ -z "$ESCENARIO" ]] || morir "sobra el argumento: $1"
                 ESCENARIO="${1%/}"; shift;;
        esac
    done
    if [[ "${LISTAR:-0}" == 1 ]]; then
        resolver_checkpoint; listar_escenarios; exit 0
    fi
}

# ---- Repo de pruebas y escenarios ---------------------------------------------
REPO_PRUEBAS_URL="https://github.com/sisoputnfrba/entrenados-pruebas"

# Si el repo de pruebas no está, se descarga solo. Con --actualizar se trae lo último
# (la cátedra publica las pruebas de cada checkpoint ahí).
resolver_checkpoint() {
    if [[ ! -d "$PRUEBAS" ]]; then
        command -v git >/dev/null || morir "falta git para descargar las pruebas"
        info "No está el repo de pruebas: descargándolo en $PRUEBAS"
        git clone -q "$REPO_PRUEBAS_URL" "$PRUEBAS" || morir "no se pudo clonar $REPO_PRUEBAS_URL
       Clonalo a mano o indicá otra ubicación con --pruebas <dir>."
    elif [[ "${ACTUALIZAR:-0}" == 1 ]]; then
        info "Actualizando el repo de pruebas ($PRUEBAS)"
        git -C "$PRUEBAS" pull -q --ff-only || morir "no se pudo actualizar $PRUEBAS (¿tiene cambios locales?)"
    fi
    PRUEBAS="$(cd "$PRUEBAS" && pwd)"
    CP_DIR="$PRUEBAS/checkpoints/$CHECKPOINT"
    [[ -d "$CP_DIR/escenarios" ]] || morir "no existe $CP_DIR/escenarios (¿checkpoint correcto?)"
}

# Un escenario es cualquier carpeta de escenarios/ que tenga configs/
# (05_multiprogramacion tiene dos: grado_1 y grado_2).
listar_escenarios() {
    echo "Escenarios de $CHECKPOINT ($CP_DIR/escenarios):"
    local d esc job cores
    while IFS= read -r d; do
        esc="${d#"$CP_DIR/escenarios/"}"; esc="${esc%/configs}"
        job="$(job_por_defecto "$CP_DIR/escenarios/$esc")"
        cores="$(ls "$d"/core-*.config 2>/dev/null | wc -l)"
        printf '  %-32s cores: %s   job: %s\n' "$esc" "$cores" "${job:-?}"
    done < <(find "$CP_DIR/escenarios" -type d -name configs | sort)
}

# Lee «| Job inicial por defecto | `launchers/rr.asm` |» del README del escenario.
job_por_defecto() {
    local readme="$1/README.md"
    [[ -f "$readme" ]] || return 0
    grep -m1 'Job inicial por defecto' "$readme" | sed -n 's/.*`\([^`]*\)`.*/\1/p'
}

# Todos los escenarios del checkpoint, uno por línea (ej. 05_multiprogramacion/grado_1).
escenarios_disponibles() {
    local d
    while IFS= read -r d; do
        d="${d#"$CP_DIR/escenarios/"}"; echo "${d%/configs}"
    done < <(find "$CP_DIR/escenarios" -type d -name configs | sort)
}

# Acepta el nombre completo o un pedazo que identifique a un solo escenario:
#   1 / 01 -> 01_instrucciones     fifo -> 02_fifo_un_core     5/2 o grado_2 -> 05_.../grado_2
buscar_escenario() {
    local pedido="$1" e coincidencias=()
    [[ -d "$CP_DIR/escenarios/$pedido/configs" ]] && { echo "$pedido"; return; }
    # Nombres cortos fijos (los que se usan con make). Van antes de la búsqueda por pedazo
    # porque «rr» por sí solo coincide con 03_rr_un_core y con 04_rr_dos_cores.
    case "$pedido" in
        instrucciones|aisladas)        pedido=01_instrucciones;;
        fifo)                          pedido=02_fifo_un_core;;
        rr)                            pedido=03_rr_un_core;;
        concurrente|rr2|dos_cores)     pedido=04_rr_dos_cores;;
        multi1|mp1|grado1)             pedido=05_multiprogramacion/grado_1;;
        multi2|mp2|grado2)             pedido=05_multiprogramacion/grado_2;;
    esac
    [[ -d "$CP_DIR/escenarios/$pedido/configs" ]] && { echo "$pedido"; return; }
    pedido="${pedido//./\/}"                                   # 5.2 -> 5/2
    if [[ "$pedido" =~ ^([0-9]+)(/([0-9]+))?$ ]]; then          # número: 1 -> 01_ ; 5/2 -> 05_*/grado_2
        local num; num="$(printf '%02d' "$((10#${BASH_REMATCH[1]}))")"
        while IFS= read -r e; do
            [[ "$e" == "$num"_* ]] || continue
            [[ -z "${BASH_REMATCH[3]}" || "$e" == *"_${BASH_REMATCH[3]}" ]] && coincidencias+=("$e")
        done < <(escenarios_disponibles)
    else
        while IFS= read -r e; do
            [[ "$e" == *"$pedido"* ]] && coincidencias+=("$e")
        done < <(escenarios_disponibles)
    fi
    if [[ ${#coincidencias[@]} -eq 1 ]]; then echo "${coincidencias[0]}"; return; fi
    if [[ ${#coincidencias[@]} -eq 0 ]]; then error "no hay ningún escenario que coincida con '$1'"
    else error "'$1' coincide con varios escenarios: ${coincidencias[*]}"; fi
    listar_escenarios >&2
    exit 1
}

resolver_escenario() {
    resolver_checkpoint
    if [[ -z "$ESCENARIO" ]]; then
        error "falta el escenario"
        listar_escenarios >&2
        exit 1
    fi
    ESCENARIO="$(buscar_escenario "$ESCENARIO")"
    ESC_DIR="$CP_DIR/escenarios/$ESCENARIO"
    ESC_ID="${ESCENARIO//\//_}"                      # 05_multiprogramacion/grado_1 -> ..._grado_1
    RUN_DIR="$CP_DIR/runtime/_deploy/$ESC_ID"

    [[ -n "$JOB" ]] || JOB="$(job_por_defecto "$ESC_DIR")"
    [[ -n "$JOB" ]] || morir "el README de $ESCENARIO no indica Job inicial: pasalo con --job"
    [[ -f "$CP_DIR/scripts/$JOB" ]] || morir "no existe el Job $CP_DIR/scripts/$JOB"
    if [[ "$JOB_ABSOLUTO" == 1 ]]; then JOB_ARG="$CP_DIR/scripts/$JOB"; else JOB_ARG="$JOB"; fi

    CORES=()
    local f
    for f in "$ESC_DIR"/configs/core-*.config; do
        [[ -e "$f" ]] || continue
        f="${f##*/core-}"; CORES+=("${f%.config}")
    done
}

# ---- Configs -----------------------------------------------------------------
# cfg_get <archivo> <CLAVE>  -> valor (tolera espacios alrededor del '=')
# Si la clave no está devuelve vacío sin fallar (con pipefail, un grep sin coincidencias cortaría el script).
cfg_get() { { grep -E "^$2[[:space:]]*=" "$1" || true; } | head -1 | sed -E 's/^[^=]*=[[:space:]]*//; s/[[:space:]]*$//'; }

# cfg_set <archivo> <CLAVE> <valor>  -> solo si la clave existe
cfg_set() {
    if grep -qE "^$2[[:space:]]*=" "$1"; then
        sed -i -E "s|^($2[[:space:]]*=[[:space:]]*).*|\1$3|" "$1"
    fi
}

# Decide con qué archivo arranca cada módulo. Por defecto es el ORIGINAL de la carpeta
# del escenario (escenarios/<escenario>/configs/<modulo>.config): no se copia ni se
# modifica nada, el módulo recibe esa ruta como parámetro.
#
# Solo se usa una copia (en runtime/_deploy/<escenario>/configs/, ignorado por git) para
# un módulo cuya config tiene que cambiar, y eso pasa únicamente si se pide:
#   - distribuido: una IP de servidor distinta de la que trae la config (127.0.0.1)
#   - --rapido / --log-level
declare -gA CFG
preparar_configs() {
    expandir_modulos
    local f nombre clave valor quiere cambia originales="" copias=""
    for f in "$ESC_DIR"/configs/*.config; do
        nombre="$(basename "$f" .config)"
        cambia=0
        # un módulo que no corre en esta máquina usa siempre el original (ni se lee)
        if ! seleccionado "${nombre/-/}"; then CFG[$nombre]="$f"; continue; fi
        [[ "$RAPIDO" == 1 || -n "$LOG_LEVEL_OVERRIDE" ]] && cambia=1
        for clave in IP_PLACA IP_STORAGE IP_PLANIFICADOR; do
            valor="$(cfg_get "$f" "$clave")"; quiere="${!clave}"
            [[ -n "$valor" && "$valor" != "$quiere" ]] && cambia=1
        done
        if [[ "$cambia" == 0 ]]; then
            CFG[$nombre]="$f"; originales+=" $nombre"
            continue
        fi
        mkdir -p "$RUN_DIR/configs"
        cp "$f" "$RUN_DIR/configs/"
        CFG[$nombre]="$RUN_DIR/configs/$nombre.config"; copias+=" $nombre"
        cfg_set "${CFG[$nombre]}" IP_PLACA "$IP_PLACA"
        cfg_set "${CFG[$nombre]}" IP_STORAGE "$IP_STORAGE"
        cfg_set "${CFG[$nombre]}" IP_PLANIFICADOR "$IP_PLANIFICADOR"
        [[ -z "$LOG_LEVEL_OVERRIDE" ]] || cfg_set "${CFG[$nombre]}" LOG_LEVEL "$LOG_LEVEL_OVERRIDE"
        if [[ "$RAPIDO" == 1 ]]; then
            cfg_set "${CFG[$nombre]}" RETARDO_MEMORIA 10
            cfg_set "${CFG[$nombre]}" RR_QUANTUM 20
        fi
    done
    info "Configs de escenarios/$ESCENARIO/configs/"
    [[ -z "$originales" ]] || detalle "originales, sin cambios:${originales}"
    if [[ -n "$copias" ]]; then
        detalle "con cambios (copia en runtime/_deploy/$ESC_ID/configs/):${copias}"
        [[ "$IP_PLACA$IP_STORAGE$IP_PLANIFICADOR" == "127.0.0.1127.0.0.1127.0.0.1" ]] \
            || detalle "IPs: placa=$IP_PLACA  storage=$IP_STORAGE  planificador=$IP_PLANIFICADOR"
        [[ "$RAPIDO" == 0 ]] || detalle "--rapido: RETARDO_MEMORIA=10, RR_QUANTUM=20"
        [[ -z "$LOG_LEVEL_OVERRIDE" ]] || detalle "LOG_LEVEL=$LOG_LEVEL_OVERRIDE"
    fi

    PUERTO_PLACA="$(cfg_get "${CFG[placa]}" PUERTO_ESCUCHA)"
    PUERTO_STORAGE="$(cfg_get "${CFG[storage]}" PUERTO_ESCUCHA)"
    PUERTO_PLANIFICADOR="$(cfg_get "${CFG[planificador]}" PUERTO_ESCUCHA)"
}

# Crea las carpetas de salida que usan las configs (datos, storage y la carpeta del
# offload / reportes), como pide el README del escenario. offload.dat y reportes.log
# NO se crean: los generan los módulos.
preparar_runtime() {
    local d
    for d in "$(cfg_get "${CFG[planificador]}" PATH_DATOS)" \
             "$(cfg_get "${CFG[storage]}" PATH_STORAGE)" \
             "$(dirname "$(cfg_get "${CFG[planificador]}" PATH_REPORTES)")" \
             "$(dirname "$(cfg_get "${CFG[placa]}" PATH_OFFLOAD)")"; do
        [[ -n "$d" ]] && (cd "$CP_DIR" && mkdir -p "$d")
    done
}

# ---- Selección de módulos (para el deploy en varias máquinas) ---------------
# Expande «cores» a core<N> por cada config del escenario.
expandir_modulos() {
    local m out="" id
    for m in $MODULOS; do
        case "$m" in
            storage|placa|planificador) out+=" $m";;
            cores) for id in "${CORES[@]}"; do out+=" core$id"; done;;
            core[0-9]*)
                [[ -f "$ESC_DIR/configs/core-${m#core}.config" ]] || morir "$ESCENARIO no tiene core-${m#core}.config"
                out+=" $m";;
            *) morir "módulo desconocido en --modulos: '$m' (válidos: storage, placa, planificador, cores, core1, core2...)";;
        esac
    done
    MODULOS="${out# }"
}

seleccionado() { [[ " $MODULOS " == *" $1 "* ]]; }
algun_core()   { [[ " $MODULOS " == *" core"* ]]; }
es_local()     { [[ "$1" == "127.0.0.1" || "$1" == "localhost" ]]; }

# Corta ANTES de levantar nada si un cliente de esta máquina apunta a 127.0.0.1 para
# un servidor que NO corre acá (la falla clásica del deploy en varias VMs).
validar_ips() {
    local errores=0
    if seleccionado planificador; then
        if ! seleccionado placa && es_local "$IP_PLACA"; then
            error "el Planificador corre acá pero la Placa no: pasá --ip-placa <IP de la Placa>"; errores=1; fi
        if ! seleccionado storage && es_local "$IP_STORAGE"; then
            error "el Planificador corre acá pero el Storage no: pasá --ip-storage <IP del Storage>"; errores=1; fi
    fi
    if algun_core; then
        if ! seleccionado planificador && es_local "$IP_PLANIFICADOR"; then
            error "hay Cores acá pero el Planificador no: pasá --ip-planificador <IP del Planificador>"; errores=1; fi
        if ! seleccionado placa && es_local "$IP_PLACA"; then
            error "hay Cores acá pero la Placa no: pasá --ip-placa <IP de la Placa>"; errores=1; fi
    fi
    [[ $errores -eq 0 ]] || exit 1
}

# ---- Chequeos previos --------------------------------------------------------
binario() { echo "$REPO_ROOT/$1/bin/$1"; }

chequear_binarios() {
    local m mod
    for m in $MODULOS; do
        mod="$m"; [[ "$m" == core* ]] && mod=core
        [[ -x "$(binario "$mod")" ]] || morir "falta $(binario "$mod"). Compilá con ./scripts/build.sh"
    done
}

puerto_escuchando() { ss -ltnH "sport = :$1" 2>/dev/null | grep -q .; }

chequear_puertos_libres() {
    local ocupados=""
    seleccionado storage      && puerto_escuchando "$PUERTO_STORAGE"      && ocupados+=" storage:$PUERTO_STORAGE"
    seleccionado placa        && puerto_escuchando "$PUERTO_PLACA"        && ocupados+=" placa:$PUERTO_PLACA"
    seleccionado planificador && puerto_escuchando "$PUERTO_PLANIFICADOR" && ocupados+=" planificador:$PUERTO_PLANIFICADOR"
    if [[ -n "$ocupados" ]]; then
        morir "ya hay procesos escuchando en:$ocupados
       Bajalos con ./deploy/bajar.sh y volvé a intentar."
    fi
}

chequear_tmux() {
    command -v tmux >/dev/null || morir "falta tmux. Instalalo con: sudo apt-get install -y tmux"
}

# Espera a que un servidor de ESTA máquina escuche. Se mira la tabla de sockets con
# `ss` en vez de conectarse: una conexión de prueba le dejaría al servidor un
# «error de handshake» en los logs que se evalúan.
esperar_escucha() {
    local nombre="$1" puerto="$2" i
    for ((i = 0; i < TIMEOUT_ESCUCHA * 4; i++)); do
        puerto_escuchando "$puerto" && { detalle "$nombre escuchando en :$puerto"; return 0; }
        sleep 0.25
    done
    morir "$nombre no empezó a escuchar en :$puerto después de ${TIMEOUT_ESCUCHA}s.
       Mirá su ventana con ./deploy/attach.sh"
}

# ---- Lanzamiento en tmux -----------------------------------------------------
# lanzar <ventana> <binario> <args...>
# Cada módulo va en su ventana de la sesión tmux, con directorio de trabajo
# checkpoints/<checkpoint>/. El «; exec bash» deja la ventana abierta aunque el
# módulo termine o crashee, para poder leer el error.
lanzar() {
    local ventana="$1"; shift
    local cmd; cmd="$(printf '%q ' "$@")"
    detalle "$ventana: $*"
    if tmux has-session -t "$TMUX_SESSION" 2>/dev/null; then
        tmux new-window -t "$TMUX_SESSION" -n "$ventana" -c "$CP_DIR" "$cmd; exec bash"
    else
        tmux new-session -d -s "$TMUX_SESSION" -n "$ventana" -c "$CP_DIR" "$cmd; exec bash"
        # mouse: clic en la barra de abajo cambia de módulo y la rueda sube por los logs
        tmux set-option -t "$TMUX_SESSION" mouse on >/dev/null
        tmux set-option -t "$TMUX_SESSION" history-limit 50000 >/dev/null
    fi
}

guardar_estado() {
    mkdir -p "$CP_DIR/runtime/_deploy"
    {
        echo "CP_DIR=$(printf '%q' "$CP_DIR")"
        echo "ESC_ID=$(printf '%q' "$ESC_ID")"
        echo "MODULOS=$(printf '%q' "$MODULOS")"
        local k; printf 'CFGS_USADAS=('; for k in "${!CFG[@]}"; do printf '%q ' "${CFG[$k]}"; done; echo ')' 
    } > "$DEPLOY_DIR/.ultimo_deploy"
}

# Orquesta un escenario completo con los módulos seleccionados.
levantar_escenario() {
    expandir_modulos
    validar_ips
    chequear_tmux
    chequear_binarios
    if tmux has-session -t "$TMUX_SESSION" 2>/dev/null; then
        morir "ya hay una sesión tmux '$TMUX_SESSION' (otro escenario corriendo).
       Bajala con ./deploy/bajar.sh"
    fi
    chequear_puertos_libres
    preparar_runtime
    guardar_estado

    info "Escenario $ESCENARIO — módulos en esta máquina: $MODULOS"
    detalle "directorio de trabajo: $CP_DIR"
    detalle "Job inicial: $JOB_ARG"

    if seleccionado storage; then
        lanzar storage "$(binario storage)" "${CFG[storage]}"
        esperar_escucha storage "$PUERTO_STORAGE"
    fi
    if seleccionado placa; then
        lanzar placa "$(binario placa)" "${CFG[placa]}"
        esperar_escucha placa "$PUERTO_PLACA"
    fi
    if seleccionado planificador; then
        es_local "$IP_PLACA"   || detalle "(la Placa está en $IP_PLACA: tiene que estar levantada antes)"
        es_local "$IP_STORAGE" || detalle "(el Storage está en $IP_STORAGE: tiene que estar levantado antes)"
        lanzar planificador "$(binario planificador)" "${CFG[planificador]}" "$JOB_ARG"
        esperar_escucha planificador "$PUERTO_PLANIFICADOR"
    fi
    local m id
    for m in $MODULOS; do
        [[ "$m" == core* ]] || continue
        id="${m#core}"
        if [[ -z "${_aviso_remoto:-}" ]]; then
            es_local "$IP_PLANIFICADOR" || detalle "(el Planificador está en $IP_PLANIFICADOR: tiene que estar levantado antes)"
            _aviso_remoto=1
        fi
        lanzar "core$id" "$(binario core)" "${CFG[core-$id]}" "$id"
        sleep 0.5
    done

    echo
    info "Listo. Los módulos corren en la sesión tmux '$TMUX_SESSION'."
    detalle "ver los módulos:    make ver   (clic en la barra de abajo, o Ctrl+b, soltar, y el número)"
    detalle "salir sin cortar:   Ctrl+b, soltar, d"
    detalle "logs de cada módulo: $CP_DIR/*.log"
    detalle "bajar y guardar evidencias: ./deploy/bajar.sh"
}

# ---- Puntos de entrada ---------------------------------------------------------
# Los usan probar.sh / levantar.sh (escenario por parámetro) y los scripts de cada
# prueba (01_instrucciones.sh, 02_fifo.sh...), que fijan el escenario:
#   correr_local 02_fifo_un_core "$@"
#   correr_distribuido 02_fifo_un_core "$@"

# En los scripts de cada prueba el escenario ya viene fijo: no se pide como argumento.
_arg_escenario() { [[ -n "${ESC_FIJO:-}" ]] || echo -n "<escenario> "; }

_uso_local() {
    cat >&2 <<USO
Uso: $(basename "$0") $(_arg_escenario)[opciones]

Levanta TODOS los módulos ${ESC_FIJO:+de $ESC_FIJO }en esta máquina, cada uno en su ventana de tmux.
Cada módulo arranca con la config ORIGINAL de la carpeta del escenario en el repo de pruebas
(escenarios/<escenario>/configs/<modulo>.config); las configs del TP no se usan.
Para repartir los módulos entre varias máquinas: deploy/distribuido/

Opciones:
USO
    opciones_comunes
}

_uso_distribuido() {
    cat >&2 <<USO
Uso: $(basename "$0") $(_arg_escenario)--modulos <lista> [--ip-... <ip>] [opciones]

Levanta en ESTA máquina solo los módulos de --modulos. Los clientes apuntan a las IPs
de los servidores que corren en otras máquinas. Se corre en cada máquina, en orden:
primero la que tiene placa/storage, después el planificador, al final los cores.

Opciones de red:
USO
    opciones_red
    echo "Opciones generales:" >&2
    opciones_comunes
}

correr_local() {
    usage() { _uso_local; }
    parse_args 0 "$@"
    resolver_escenario
    preparar_configs
    levantar_escenario
}

correr_distribuido() {
    usage() { _uso_distribuido; }
    # --modulos es obligatorio: así nadie levanta todo en cada máquina por error.
    if [[ " $* " != *" --modulos "* && " $* " != *" -h "* && " $* " != *" --help "* && " $* " != *" --listar "* ]]; then
        error "falta --modulos (qué módulos corren en ESTA máquina)"
        usage
        exit 1
    fi
    parse_args 1 "$@"
    resolver_escenario
    preparar_configs
    levantar_escenario
    echo
    info "IPs de esta máquina (pasáselas a las otras con --ip-...):"
    hostname -I 2>/dev/null | tr ' ' '\n' | grep -v '^$' | sed 's/^/    /' || true
}
