#!/usr/bin/env python3
"""Resultado esperado del Mini Entrenamiento (scripts/mini_entrenamiento.asm).

Simula sobre los 640 bytes del ALLOC lo que hacen FORWARD, BACKWARD y UPDATE (enunciado,
"Instrucciones de entrenamiento"), con aritmética de uint32_t: las restas que bajan de 0
dan la vuelta, igual que en el Core.

Uso: ./calcular_esperado.py            (lee datos/pesos1 y datos/lote1)
"""
import os
import struct
import sys

MASK = 0xFFFFFFFF
N = 32                                   # 128 bytes / 4 = 32 uint32_t por zona

AQUI = os.path.dirname(os.path.abspath(__file__))


def leer_u32(nombre):
    with open(os.path.join(AQUI, "datos", nombre), "rb") as f:
        datos = f.read(N * 4)
    if len(datos) < N * 4:
        sys.exit(f"datos/{nombre} tiene {len(datos)} bytes: hacen falta {N * 4} (corré generar_datos.sh)")
    return list(struct.unpack(f"<{N}I", datos))


# Las 5 zonas de 128 bytes del ALLOC P0 640
pesos      = leer_u32("pesos1")         # P0 + 0    (LOAD_BATCH pesos1)
entrada    = leer_u32("lote1")          # P0 + 128  (LOAD_BATCH lote1)
salida     = [0] * N                    # P0 + 256  (la escribe FORWARD)
gradientes = [0] * N                    # P0 + 384  (la escribe BACKWARD)
estado     = [0] * N                    # P0 + 512  (página nueva: arranca en 0)

# FORWARD P1 P2 P3 AX, 32 veces hacia adelante:  acum = acum + E1*E2 ; E3 = acum
ax = 0
for i in range(N):
    ax = (ax + pesos[i] * entrada[i]) & MASK
    salida[i] = ax

# BACKWARD P1 P2 P3 BX, 32 veces desde el final:  acum = acum - E1*E2 ; E3 = acum
bx = 0
for i in reversed(range(N)):
    bx = (bx - pesos[i] * salida[i]) & MASK
    gradientes[i] = bx

# UPDATE P1 P2 P3, 32 veces hacia adelante:  E2 = (E2 + E1) / 2 ; E3 = E3 - E2
for i in range(N):
    estado[i] = ((estado[i] + gradientes[i]) & MASK) // 2
    pesos[i] = (pesos[i] - estado[i]) & MASK


def hexa(valores):
    # como el Logger: cada byte en hexa, separados por espacio, en orden de memoria (little endian)
    return " ".join(f"{b:02X}" for b in struct.pack(f"<{len(valores)}I", *valores))


def tabla(nombre, valores):
    print(f"\n{nombre}:")
    for i, v in enumerate(valores):
        print(f"  [{i:2}] {v:>10}  (0x{v:08X})")


def markdown():
    # la tabla de resultados/mini_entrenamiento.md
    print("| i | entrada | pesos inicial | salida (FORWARD) | gradiente (BACKWARD) | estado (UPDATE) | peso final |")
    print("|---|---|---|---|---|---|---|")
    iniciales = leer_u32("pesos1")
    for i in range(N):
        print(f"| {i} | {entrada[i]} | {iniciales[i]} | {salida[i]} | {gradientes[i]} | {estado[i]} | {pesos[i]} |")
    print()
    print("REPORT (128 bytes de pesos finales):")
    print()
    print("```")
    print(hexa(pesos))
    print("```")


if __name__ == "__main__" and "--markdown" in sys.argv:
    markdown()
elif __name__ == "__main__":
    tabla("salida (FORWARD)", salida)
    tabla("gradientes (BACKWARD)", gradientes)
    tabla("estado (UPDATE)", estado)
    tabla("pesos finales (UPDATE)", pesos)
    print("\nREPORT P1 CX (los 128 bytes de pesos, como los escribe el Logger):")
    print(hexa(pesos))
