#ifndef JOBS_H
#define JOBS_H

#include <stdint.h>

#define LOG_CREACION_JOB "## (%u) Se crea el Job - Estado: NEW"

uint32_t crear_job(char* archivo_pseudocodigo);

#endif