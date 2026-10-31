# 03_report

## Objetivo y configuración

Prueba el **Logger** (REPORT) por separado: el Job carga con el Loader los primeros 16 bytes de `datos/lote1` y los reporta. Como los valores son conocidos (1, 2, 3, 4), el reporte se puede comparar byte a byte.

| Parámetro | Valor |
| --- | --- |
| Algoritmo de planificación | FIFO |
| Cores | 1 |
| Memoria de la Placa | 4096 bytes (no hay reemplazos) |
| Job inicial por defecto | `report.asm` |

## Arrancar

```bash
./deploy/local/cp3.sh 03_report
```

## Evidencia

- **Reporte** (`runtime/03_report/reportes.log`): una línea con `01 00 00 00 02 00 00 00 03 00 00 00 04 00 00 00`. Ver [resultado esperado](../../resultados/report.md).
- **Planificador:** el Job pasa a BLOCKED en el LOAD_BATCH y en el REPORT, con los logs `## (<JID>) inicia <SERVICIO>` y `## (<JID>) finalizó <SERVICIO> y pasa a READY`, y el deslockeo de páginas antes de volver a READY.
