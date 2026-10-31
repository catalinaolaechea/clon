# Resultado esperado: REPORT

Script: [`scripts/report.asm`](../scripts/report.asm). Escenario: `03_report`.

```
ALLOC P0 64              página 0 del Job
SET CX 16
LOAD_BATCH lote1 P0 CX   el Loader escribe los primeros 16 bytes de datos/lote1: los números 1, 2, 3 y 4
REPORT P0 CX             el Logger lee esos 16 bytes y los escribe en hexadecimal
FREE P0
EXIT
```

`datos/lote1` empieza con `1, 2, 3, 4` como `uint32_t` en little endian, así que en `runtime/03_report/reportes.log` tiene que aparecer:

```
01 00 00 00 02 00 00 00 03 00 00 00 04 00 00 00
```

(el Logger escribe cada byte como dos dígitos hexadecimales, separados por espacio).
