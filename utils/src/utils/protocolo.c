#include <utils/protocolo.h>

// Nombres tal como los escribe el enunciado (capitalizados), porque salen
// literales en los logs obligatorios de conexion.
char* modulo_to_string(t_modulo modulo) {
    switch (modulo) {
        case MODULO_PLANIFICADOR: return "Planificador";
        case MODULO_CORE:         return "Core";
        case MODULO_PLACA:        return "Placa";
        case MODULO_STORAGE:      return "Storage";
        default:                  return "Desconocido";
    }
}
