# Atajos para los scripts de deploy/ (desde la raíz del repo). `make` muestra la ayuda.
#
#   make modulo placa fifo            ->  ./deploy/local/modulo.sh placa 02_fifo_un_core
#   make prueba rr rapido             ->  ./deploy/local/probar.sh 03_rr_un_core --rapido
#   make deploy 4 cores PLACA=<ip> PLANI=<ip>
#
# Las palabras que van después del comando son sus argumentos (no targets de make).
# La prueba se puede nombrar por número o por una palabra de su nombre:
#   1 instrucciones · 2 fifo · 3 rr · 4 concurrente · 5.1 multi1 · 5.2 multi2
# Cada módulo sigue compilándose con su propio Makefile (make -C placa).

COMANDOS := modulo prueba deploy compilar

# Si el primer objetivo es uno de los comandos, el resto de las palabras son argumentos:
# se las define como reglas vacías para que make no intente "construirlas".
ifneq ($(filter $(firstword $(MAKECMDGOALS)),$(COMANDOS)),)
  ARGS := $(wordlist 2,$(words $(MAKECMDGOALS)),$(MAKECMDGOALS))
  ifneq ($(ARGS),)
    # .PHONY: «placa», «core»... también son carpetas; sin esto make diría "is up to date"
    $(eval .PHONY: $(ARGS))
    $(eval $(ARGS):;@:)
  endif
endif

# Palabras sueltas que activan opciones en cualquier comando
RAPIDO   := $(if $(filter rapido,$(ARGS)),--rapido)
VALGRIND := $(if $(filter valgrind,$(ARGS)),--valgrind)
POS      := $(filter-out rapido valgrind,$(ARGS))
arg       = $(word $(1),$(POS))

# IPs para deploy (variables: make deploy ... PLACA=10.0.0.1 PLANI=10.0.0.2 STORAGE=10.0.0.1)
IPS := $(if $(PLACA),--ip-placa $(PLACA)) $(if $(STORAGE),--ip-storage $(STORAGE)) \
       $(if $(PLANI),--ip-planificador $(PLANI))

# La prueba 1 (instrucciones aisladas) se corre de a un archivo: «make prueba 1 5»
es_instrucciones = $(filter 1 01 instrucciones 01_instrucciones,$(1))

.PHONY: help modulo prueba deploy compilar listar ver bajar
.DEFAULT_GOAL := help

help:
	@echo "Uso: make <comando> [argumentos]"
	@echo ""
	@echo "  make modulo <modulo> [prueba] [id]     un módulo solo, en esta terminal"
	@echo "      make modulo placa                    placa con la config de la prueba 2 (fifo)"
	@echo "      make modulo placa rr                 placa con la config de la prueba 3"
	@echo "      make modulo core concurrente 2       el core 2 de la prueba 4"
	@echo "      make modulo placa fifo valgrind      dentro de valgrind"
	@echo ""
	@echo "  make prueba <prueba> [archivo]         la prueba completa en esta máquina (tmux)"
	@echo "      make prueba fifo"
	@echo "      make prueba rr rapido                retardo 10 ms y quantum 20 ms"
	@echo "      make prueba 1 5                      instrucciones aisladas con 05_loop.asm"
	@echo ""
	@echo "  make deploy <prueba> <modulos> [IPs]   parte de los módulos, en varias máquinas"
	@echo "      make deploy 4 storage,placa"
	@echo "      make deploy 4 planificador PLACA=192.168.0.10 STORAGE=192.168.0.10"
	@echo "      make deploy 4 cores PLACA=192.168.0.10 PLANI=192.168.0.11"
	@echo ""
	@echo "  make ver           entrar a tmux a ver los módulos (Ctrl+b n cambia, Ctrl+b d sale)"
	@echo "  make bajar         bajar todo y guardar los logs"
	@echo "  make listar        pruebas disponibles"
	@echo "  make compilar [modulo...]   todo, o solo los módulos indicados (ej. make compilar placa)"
	@echo ""
	@echo "Pruebas (número o nombre):  1 instrucciones   2 fifo   3 rr   4 concurrente"
	@echo "                            5.1 multi1        5.2 multi2"
	@echo "Palabras extra: rapido (retardo 10 ms, quantum 20 ms)   valgrind (solo modulo)"

# make modulo <modulo> [prueba=2] [id=1]
modulo:
	@test -n "$(call arg,1)" || { echo "Uso: make modulo <storage|placa|planificador|core> [prueba] [id]"; exit 1; }
	@./deploy/local/modulo.sh $(call arg,1) $(or $(call arg,2),2) --id $(or $(call arg,3),1) $(RAPIDO) $(VALGRIND)

# make prueba <prueba> [archivo de la prueba 1]
prueba:
	@test -n "$(call arg,1)" || { echo "Uso: make prueba <prueba> [rapido]   (make listar)"; exit 1; }
ifneq ($(and $(call es_instrucciones,$(call arg,1)),$(call arg,2)),)
	@./deploy/local/01_instrucciones.sh $(call arg,2) $(RAPIDO)
else
	@./deploy/local/probar.sh $(call arg,1) $(RAPIDO)
endif

# make deploy <prueba> <modulos> [archivo de la prueba 1] PLACA=... STORAGE=... PLANI=...
deploy:
	@test -n "$(call arg,2)" || { echo "Uso: make deploy <prueba> <modulos> [PLACA=ip] [STORAGE=ip] [PLANI=ip]"; \
	                              echo "     modulos: storage, placa, planificador, cores, core1, core2... (separados por coma)"; exit 1; }
ifneq ($(and $(call es_instrucciones,$(call arg,1)),$(call arg,3)),)
	@./deploy/distribuido/01_instrucciones.sh $(call arg,3) --modulos $(call arg,2) $(IPS) $(RAPIDO)
else
	@./deploy/distribuido/levantar.sh $(call arg,1) --modulos $(call arg,2) $(IPS) $(RAPIDO)
endif

# make compilar [modulo...]   (utils va primero porque los módulos enlazan contra ella)
compilar:
ifeq ($(POS),)
	@./scripts/build.sh
else
	@for m in utils $(filter-out utils,$(POS)); do $(MAKE) --no-print-directory -C $$m || exit 1; done
endif

listar:
	@./deploy/local/probar.sh --listar

ver:
	@./deploy/attach.sh

bajar:
	@./deploy/bajar.sh
