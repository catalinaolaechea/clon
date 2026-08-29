
// Define los modulos que pueden conectarse entre si y los canales que pueden usar para comunicarse
static bool handshake_valido(t_modulo servidor, t_modulo cliente, t_canal canal)  {
    
    switch(servidor) {
        case MODULO_PLANIFICADOR:
            if (cliente == MODULO_CORE && canal == CANAL_DISPATCH) return true;
            if (cliente == MODULO_CORE && canal == CANAL_INTERRUPT) return true;
            break;
        case MODULO_PLACA:
            if (cliente == MODULO_PLANIFICADOR && canal == CANAL_UNICO) return true;
            if (cliente == MODULO_CORE && canal == CANAL_UNICO) return true;
            break;
        case MODULO_STORAGE:
            if (cliente == MODULO_PLANIFICADOR && canal == CANAL_UNICO) return true;
            break;
        default:
            break;
    }

    return false;
}

