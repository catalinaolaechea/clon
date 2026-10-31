# 04_label

## Objetivo y configuración

Prueba el **Labeler** (LABEL) por separado: el Job pide 8 caracteres por teclado, los guarda en su memoria y los reporta con REPORT. Lo que se tipea vuelve, en hexadecimal, en el archivo de reportes.

| Parámetro | Valor |
| --- | --- |
| Algoritmo de planificación | FIFO |
| Cores | 1 |
| Memoria de la Placa | 4096 bytes (no hay reemplazos) |
| Job inicial por defecto | `label.asm` |

## Arrancar

```bash
./deploy/local/cp3.sh 04_label
```

Cuando el Planificador muestre `## (<JID>) - Ingrese 8 caracteres:`, escribir `ENTRENAR` en su ventana (`make ver` y elegir la ventana del planificador) y Enter. O, desde otra terminal:

```bash
tmux send-keys -t entrenados:planificador "ENTRENAR" Enter
```

## Evidencia

- **Reporte** (`runtime/04_label/reportes.log`): `45 4E 54 52 45 4E 41 52` (los códigos ASCII de `ENTRENAR`). Ver [resultado esperado](../../resultados/label.md).
- Si se tipean más de 8 caracteres, el Labeler usa los primeros 8 y deja un warning; si se tipean menos, los vuelve a pedir.
