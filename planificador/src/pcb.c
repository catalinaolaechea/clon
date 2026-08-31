#include "pcb.h"

char* estado_to_string(t_estado estado) {
    switch (estado) {
        case ESTADO_NEW:      return "NEW";
        case ESTADO_READY:    return "READY";
        case ESTADO_EXEC:     return "EXEC";
        case ESTADO_BLOCKED:  return "BLOCKED";
        case ESTADO_EXIT:     return "EXIT";
        default:              return "DESCONOCIDO";
    }
}
