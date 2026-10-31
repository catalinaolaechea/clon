# Resultado esperado: LABEL

Script: [`scripts/label.asm`](../scripts/label.asm). Escenario: `04_label`.

```
ALLOC P0 64
SET CX 8
LABEL P0 CX     el Labeler pide 8 caracteres por teclado y los escribe en la memoria del Job
REPORT P0 CX    el Logger los vuelve a leer y los escribe en hexadecimal
FREE P0
EXIT
```

Si se tipea `ENTRENAR`, en `runtime/04_label/reportes.log` tiene que aparecer:

```
45 4E 54 52 45 4E 41 52
```

| Letra | E | N | T | R | E | N | A | R |
|---|---|---|---|---|---|---|---|---|
| ASCII (hexa) | 45 | 4E | 54 | 52 | 45 | 4E | 41 | 52 |

Si se tipean menos de 8 caracteres, el Labeler los vuelve a pedir; si se tipean más, usa los primeros 8 y deja un warning en el log del Planificador.
