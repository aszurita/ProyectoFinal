#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <time.h>
#include <signal.h>
#include <ncurses.h>

#define MAX_BANDAS 10
#define MAX_INGREDIENTES 20
#define MAX_ORDENES 100
#define MAX_NOMBRE_INGREDIENTE 50
#define CAPACIDAD_DISPENSADOR 20

// Estructura para ingredientes
typedef struct {
    char nombre[MAX_NOMBRE_INGREDIENTE];
    int cantidad;
    pthread_mutex_t mutex;
} Ingrediente;

// Estructura para una orden
typedef struct {
    int id_orden;
    char ingredientes_solicitados[MAX_INGREDIENTES][MAX_NOMBRE_INGREDIENTE];
    int num_ingredientes;
    time_t tiempo_creacion;
} Orden;

// Estructura para una banda de preparación
typedef struct {
    int id;
    int activa;
    int pausada;
    int hamburguesas_procesadas;
    int procesando_orden;
    Orden orden_actual;
    pthread_t hilo;
    pthread_mutex_t mutex;
    pthread_cond_t condicion;
} Banda;

// Cola FIFO para órdenes en espera
typedef struct {
    Orden ordenes[MAX_ORDENES];
    int frente;
    int atras;
    int tamano;
    pthread_mutex_t mutex;
    pthread_cond_t no_vacia;
    pthread_cond_t no_llena;
} ColaFIFO;

// Estructura de datos compartidos
typedef struct {
    Banda bandas[MAX_BANDAS];
    Ingrediente ingredientes[MAX_INGREDIENTES];
    ColaFIFO cola_espera;
    int num_bandas;
    int num_ingredientes;
    int sistema_activo;
    int total_ordenes_procesadas;
    pthread_mutex_t mutex_global;
    pthread_cond_t nueva_orden;
} DatosCompartidos;

// Variables globales
DatosCompartidos *datos_compartidos;
pthread_t hilo_interfaz_estado;
pthread_t hilo_interfaz_control;
pthread_t hilo_generador_ordenes;
char ingredientes_disponibles[MAX_INGREDIENTES][MAX_NOMBRE_INGREDIENTE] = {
    "pan_inferior", "pan_superior", "carne", "queso", "tomate", 
    "lechuga", "cebolla", "pepinillos", "mayonesa", "ketchup",
    "mostaza", "bacon", "champinones", "aguacate", "jalapeños"
};
int num_ingredientes_disponibles = 15;

// Prototipos de funciones
void inicializar_sistema(int num_bandas);
void* banda_worker(void* arg);
void* interfaz_estado(void* arg);
void* interfaz_control(void* arg);
void* generador_ordenes(void* arg);
void procesar_orden(int banda_id, Orden* orden);
int verificar_ingredientes_disponibles(Orden* orden);
void consumir_ingredientes(Orden* orden);
void encolar_orden(Orden* orden);
Orden* desencolar_orden();
void mostrar_estado_sistema();
void mostrar_interfaz_control();
void generar_orden_aleatoria(Orden* orden, int id);
void reabastecer_ingredientes();
void limpiar_sistema();
void manejar_senal(int sig);
int validar_parametros(int argc, char* argv[], int* num_bandas);
void mostrar_ayuda();

