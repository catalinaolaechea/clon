#ifndef PAGE_FAULT_H
#define PAGE_FAULT_H

#include <stdint.h>
#include "pcb.h"

void inicializar_page_faults(void);
void destruir_page_faults(void);
void iniciar_page_faults(void);
void encolar_page_fault(t_pcb* pcb, uint32_t nro_pagina);

#endif
