# Resultado esperado: Mini Entrenamiento

Script: [`scripts/mini_entrenamiento.asm`](../scripts/mini_entrenamiento.asm). Escenarios: `01_mini_lru` y `02_mini_clock` (el resultado es el mismo con los dos algoritmos).

Es el Job de ejemplo de la cátedra (`entrenados-pruebas/ejemplo_job/job_entrenamiento.asm`, versión con los `JNZ` corregidos) con dos cambios, porque los checkpoints son del Check 4:

- `LOAD_CHECKPOINT modelo7 P1 CX` → `LOAD_BATCH pesos1 P1 CX`: los pesos iniciales se cargan de un archivo.
- Se saca `SAVE_CHECKPOINT modelo7 P1 CX`.

## Datos

Los arma [`generar_datos.sh`](../generar_datos.sh) (32 `uint32_t` en little endian cada uno):

| Archivo | Zona del Job | Valores |
|---|---|---|
| `datos/pesos1` | `P0 + 0` (pesos) | 2, 2, 2, ..., 2 |
| `datos/lote1` | `P0 + 128` (entrada) | 1, 2, 3, ..., 32 |

Las zonas `salida` (`P0 + 256`), `gradientes` (`P0 + 384`) y `estado` (`P0 + 512`) arrancan en 0: son páginas que nunca se escribieron.

## Las cuentas a mano

Todo es `uint32_t`: una resta que baja de 0 **da la vuelta** (se le suma 2³² = 4294967296).

**FORWARD** (`acum = acum + peso[i] × entrada[i]`; `salida[i] = acum`), con `acum` arrancando en 0:

- `salida[0] = 0 + 2×1 = 2`
- `salida[1] = 2 + 2×2 = 6`
- `salida[2] = 6 + 2×3 = 12`
- En general, `salida[i] = (i+1)(i+2)`: la última es `salida[31] = 32 × 33 = 1056`.

**BACKWARD** (desde el final: `acum = acum - peso[i] × salida[i]`; `gradiente[i] = acum`), con `acum` arrancando en 0:

- `gradiente[31] = 0 - 2×1056 = -2112` → da la vuelta: `4294967296 - 2112 = 4294965184`
- `gradiente[30] = -2112 - 2×992 = -4096` → `4294963200`
- `gradiente[0]` acumula todo: `-2 × (2 + 6 + ... + 1056) = -2 × 11968 = -23936` → `4294943360`

**UPDATE** (`estado[i] = (estado[i] + gradiente[i]) / 2`; `peso[i] = peso[i] - estado[i]`), con `estado` en 0:

- `estado[31] = 4294965184 / 2 = 2147482592`; `peso[31] = 2 - 2147482592` → da la vuelta: `2147484706`
- `estado[0] = 4294943360 / 2 = 2147471680`; `peso[0] = 2 - 2147471680` → `2147495618`

## Tabla completa

La genera [`calcular_esperado.py --markdown`](../calcular_esperado.py), que hace las mismas cuentas:

| i | entrada | pesos inicial | salida (FORWARD) | gradiente (BACKWARD) | estado (UPDATE) | peso final |
|---|---|---|---|---|---|---|
| 0 | 1 | 2 | 2 | 4294943360 | 2147471680 | 2147495618 |
| 1 | 2 | 2 | 6 | 4294943364 | 2147471682 | 2147495616 |
| 2 | 3 | 2 | 12 | 4294943376 | 2147471688 | 2147495610 |
| 3 | 4 | 2 | 20 | 4294943400 | 2147471700 | 2147495598 |
| 4 | 5 | 2 | 30 | 4294943440 | 2147471720 | 2147495578 |
| 5 | 6 | 2 | 42 | 4294943500 | 2147471750 | 2147495548 |
| 6 | 7 | 2 | 56 | 4294943584 | 2147471792 | 2147495506 |
| 7 | 8 | 2 | 72 | 4294943696 | 2147471848 | 2147495450 |
| 8 | 9 | 2 | 90 | 4294943840 | 2147471920 | 2147495378 |
| 9 | 10 | 2 | 110 | 4294944020 | 2147472010 | 2147495288 |
| 10 | 11 | 2 | 132 | 4294944240 | 2147472120 | 2147495178 |
| 11 | 12 | 2 | 156 | 4294944504 | 2147472252 | 2147495046 |
| 12 | 13 | 2 | 182 | 4294944816 | 2147472408 | 2147494890 |
| 13 | 14 | 2 | 210 | 4294945180 | 2147472590 | 2147494708 |
| 14 | 15 | 2 | 240 | 4294945600 | 2147472800 | 2147494498 |
| 15 | 16 | 2 | 272 | 4294946080 | 2147473040 | 2147494258 |
| 16 | 17 | 2 | 306 | 4294946624 | 2147473312 | 2147493986 |
| 17 | 18 | 2 | 342 | 4294947236 | 2147473618 | 2147493680 |
| 18 | 19 | 2 | 380 | 4294947920 | 2147473960 | 2147493338 |
| 19 | 20 | 2 | 420 | 4294948680 | 2147474340 | 2147492958 |
| 20 | 21 | 2 | 462 | 4294949520 | 2147474760 | 2147492538 |
| 21 | 22 | 2 | 506 | 4294950444 | 2147475222 | 2147492076 |
| 22 | 23 | 2 | 552 | 4294951456 | 2147475728 | 2147491570 |
| 23 | 24 | 2 | 600 | 4294952560 | 2147476280 | 2147491018 |
| 24 | 25 | 2 | 650 | 4294953760 | 2147476880 | 2147490418 |
| 25 | 26 | 2 | 702 | 4294955060 | 2147477530 | 2147489768 |
| 26 | 27 | 2 | 756 | 4294956464 | 2147478232 | 2147489066 |
| 27 | 28 | 2 | 812 | 4294957976 | 2147478988 | 2147488310 |
| 28 | 29 | 2 | 870 | 4294959600 | 2147479800 | 2147487498 |
| 29 | 30 | 2 | 930 | 4294961340 | 2147480670 | 2147486628 |
| 30 | 31 | 2 | 992 | 4294963200 | 2147481600 | 2147485698 |
| 31 | 32 | 2 | 1056 | 4294965184 | 2147482592 | 2147484706 |

REPORT (128 bytes de pesos finales):

```
C2 2E 00 80 C0 2E 00 80 BA 2E 00 80 AE 2E 00 80 9A 2E 00 80 7C 2E 00 80 52 2E 00 80 1A 2E 00 80 D2 2D 00 80 78 2D 00 80 0A 2D 00 80 86 2C 00 80 EA 2B 00 80 34 2B 00 80 62 2A 00 80 72 29 00 80 62 28 00 80 30 27 00 80 DA 25 00 80 5E 24 00 80 BA 22 00 80 EC 20 00 80 F2 1E 00 80 CA 1C 00 80 72 1A 00 80 E8 17 00 80 2A 15 00 80 36 12 00 80 0A 0F 00 80 A4 0B 00 80 02 08 00 80 22 04 00 80
```

Cada número ocupa 4 bytes en memoria en little endian, así que `2147495618 = 0x80002EC2` aparece como `C2 2E 00 80`.

## Qué tiene que pasar además

- El Job hace `ALLOC 640` (10 páginas) y la Placa tiene 4 marcos: **tiene que haber reemplazos** (criterio 2 de SIS-80) y bajadas al Offload de las páginas modificadas.
- Al final, `FREE P0`: los 10 slots del Offload y los marcos vuelven a quedar libres.
