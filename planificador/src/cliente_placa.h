#ifndef CLIENTE_PLACA_H
#define CLIENTE_PLACA_H
#include <stdint.h>
#include <stdbool.h>

#define LOG_DESLOCKEO "## (%u) - Deslockeo de paginas"

bool placa_crear_job(uint32_t jid, char* archivo_pseudocodigo);
bool placa_finalizar_job(uint32_t jid);

// Pide a la Placa que deslockee todas las paginas del Job y espera DESLOCKEO_OK.
bool placa_deslockear_paginas(uint32_t jid);

#endif