# Pruebas del Check 3 (grupo)

La cátedra todavía no publicó el `checkpoint_3` en
[entrenados-pruebas](https://github.com/sisoputnfrba/entrenados-pruebas). Estas son pruebas propias del
grupo para el Check 3 (pág. 32 del enunciado): **memoria, paginación, reemplazo, Offload y servicios**.

La carpeta tiene la **misma estructura** que un checkpoint de la cátedra, así el deploy del repo la
levanta igual que el `checkpoint_2`: el directorio de trabajo de todos los módulos es esta carpeta, y
las rutas de las configs (`./scripts/`, `./datos/`, `./runtime/<escenario>/`) son relativas a ella.

## Escenarios

| Escenario | Qué prueba | Job inicial | Memoria de la Placa |
|---|---|---|---|
| `01_mini_lru` | Mini Entrenamiento con **LRU**: Page Faults, reemplazos y Offload | `mini_entrenamiento.asm` | 256 bytes (4 marcos) |
| `02_mini_clock` | Mini Entrenamiento con **CLOCK-M** | `mini_entrenamiento.asm` | 256 bytes (4 marcos) |
| `03_report` | el Logger (REPORT) por separado | `report.asm` | 4096 bytes |
| `04_label` | el Labeler (LABEL) por separado | `label.asm` | 4096 bytes |

Cada escenario tiene su `README.md` con qué mirar en los logs, y en [`resultados/`](resultados/) está
el resultado esperado de cada script, con las cuentas a mano.

## Correr

Desde la **raíz del repo del TP**, con los binarios compilados:

```bash
./deploy/local/cp3.sh --listar           # los escenarios
./deploy/local/cp3.sh 01_mini_lru        # o por número: ./deploy/local/cp3.sh 1
./deploy/local/cp3.sh 2 --log-level DEBUG
./deploy/bajar.sh                        # bajar todo
```

Acepta las mismas opciones que `deploy/local/probar.sh` (`--rapido`, `--log-level`, `--job`...).
Para repartir los módulos en varias máquinas: `./deploy/distribuido/cp3.sh <escenario> --modulos ...`.

Los módulos escriben sus `.log` en esta carpeta, y el Offload y los reportes en `runtime/<escenario>/`
(ignorados por git).

## Archivos

| Archivo | Qué es |
|---|---|
| `scripts/*.asm` | los Jobs (`PATH_INSTRUCCIONES=./scripts/`) |
| `datos/pesos1`, `datos/lote1` | lo que lee el Loader (`PATH_DATOS=./datos/`): 32 `uint32_t` cada uno |
| `generar_datos.sh` | arma los archivos de `datos/` (están versionados: solo hace falta si se cambian) |
| `calcular_esperado.py` | simula el Mini Entrenamiento y da el resultado esperado (`--markdown` para la tabla) |
| `resultados/*.md` | el resultado esperado de cada script |