// Implementación de funciones principales
void inicializar_sistema(int num_bandas) {
    // Crear memoria compartida
    int shm_fd = shm_open("/burger_system", O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("Error creando memoria compartida");
        exit(1);
    }
    
    ftruncate(shm_fd, sizeof(DatosCompartidos));
    datos_compartidos = mmap(0, sizeof(DatosCompartidos), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    
    if (datos_compartidos == MAP_FAILED) {
        perror("Error mapeando memoria compartida");
        exit(1);
    }
    
    // Inicializar datos compartidos
    memset(datos_compartidos, 0, sizeof(DatosCompartidos));
    datos_compartidos->num_bandas = num_bandas;
    datos_compartidos->num_ingredientes = num_ingredientes_disponibles;
    datos_compartidos->sistema_activo = 1;
    datos_compartidos->total_ordenes_procesadas = 0;
    
    // Inicializar mutex y condiciones globales
    pthread_mutex_init(&datos_compartidos->mutex_global, NULL);
    pthread_cond_init(&datos_compartidos->nueva_orden, NULL);
    
    // Inicializar ingredientes
    for (int i = 0; i < num_ingredientes_disponibles; i++) {
        strcpy(datos_compartidos->ingredientes[i].nombre, ingredientes_disponibles[i]);
        datos_compartidos->ingredientes[i].cantidad = CAPACIDAD_DISPENSADOR;
        pthread_mutex_init(&datos_compartidos->ingredientes[i].mutex, NULL);
    }
    
    // Inicializar bandas
    for (int i = 0; i < num_bandas; i++) {
        datos_compartidos->bandas[i].id = i;
        datos_compartidos->bandas[i].activa = 1;
        datos_compartidos->bandas[i].pausada = 0;
        datos_compartidos->bandas[i].hamburguesas_procesadas = 0;
        datos_compartidos->bandas[i].procesando_orden = 0;
        pthread_mutex_init(&datos_compartidos->bandas[i].mutex, NULL);
        pthread_cond_init(&datos_compartidos->bandas[i].condicion, NULL);
    }
    
    // Inicializar cola FIFO
    datos_compartidos->cola_espera.frente = 0;
    datos_compartidos->cola_espera.atras = 0;
    datos_compartidos->cola_espera.tamano = 0;
    pthread_mutex_init(&datos_compartidos->cola_espera.mutex, NULL);
    pthread_cond_init(&datos_compartidos->cola_espera.no_vacia, NULL);
    pthread_cond_init(&datos_compartidos->cola_espera.no_llena, NULL);
    
    printf("Sistema inicializado con %d bandas de preparación\n", num_bandas);
    printf("Ingredientes disponibles: %d tipos\n", num_ingredientes_disponibles);
}

void* banda_worker(void* arg) {
    int banda_id = *(int*)arg;
    Banda* banda = &datos_compartidos->bandas[banda_id];
    
    while (datos_compartidos->sistema_activo) {
        pthread_mutex_lock(&banda->mutex);
        
        // Esperar mientras esté pausada
        while (banda->pausada && datos_compartidos->sistema_activo) {
            pthread_cond_wait(&banda->condicion, &banda->mutex);
        }
        
        if (!datos_compartidos->sistema_activo) {
            pthread_mutex_unlock(&banda->mutex);
            break;
        }
        
        pthread_mutex_unlock(&banda->mutex);
        
        // Intentar obtener una orden de la cola
        Orden* orden = desencolar_orden();
        if (orden != NULL) {
            // Verificar si la banda puede procesar la orden
            if (verificar_ingredientes_disponibles(orden)) {
                pthread_mutex_lock(&banda->mutex);
                banda->procesando_orden = 1;
                banda->orden_actual = *orden;
                pthread_mutex_unlock(&banda->mutex);
                
                // Procesar la orden
                procesar_orden(banda_id, orden);
                
                pthread_mutex_lock(&banda->mutex);
                banda->hamburguesas_procesadas++;
                banda->procesando_orden = 0;
                pthread_mutex_unlock(&banda->mutex);
                
                pthread_mutex_lock(&datos_compartidos->mutex_global);
                datos_compartidos->total_ordenes_procesadas++;
                pthread_mutex_unlock(&datos_compartidos->mutex_global);
            } else {
                // Re-encolar la orden si no hay ingredientes
                encolar_orden(orden);
                // Emitir alerta de falta de ingredientes
                printf("\n[ALERTA] Ingredientes insuficientes para orden %d\n", orden->id_orden);
            }
        } else {
            // Esperar un poco antes de verificar nuevamente
            usleep(100000); // 100ms
        }
    }
    
    return NULL;
}

void* interfaz_estado(void* arg) {
    while (datos_compartidos->sistema_activo) {
        mostrar_estado_sistema();
        sleep(1);
    }
    return NULL;
}

void* interfaz_control(void* arg) {
    while (datos_compartidos->sistema_activo) {
        mostrar_interfaz_control();
        sleep(1);
    }
    return NULL;
}

void* generador_ordenes(void* arg) {
    int contador_ordenes = 1;
    
    while (datos_compartidos->sistema_activo) {
        Orden nueva_orden;
        generar_orden_aleatoria(&nueva_orden, contador_ordenes++);
        encolar_orden(&nueva_orden);
        
        pthread_mutex_lock(&datos_compartidos->mutex_global);
        pthread_cond_broadcast(&datos_compartidos->nueva_orden);
        pthread_mutex_unlock(&datos_compartidos->mutex_global);
        
        // Generar nueva orden cada 3-7 segundos
        sleep(rand() % 5 + 3);
        
        // Ocasionalmente reabastecer ingredientes
        if (rand() % 10 == 0) {
            reabastecer_ingredientes();
        }
    }
    return NULL;
}

void procesar_orden(int banda_id, Orden* orden) {
    printf("\n[BANDA %d] Procesando orden %d con %d ingredientes\n", 
           banda_id, orden->id_orden, orden->num_ingredientes);
    
    // Consumir ingredientes
    consumir_ingredientes(orden);
    
    // Simular tiempo de preparación
    for (int i = 0; i < orden->num_ingredientes; i++) {
        printf("[BANDA %d] Agregando %s...\n", banda_id, orden->ingredientes_solicitados[i]);
        sleep(1); // Simular tiempo de preparación
    }
    
    printf("[BANDA %d] ¡Hamburguesa %d completada!\n", banda_id, orden->id_orden);
}

int verificar_ingredientes_disponibles(Orden* orden) {
    for (int i = 0; i < orden->num_ingredientes; i++) {
        int encontrado = 0;
        for (int j = 0; j < datos_compartidos->num_ingredientes; j++) {
            if (strcmp(orden->ingredientes_solicitados[i], 
                      datos_compartidos->ingredientes[j].nombre) == 0) {
                pthread_mutex_lock(&datos_compartidos->ingredientes[j].mutex);
                if (datos_compartidos->ingredientes[j].cantidad <= 0) {
                    pthread_mutex_unlock(&datos_compartidos->ingredientes[j].mutex);
                    return 0;
                }
                pthread_mutex_unlock(&datos_compartidos->ingredientes[j].mutex);
                encontrado = 1;
                break;
            }
        }
        if (!encontrado) return 0;
    }
    return 1;
}

void consumir_ingredientes(Orden* orden) {
    for (int i = 0; i < orden->num_ingredientes; i++) {
        for (int j = 0; j < datos_compartidos->num_ingredientes; j++) {
            if (strcmp(orden->ingredientes_solicitados[i], 
                      datos_compartidos->ingredientes[j].nombre) == 0) {
                pthread_mutex_lock(&datos_compartidos->ingredientes[j].mutex);
                if (datos_compartidos->ingredientes[j].cantidad > 0) {
                    datos_compartidos->ingredientes[j].cantidad--;
                }
                pthread_mutex_unlock(&datos_compartidos->ingredientes[j].mutex);
                break;
            }
        }
    }
}

void encolar_orden(Orden* orden) {
    pthread_mutex_lock(&datos_compartidos->cola_espera.mutex);
    
    while (datos_compartidos->cola_espera.tamano >= MAX_ORDENES) {
        pthread_cond_wait(&datos_compartidos->cola_espera.no_llena, 
                         &datos_compartidos->cola_espera.mutex);
    }
    
    datos_compartidos->cola_espera.ordenes[datos_compartidos->cola_espera.atras] = *orden;
    datos_compartidos->cola_espera.atras = (datos_compartidos->cola_espera.atras + 1) % MAX_ORDENES;
    datos_compartidos->cola_espera.tamano++;
    
    pthread_cond_signal(&datos_compartidos->cola_espera.no_vacia);
    pthread_mutex_unlock(&datos_compartidos->cola_espera.mutex);
}

Orden* desencolar_orden() {
    static Orden orden_temp;
    
    pthread_mutex_lock(&datos_compartidos->cola_espera.mutex);
    
    if (datos_compartidos->cola_espera.tamano == 0) {
        pthread_mutex_unlock(&datos_compartidos->cola_espera.mutex);
        return NULL;
    }
    
    orden_temp = datos_compartidos->cola_espera.ordenes[datos_compartidos->cola_espera.frente];
    datos_compartidos->cola_espera.frente = (datos_compartidos->cola_espera.frente + 1) % MAX_ORDENES;
    datos_compartidos->cola_espera.tamano--;
    
    pthread_cond_signal(&datos_compartidos->cola_espera.no_llena);
    pthread_mutex_unlock(&datos_compartidos->cola_espera.mutex);
    
    return &orden_temp;
}

void mostrar_estado_sistema() {
    system("clear");
    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║              SISTEMA DE HAMBURGUESAS - ESTADO               ║\n");
    printf("╠══════════════════════════════════════════════════════════════╣\n");
    printf("║ Total órdenes procesadas: %-6d  Órdenes en cola: %-6d ║\n", 
           datos_compartidos->total_ordenes_procesadas,
           datos_compartidos->cola_espera.tamano);
    printf("╠══════════════════════════════════════════════════════════════╣\n");
    printf("║                    ESTADO DE BANDAS                          ║\n");
    printf("╠══════════════════════════════════════════════════════════════╣\n");
    
    for (int i = 0; i < datos_compartidos->num_bandas; i++) {
        Banda* banda = &datos_compartidos->bandas[i];
        pthread_mutex_lock(&banda->mutex);
        
        char estado[20];
        if (!banda->activa) strcpy(estado, "INACTIVA");
        else if (banda->pausada) strcpy(estado, "PAUSADA");
        else if (banda->procesando_orden) strcpy(estado, "PROCESANDO");
        else strcpy(estado, "ESPERANDO");
        
        printf("║ Banda %d: %-12s | Procesadas: %-6d | Orden: %-6d ║\n", 
               i, estado, banda->hamburguesas_procesadas,
               banda->procesando_orden ? banda->orden_actual.id_orden : 0);
        pthread_mutex_unlock(&banda->mutex);
    }
    
    printf("╠══════════════════════════════════════════════════════════════╣\n");
    printf("║                   INVENTARIO INGREDIENTES                    ║\n");
    printf("╠══════════════════════════════════════════════════════════════╣\n");
    
    for (int i = 0; i < datos_compartidos->num_ingredientes; i++) {
        pthread_mutex_lock(&datos_compartidos->ingredientes[i].mutex);
        int cantidad = datos_compartidos->ingredientes[i].cantidad;
        char* nombre = datos_compartidos->ingredientes[i].nombre;
        
        char alerta[10] = "";
        if (cantidad <= 2) strcpy(alerta, " [BAJO!]");
        if (cantidad == 0) strcpy(alerta, " [AGOTADO!]");
        
        printf("║ %-15s: %2d%s%*s║\n", nombre, cantidad, alerta, 
               37 - (int)strlen(nombre) - (int)strlen(alerta), "");
        pthread_mutex_unlock(&datos_compartidos->ingredientes[i].mutex);
    }
    
    printf("╚══════════════════════════════════════════════════════════════╝\n");
    printf("Presiona Ctrl+C para salir del sistema\n");
}

void mostrar_interfaz_control() {
    // Esta función se ejecutaría en un proceso/hilo separado
    // Para simplicidad, mostramos comandos disponibles
    if (rand() % 20 == 0) { // Mostrar ocasionalmente
        printf("\n[CONTROL] Comandos disponibles:\n");
        printf("  - Pausar banda: kill -USR1 <pid>\n");
        printf("  - Reanudar banda: kill -USR2 <pid>\n");
        printf("  - Reabastecer: kill -CONT <pid>\n");
    }
}

void generar_orden_aleatoria(Orden* orden, int id) {
    orden->id_orden = id;
    orden->num_ingredientes = rand() % 6 + 3; // 3-8 ingredientes
    orden->tiempo_creacion = time(NULL);
    
    // Siempre incluir pan y carne
    strcpy(orden->ingredientes_solicitados[0], "pan_inferior");
    strcpy(orden->ingredientes_solicitados[1], "carne");
    strcpy(orden->ingredientes_solicitados[2], "pan_superior");
    
    // Agregar ingredientes aleatorios
    for (int i = 3; i < orden->num_ingredientes; i++) {
        int idx_ingrediente = rand() % num_ingredientes_disponibles;
        strcpy(orden->ingredientes_solicitados[i], 
               ingredientes_disponibles[idx_ingrediente]);
    }
}

void reabastecer_ingredientes() {
    printf("\n[SISTEMA] Reabasteciendo ingredientes...\n");
    for (int i = 0; i < datos_compartidos->num_ingredientes; i++) {
        pthread_mutex_lock(&datos_compartidos->ingredientes[i].mutex);
        datos_compartidos->ingredientes[i].cantidad = CAPACIDAD_DISPENSADOR;
        pthread_mutex_unlock(&datos_compartidos->ingredientes[i].mutex);
    }
    printf("[SISTEMA] Reabastecimiento completado\n");
}

void limpiar_sistema() {
    datos_compartidos->sistema_activo = 0;
    
    // Despertar todos los hilos bloqueados
    for (int i = 0; i < datos_compartidos->num_bandas; i++) {
        pthread_cond_broadcast(&datos_compartidos->bandas[i].condicion);
    }
    pthread_cond_broadcast(&datos_compartidos->cola_espera.no_vacia);
    pthread_cond_broadcast(&datos_compartidos->nueva_orden);
    
    // Esperar que terminen los hilos de las bandas
    for (int i = 0; i < datos_compartidos->num_bandas; i++) {
        pthread_join(datos_compartidos->bandas[i].hilo, NULL);
    }
    
    // Limpiar memoria compartida
    shm_unlink("/burger_system");
    printf("\nSistema terminado correctamente\n");
}

void manejar_senal(int sig) {
    switch (sig) {
        case SIGINT:
        case SIGTERM:
            printf("\nRecibida señal de terminación...\n");
            limpiar_sistema();
            exit(0);
            break;
        case SIGUSR1:
            // Pausar banda aleatoria
            if (datos_compartidos->num_bandas > 0) {
                int banda = rand() % datos_compartidos->num_bandas;
                datos_compartidos->bandas[banda].pausada = 1;
                printf("\n[CONTROL] Banda %d pausada\n", banda);
            }
            break;
        case SIGUSR2:
            // Reanudar todas las bandas pausadas
            for (int i = 0; i < datos_compartidos->num_bandas; i++) {
                if (datos_compartidos->bandas[i].pausada) {
                    datos_compartidos->bandas[i].pausada = 0;
                    pthread_cond_signal(&datos_compartidos->bandas[i].condicion);
                    printf("[CONTROL] Banda %d reanudada\n", i);
                }
            }
            break;
        case SIGCONT:
            reabastecer_ingredientes();
            break;
    }
}

int validar_parametros(int argc, char* argv[], int* num_bandas) {
    *num_bandas = 3; // Valor por defecto
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            mostrar_ayuda();
            return 0;
        } else if (strcmp(argv[i], "-n") == 0 || strcmp(argv[i], "--bandas") == 0) {
            if (i + 1 < argc) {
                *num_bandas = atoi(argv[i + 1]);
                if (*num_bandas <= 0 || *num_bandas > MAX_BANDAS) {
                    printf("Error: Número de bandas debe estar entre 1 y %d\n", MAX_BANDAS);
                    return 0;
                }
                i++; // Saltar el siguiente argumento
            } else {
                printf("Error: -n requiere un número\n");
                return 0;
            }
        } else if (strcmp(argv[i], "-i") == 0 || strcmp(argv[i], "--ingredientes") == 0) {
            printf("Ingredientes disponibles:\n");
            for (int j = 0; j < num_ingredientes_disponibles; j++) {
                printf("  - %s\n", ingredientes_disponibles[j]);
            }
            return 0;
        } else {
            printf("Parámetro desconocido: %s\n", argv[i]);
            mostrar_ayuda();
            return 0;
        }
    }
    
    return 1;
}

