#!/usr/bin/env bash
# Entra a la sesión tmux donde corren los módulos.
#   Clic en un nombre de la barra de abajo  -> cambia a ese módulo (rueda: sube por el log)
#   Ctrl+b, soltar, y el número (0, 1, 2...) -> va directo a ese módulo
#   Ctrl+b, soltar, n / p                    -> siguiente / anterior
#   Ctrl+b, soltar, d                        -> sale SIN cortar nada
SESION="${TMUX_SESSION:-entrenados}"
tmux has-session -t "$SESION" 2>/dev/null || { echo "No hay ninguna prueba corriendo (sesión '$SESION'). Levantá una con: make prueba <prueba>"; exit 1; }
tmux set-option -t "$SESION" mouse on >/dev/null
exec tmux attach -t "$SESION"
