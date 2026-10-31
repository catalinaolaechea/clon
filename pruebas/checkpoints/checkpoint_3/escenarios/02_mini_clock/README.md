# 02_mini_clock

## Objetivo y configuración

El Mini Entrenamiento (Check 3, pág. 32 del enunciado) con **poca memoria**: el Job pide 640 bytes (10 páginas) y la Placa tiene 256 bytes (4 marcos de 64). Así se fuerzan Page Faults, reemplazos con **CLOCK-M** y bajadas al Offload.

| Parámetro | Valor |
| --- | --- |
| Algoritmo de planificación | FIFO |
| Cores | 1 |
| Memoria de la Placa | 256 bytes = 4 marcos de 64 |
| Algoritmo de reemplazo | CLOCK-M |
| Retardo de memoria / Offload | 10 ms / 100 ms |
| Job inicial por defecto | `mini_entrenamiento.asm` |

¿Por qué 4 marcos y no menos? FORWARD y BACKWARD tocan **3 páginas a la vez** (pesos, entrada y salida), y las tres quedan lockeadas mientras dura la instrucción. Con 2 marcos la instrucción nunca podría completarse.

## Arrancar

Desde la raíz del repo del TP:

```bash
./deploy/local/cp3.sh 02_mini_clock
```

## Evidencia

- **Placa:** `## JID: <JID> - Asignación - Dirección Lógica: 0 - Tamaño: 640`, después muchas `## JID: <JID> - Subida a Memoria - Página: <N> - Marco: <M>` y, cuando se llenan los 4 marcos, la secuencia `## Reemplazo ...`, `## JID: <JID> - Bajada al Offload ...` (solo si la víctima estaba modificada) y `## ... Subida a Memoria ...`, en ese orden. Al final, `## JID: <JID> - Liberación - Dirección Lógica: 0 - Tamaño: 640`.
- **Core:** `## JID: <JID> - Page Fault - Página: <N>` y los accesos `## JID: <JID> - Acción: <Lectura/Escritura> - Dirección Física: ...`.
- **Planificador:** el Job pasa por **BLOCKED** en cada Page Fault y en cada LOAD_BATCH/REPORT, y vuelve a READY.
- **Reporte:** `runtime/02_mini_clock/reportes.log` tiene una línea con los 128 bytes de pesos finales. Compararla con [el resultado esperado](../../resultados/mini_entrenamiento.md).

El resultado del reporte tiene que ser **el mismo con LRU y con CLOCK-M**: el algoritmo solo cambia qué página se saca, no las cuentas. Lo que cambia entre los dos escenarios son los logs de `## Reemplazo`.
