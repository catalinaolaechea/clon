#ifndef CORTO_PLAZO_H
#define CORTO_PLAZO_H

#define LOG_PRIORIDAD_HRRN "## (%u) - Prioridad HRRN calculada: %.2f - Estimacion proxima rafaga: %.2f"

void inicializar_corto_plazo(void);
void destruir_corto_plazo(void);
void corto_plazo_avisar_job_ready(void);
void corto_plazo_avisar_core_libre(void);
void iniciar_corto_plazo(void);

#endif