void mostrar_ayuda() {
    printf("Sistema de Preparación Automatizada de Hamburguesas\n");
    printf("Uso: %s [opciones]\n\n", "burger_system");
    printf("Opciones:\n");
    printf("  -n, --bandas <N>     Número de bandas de preparación (1-%d, default: 3)\n", MAX_BANDAS);
    printf("  -i, --ingredientes   Mostrar ingredientes disponibles\n");
    printf("  -h, --help          Mostrar esta ayuda\n\n");
    printf("Controles durante ejecución:\n");
    printf("  Ctrl+C              Terminar sistema\n");
    printf("  kill -USR1 <pid>    Pausar banda aleatoria\n");
    printf("  kill -USR2 <pid>    Reanudar bandas pausadas\n");
    printf("  kill -CONT <pid>    Reabastecer ingredientes\n");
}

int main(int argc, char* argv[]) {
    int num_bandas;
    
    // Validar parámetros de entrada
    if (!validar_parametros(argc, argv, &num_bandas)) {
        return 0;
    }
    
    // Configurar manejo de señales
    signal(SIGINT, manejar_senal);
    signal(SIGTERM, manejar_senal);
    signal(SIGUSR1, manejar_senal);
    signal(SIGUSR2, manejar_senal);
    signal(SIGCONT, manejar_senal);
    
    // Inicializar generador de números aleatorios
    srand(time(NULL));
    
    // Inicializar el sistema
    inicializar_sistema(num_bandas);
    
    // Crear hilos para las bandas de preparación
    int banda_ids[MAX_BANDAS];
    for (int i = 0; i < num_bandas; i++) {
        banda_ids[i] = i;
        if (pthread_create(&datos_compartidos->bandas[i].hilo, NULL, banda_worker, &banda_ids[i]) != 0) {
            perror("Error creando hilo de banda");
            exit(1);
        }
    }
    
    // Crear hilos adicionales del sistema
    pthread_create(&hilo_interfaz_estado, NULL, interfaz_estado, NULL);
    pthread_create(&hilo_generador_ordenes, NULL, generador_ordenes, NULL);
    
    printf("Sistema iniciado exitosamente con %d bandas\n", num_bandas);
    printf("PID del proceso: %d\n", getpid());
    
    // Hilo principal maneja la interfaz de estado
    while (datos_compartidos->sistema_activo) {
        sleep(1);
    }
    
    // Limpiar recursos
    pthread_join(hilo_interfaz_estado, NULL);
    pthread_join(hilo_generador_ordenes, NULL);
    
    return 0;
}