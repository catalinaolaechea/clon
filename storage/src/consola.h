//! contrato de la consola de Storage, especificamente los comandos que se escriben por teclado (stdin) mientras el modulo atiende al Planificador (Planificador se conecta a Storage -> Hace el handshake. Y en el Check 4 ya directamente cuando Job ejecuta las syscalls SAVE, LOAD, DELETE_CHECKPOINT es Core -> Planificador -> Storage, donde este ultimo lee guarda o borra en el volumen y contesta)

// include guards
#ifndef STORAGE_CONSOLA_H_
#define STORAGE_CONSOLA_H_

#define STORAGE_CONSOLA_PROMPT "Storage> "  // el textito que aparecera en la consola, como lo vamos a repetir y no queremos cosas sueltas la guardamos en una constante 

void* storage_consola_atender(void* sin_uso);  // esta funcion va a ir pidiendo lineas con "readline()" hasta que stdin se cierre (dejen de haber lineas). La firma "void* funcion(void*)" la pide la misma "pthread_create()" para cualquier funcion con hilos, por eso recibe un parametro que no usa

#endif
