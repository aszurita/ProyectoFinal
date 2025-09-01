#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <time.h>
#include <signal.h>

#define MAX_BANDAS 10
#define MAX_INGREDIENTES 15
#define MAX_ORDENES 100
#define MAX_NOMBRE_INGREDIENTE 30
#define MAX_LOGS_POR_BANDA 10
#define CAPACIDAD_DISPENSADOR 20
#define NUM_TIPOS_HAMBURGUESA 6

// Estructura para ingredientes por banda
typedef struct
{
    char nombre[MAX_NOMBRE_INGREDIENTE];
    int cantidad;
    pthread_mutex_t mutex;
} Ingrediente;

// Definición de tipos de hamburguesas
typedef struct
{
    char nombre[50];
    char ingredientes[10][MAX_NOMBRE_INGREDIENTE];
    int num_ingredientes;
    float precio;
} TipoHamburguesa;

// Estructura para logs de cada banda
typedef struct
{
    char mensaje[100];
    time_t timestamp;
} LogEntry;

// Estructura para una orden específica
typedef struct
{
    int id_orden;
    int tipo_hamburguesa;
    char nombre_hamburguesa[50];
    char ingredientes_solicitados[MAX_INGREDIENTES][MAX_NOMBRE_INGREDIENTE];
    int num_ingredientes;
    time_t tiempo_creacion;
    int paso_actual;
    int completada;
    int asignada_a_banda; // Nueva: para tracking
} Orden;

// Estructura para una banda de preparación
typedef struct
{
    int id;
    int activa;
    int pausada;
    int hamburguesas_procesadas;
    int procesando_orden;
    Orden orden_actual;
    Ingrediente dispensadores[MAX_INGREDIENTES];
    LogEntry logs[MAX_LOGS_POR_BANDA];
    int num_logs;
    pthread_t hilo;
    pthread_mutex_t mutex;
    pthread_cond_t condicion;
    char estado_actual[100];
    char ingrediente_actual[50];
} Banda;

// Cola FIFO para órdenes en espera
typedef struct
{
    Orden ordenes[MAX_ORDENES];
    int frente;
    int atras;
    int tamano;
    pthread_mutex_t mutex;
    pthread_cond_t no_vacia;
    pthread_cond_t no_llena;
} ColaFIFO;

// Estructura de datos compartidos
typedef struct
{
    Banda bandas[MAX_BANDAS];
    ColaFIFO cola_espera;
    int num_bandas;
    int sistema_activo;
    int total_ordenes_procesadas;
    pthread_mutex_t mutex_global;
    pthread_cond_t nueva_orden;
} DatosCompartidos;

// Variables globales
DatosCompartidos *datos_compartidos;
pthread_t hilo_generador_ordenes;
pthread_t hilo_asignador_ordenes; // Nuevo hilo para asignación inteligente

// Menú de hamburguesas disponibles
TipoHamburguesa menu_hamburguesas[NUM_TIPOS_HAMBURGUESA] = {
    {"Clasica", {"pan_inferior", "carne", "lechuga", "tomate", "pan_superior"}, 5, 8.50},
    {"Cheeseburger", {"pan_inferior", "carne", "queso", "lechuga", "tomate", "pan_superior"}, 6, 9.25},
    {"BBQ Bacon", {"pan_inferior", "carne", "bacon", "queso", "cebolla", "salsa_bbq", "pan_superior"}, 7, 11.75},
    {"Vegetariana", {"pan_inferior", "vegetal", "lechuga", "tomate", "aguacate", "mayonesa", "pan_superior"}, 7, 10.25},
    {"Deluxe", {"pan_inferior", "carne", "queso", "bacon", "lechuga", "tomate", "cebolla", "mayonesa", "pan_superior"}, 9, 13.50},
    {"Spicy Mexican", {"pan_inferior", "carne", "queso", "jalapenos", "tomate", "cebolla", "salsa_picante", "pan_superior"}, 8, 12.00}};

// Ingredientes base disponibles
char ingredientes_base[MAX_INGREDIENTES][MAX_NOMBRE_INGREDIENTE] = {
    "pan_inferior", "pan_superior", "carne", "queso", "tomate",
    "lechuga", "cebolla", "bacon", "mayonesa", "jalapenos",
    "aguacate", "vegetal", "salsa_bbq", "salsa_picante", "pepinillos"};

// Prototipos de funciones
void inicializar_sistema(int num_bandas);
void mostrar_menu_hamburguesas();
void *banda_worker(void *arg);
void *generador_ordenes(void *arg);
void *asignador_ordenes(void *arg); // Nuevo
void procesar_orden(int banda_id, Orden *orden);
int verificar_ingredientes_banda(int banda_id, Orden *orden);
void consumir_ingredientes_banda(int banda_id, Orden *orden);
int encontrar_banda_disponible(Orden *orden); // Nuevo
void agregar_log_banda(int banda_id, const char *mensaje);
void encolar_orden(Orden *orden);
Orden *desencolar_orden();
void mostrar_estado_columnar();
void generar_orden_especifica(Orden *orden, int id);
void reabastecer_banda(int banda_id);
void limpiar_sistema();
void manejar_senal(int sig);
int validar_parametros(int argc, char *argv[], int *num_bandas);
void mostrar_ayuda();

// ═══════════════════════════════════════════════════════════════
// FUNCIONES DE INICIALIZACIÓN
// ═══════════════════════════════════════════════════════════════

void inicializar_sistema(int num_bandas)
{
    // Limpiar memoria compartida previa
    shm_unlink("/burger_system");

    // Crear memoria compartida
    int shm_fd = shm_open("/burger_system", O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1)
    {
        perror("Error creando memoria compartida");
        exit(1);
    }

    ftruncate(shm_fd, sizeof(DatosCompartidos));
    datos_compartidos = mmap(0, sizeof(DatosCompartidos), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);

    if (datos_compartidos == MAP_FAILED)
    {
        perror("Error mapeando memoria compartida");
        exit(1);
    }

    // Inicializar datos compartidos
    memset(datos_compartidos, 0, sizeof(DatosCompartidos));
    datos_compartidos->num_bandas = num_bandas;
    datos_compartidos->sistema_activo = 1;
    datos_compartidos->total_ordenes_procesadas = 0;

    // Inicializar mutex y condiciones globales
    pthread_mutex_init(&datos_compartidos->mutex_global, NULL);
    pthread_cond_init(&datos_compartidos->nueva_orden, NULL);

    // Inicializar bandas con sus dispensadores individuales
    for (int i = 0; i < num_bandas; i++)
    {
        datos_compartidos->bandas[i].id = i;
        datos_compartidos->bandas[i].activa = 1;
        datos_compartidos->bandas[i].pausada = 0;
        datos_compartidos->bandas[i].hamburguesas_procesadas = 0;
        datos_compartidos->bandas[i].procesando_orden = 0;
        datos_compartidos->bandas[i].num_logs = 0;
        strcpy(datos_compartidos->bandas[i].estado_actual, "ESPERANDO");
        strcpy(datos_compartidos->bandas[i].ingrediente_actual, "");
        pthread_mutex_init(&datos_compartidos->bandas[i].mutex, NULL);
        pthread_cond_init(&datos_compartidos->bandas[i].condicion, NULL);

        // Inicializar dispensadores para cada banda
        for (int j = 0; j < MAX_INGREDIENTES; j++)
        {
            strcpy(datos_compartidos->bandas[i].dispensadores[j].nombre, ingredientes_base[j]);
            datos_compartidos->bandas[i].dispensadores[j].cantidad = CAPACIDAD_DISPENSADOR;
            pthread_mutex_init(&datos_compartidos->bandas[i].dispensadores[j].mutex, NULL);
        }

        // Log inicial
        agregar_log_banda(i, "BANDA INICIADA");
    }

    // Inicializar cola FIFO
    datos_compartidos->cola_espera.frente = 0;
    datos_compartidos->cola_espera.atras = 0;
    datos_compartidos->cola_espera.tamano = 0;
    pthread_mutex_init(&datos_compartidos->cola_espera.mutex, NULL);
    pthread_cond_init(&datos_compartidos->cola_espera.no_vacia, NULL);
    pthread_cond_init(&datos_compartidos->cola_espera.no_llena, NULL);

    printf("Sistema inicializado con %d bandas de preparación\n", num_bandas);
    mostrar_menu_hamburguesas();
}

void mostrar_menu_hamburguesas()
{
    printf("\n╔══════════════════════════════════════════════════════════════╗\n");
    printf("║                    MENÚ DE HAMBURGUESAS                      ║\n");
    printf("╠══════════════════════════════════════════════════════════════╣\n");

    for (int i = 0; i < NUM_TIPOS_HAMBURGUESA; i++)
    {
        printf("║ %d. %-20s - $%.2f                        ║\n",
               i + 1, menu_hamburguesas[i].nombre, menu_hamburguesas[i].precio);
    }
    printf("╚══════════════════════════════════════════════════════════════╝\n");
}

// ═══════════════════════════════════════════════════════════════
// FUNCIONES DE MANEJO DE LOGS
// ═══════════════════════════════════════════════════════════════

void agregar_log_banda(int banda_id, const char *mensaje)
{
    if (banda_id < 0 || banda_id >= datos_compartidos->num_bandas)
        return;

    Banda *banda = &datos_compartidos->bandas[banda_id];

    pthread_mutex_lock(&banda->mutex);

    // Si ya tenemos el máximo de logs, desplazar hacia arriba
    if (banda->num_logs >= MAX_LOGS_POR_BANDA)
    {
        for (int i = 1; i < MAX_LOGS_POR_BANDA; i++)
        {
            banda->logs[i - 1] = banda->logs[i];
        }
        banda->num_logs = MAX_LOGS_POR_BANDA - 1;
    }

    // Agregar nuevo log
    strcpy(banda->logs[banda->num_logs].mensaje, mensaje);
    banda->logs[banda->num_logs].timestamp = time(NULL);
    banda->num_logs++;

    pthread_mutex_unlock(&banda->mutex);
}

// ═══════════════════════════════════════════════════════════════
// FUNCIONES DE HILOS DE TRABAJO
// ═══════════════════════════════════════════════════════════════

void *banda_worker(void *arg)
{
    int banda_id = *(int *)arg;
    Banda *banda = &datos_compartidos->bandas[banda_id];

    while (datos_compartidos->sistema_activo)
    {
        pthread_mutex_lock(&banda->mutex);

        // Esperar mientras esté pausada
        while (banda->pausada && datos_compartidos->sistema_activo)
        {
            strcpy(banda->estado_actual, "PAUSADA");
            pthread_cond_wait(&banda->condicion, &banda->mutex);
        }

        if (!datos_compartidos->sistema_activo)
        {
            pthread_mutex_unlock(&banda->mutex);
            break;
        }

        // Si no está procesando nada, quedarse esperando
        if (!banda->procesando_orden)
        {
            strcpy(banda->estado_actual, "ESPERANDO");
            pthread_mutex_unlock(&banda->mutex);
            usleep(100000); // 100ms
            continue;
        }

        pthread_mutex_unlock(&banda->mutex);

        // Si llegamos aquí, tenemos una orden asignada para procesar
        procesar_orden(banda_id, &banda->orden_actual);

        pthread_mutex_lock(&banda->mutex);
        banda->hamburguesas_procesadas++;
        banda->procesando_orden = 0;
        strcpy(banda->estado_actual, "ESPERANDO");
        strcpy(banda->ingrediente_actual, "");
        pthread_mutex_unlock(&banda->mutex);

        pthread_mutex_lock(&datos_compartidos->mutex_global);
        datos_compartidos->total_ordenes_procesadas++;
        pthread_mutex_unlock(&datos_compartidos->mutex_global);

        char log_msg[100];
        sprintf(log_msg, "COMPLETADA %s #%d", banda->orden_actual.nombre_hamburguesa, banda->orden_actual.id_orden);
        agregar_log_banda(banda_id, log_msg);
    }

    return NULL;
}

void *generador_ordenes(void *arg)
{
    (void)arg;
    int contador_ordenes = 1;

    while (datos_compartidos->sistema_activo)
    {
        Orden nueva_orden;
        generar_orden_especifica(&nueva_orden, contador_ordenes++);
        encolar_orden(&nueva_orden);

        printf("\n[NUEVA ORDEN] %s #%d generada - En cola\n",
               nueva_orden.nombre_hamburguesa, nueva_orden.id_orden);

        pthread_mutex_lock(&datos_compartidos->mutex_global);
        pthread_cond_broadcast(&datos_compartidos->nueva_orden);
        pthread_mutex_unlock(&datos_compartidos->mutex_global);

        // Generar nueva orden cada 30 segundos
        sleep(30);

        // Ocasionalmente reabastecer una banda aleatoria
        if (rand() % 5 == 0)
        {
            int banda_aleatoria = rand() % datos_compartidos->num_bandas;
            reabastecer_banda(banda_aleatoria);
        }
    }
    return NULL;
}

// NUEVO: Hilo asignador inteligente de órdenes
void *asignador_ordenes(void *arg)
{
    (void)arg;

    while (datos_compartidos->sistema_activo)
    {
        // Intentar asignar órdenes pendientes a bandas disponibles
        Orden *orden = desencolar_orden();
        if (orden != NULL)
        {
            int banda_asignada = encontrar_banda_disponible(orden);

            if (banda_asignada >= 0)
            {
                // Asignar orden a la banda encontrada
                Banda *banda = &datos_compartidos->bandas[banda_asignada];

                pthread_mutex_lock(&banda->mutex);
                banda->procesando_orden = 1;
                banda->orden_actual = *orden;
                banda->orden_actual.asignada_a_banda = banda_asignada;
                sprintf(banda->estado_actual, "ASIGNADA %s", orden->nombre_hamburguesa);
                pthread_mutex_unlock(&banda->mutex);

                char log_msg[100];
                sprintf(log_msg, "ASIGNADA %s #%d", orden->nombre_hamburguesa, orden->id_orden);
                agregar_log_banda(banda_asignada, log_msg);
            }
            else
            {
                // No hay bandas disponibles con recursos, re-encolar
                encolar_orden(orden);
                sleep(2); // Esperar un poco antes de intentar de nuevo
            }
        }
        else
        {
            // No hay órdenes, esperar
            usleep(200000); // 200ms
        }
    }

    return NULL;
}

// NUEVO: Encuentra una banda disponible que tenga los recursos necesarios
int encontrar_banda_disponible(Orden *orden)
{
    // Buscar banda libre con recursos suficientes
    for (int i = 0; i < datos_compartidos->num_bandas; i++)
    {
        Banda *banda = &datos_compartidos->bandas[i];

        pthread_mutex_lock(&banda->mutex);
        int banda_libre = banda->activa && !banda->pausada && !banda->procesando_orden;
        pthread_mutex_unlock(&banda->mutex);

        if (banda_libre && verificar_ingredientes_banda(i, orden))
        {
            return i; // Banda encontrada
        }
    }

    return -1; // No se encontró banda disponible
}

// ═══════════════════════════════════════════════════════════════
// FUNCIONES DE PROCESAMIENTO DE ÓRDENES
// ═══════════════════════════════════════════════════════════════

void procesar_orden(int banda_id, Orden *orden)
{
    Banda *banda = &datos_compartidos->bandas[banda_id];

    char log_msg[100];
    sprintf(log_msg, "INICIANDO %s #%d", orden->nombre_hamburguesa, orden->id_orden);
    agregar_log_banda(banda_id, log_msg);

    // Consumir ingredientes de los dispensadores de esta banda
    consumir_ingredientes_banda(banda_id, orden);

    // Simular preparación paso a paso (5 segundos por ingrediente)
    for (int i = 0; i < orden->num_ingredientes; i++)
    {
        pthread_mutex_lock(&banda->mutex);
        orden->paso_actual = i + 1;
        strcpy(banda->ingrediente_actual, orden->ingredientes_solicitados[i]);
        sprintf(banda->estado_actual, "AGREGANDO %s (%d/%d)",
                orden->ingredientes_solicitados[i], i + 1, orden->num_ingredientes);
        pthread_mutex_unlock(&banda->mutex);

        sprintf(log_msg, "Agregando %s...", orden->ingredientes_solicitados[i]);
        agregar_log_banda(banda_id, log_msg);

        // 5 segundos por ingrediente
        sleep(5);
    }

    pthread_mutex_lock(&banda->mutex);
    sprintf(banda->estado_actual, "FINALIZANDO %s", orden->nombre_hamburguesa);
    pthread_mutex_unlock(&banda->mutex);

    agregar_log_banda(banda_id, "HAMBURGUESA LISTA!");
    sleep(2); // Tiempo final de empaque
}

int verificar_ingredientes_banda(int banda_id, Orden *orden)
{
    Banda *banda = &datos_compartidos->bandas[banda_id];

    for (int i = 0; i < orden->num_ingredientes; i++)
    {
        int encontrado = 0;
        for (int j = 0; j < MAX_INGREDIENTES; j++)
        {
            if (strcmp(orden->ingredientes_solicitados[i],
                       banda->dispensadores[j].nombre) == 0)
            {
                pthread_mutex_lock(&banda->dispensadores[j].mutex);
                if (banda->dispensadores[j].cantidad <= 0)
                {
                    pthread_mutex_unlock(&banda->dispensadores[j].mutex);
                    return 0;
                }
                pthread_mutex_unlock(&banda->dispensadores[j].mutex);
                encontrado = 1;
                break;
            }
        }
        if (!encontrado)
            return 0;
    }
    return 1;
}

void consumir_ingredientes_banda(int banda_id, Orden *orden)
{
    Banda *banda = &datos_compartidos->bandas[banda_id];

    for (int i = 0; i < orden->num_ingredientes; i++)
    {
        for (int j = 0; j < MAX_INGREDIENTES; j++)
        {
            if (strcmp(orden->ingredientes_solicitados[i],
                       banda->dispensadores[j].nombre) == 0)
            {
                pthread_mutex_lock(&banda->dispensadores[j].mutex);
                if (banda->dispensadores[j].cantidad > 0)
                {
                    banda->dispensadores[j].cantidad--;
                }
                pthread_mutex_unlock(&banda->dispensadores[j].mutex);
                break;
            }
        }
    }
}

// ═══════════════════════════════════════════════════════════════
// FUNCIONES DE COLA FIFO
// ═══════════════════════════════════════════════════════════════

void encolar_orden(Orden *orden)
{
    pthread_mutex_lock(&datos_compartidos->cola_espera.mutex);

    while (datos_compartidos->cola_espera.tamano >= MAX_ORDENES)
    {
        pthread_cond_wait(&datos_compartidos->cola_espera.no_llena,
                          &datos_compartidos->cola_espera.mutex);
    }

    datos_compartidos->cola_espera.ordenes[datos_compartidos->cola_espera.atras] = *orden;
    datos_compartidos->cola_espera.atras = (datos_compartidos->cola_espera.atras + 1) % MAX_ORDENES;
    datos_compartidos->cola_espera.tamano++;

    pthread_cond_signal(&datos_compartidos->cola_espera.no_vacia);
    pthread_mutex_unlock(&datos_compartidos->cola_espera.mutex);
}

Orden *desencolar_orden()
{
    static Orden orden_temp;

    pthread_mutex_lock(&datos_compartidos->cola_espera.mutex);

    if (datos_compartidos->cola_espera.tamano == 0)
    {
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

// ═══════════════════════════════════════════════════════════════
// FUNCIÓN DE DISPLAY COLUMNAR MEJORADO
// ═══════════════════════════════════════════════════════════════

void mostrar_estado_columnar()
{
    system("clear");

    // Encabezado general del sistema
    printf("╔═══════════════════════════════════════════════════════════════════════════════════════════════╗\n");
    printf("║                              SISTEMA DE HAMBURGUESAS - ESTADO                                ║\n");
    printf("║ Total órdenes procesadas: %-6d  |  Órdenes en cola: %-6d  |  Bandas activas: %-6d      ║\n",
           datos_compartidos->total_ordenes_procesadas, datos_compartidos->cola_espera.tamano, datos_compartidos->num_bandas);
    printf("║ Nueva orden cada 30s | Ingrediente cada 5s | Asignación inteligente de bandas              ║\n");
    printf("╚═══════════════════════════════════════════════════════════════════════════════════════════════╝\n\n");

    // Mostrar bandas dinámicamente (máximo 3 por fila)
    int bandas_por_fila = (datos_compartidos->num_bandas > 3) ? 3 : datos_compartidos->num_bandas;
    int filas_bandas = (datos_compartidos->num_bandas + 2) / 3; // Redondear hacia arriba

    for (int fila = 0; fila < filas_bandas; fila++)
    {
        int banda_inicio = fila * 3;
        int banda_fin = (banda_inicio + 3 > datos_compartidos->num_bandas) ? datos_compartidos->num_bandas : banda_inicio + 3;

        // Crear las columnas para esta fila de bandas
        for (int banda = banda_inicio; banda < banda_fin; banda++)
        {
            printf("┌─────────────────────────────────┐");
            if (banda < banda_fin - 1)
                printf("  ");
        }
        printf("\n");

        // Encabezados de bandas
        for (int banda = banda_inicio; banda < banda_fin; banda++)
        {
            printf("│           BANDA %-2d               │", banda + 1);
            if (banda < banda_fin - 1)
                printf("  ");
        }
        printf("\n");

        for (int banda = banda_inicio; banda < banda_fin; banda++)
        {
            printf("├─────────────────────────────────┤");
            if (banda < banda_fin - 1)
                printf("  ");
        }
        printf("\n");

        // SECCIÓN: Inventario
        for (int banda = banda_inicio; banda < banda_fin; banda++)
        {
            printf("│         INVENTARIO              │");
            if (banda < banda_fin - 1)
                printf("  ");
        }
        printf("\n");

        for (int banda = banda_inicio; banda < banda_fin; banda++)
        {
            printf("├─────────────────────────────────┤");
            if (banda < banda_fin - 1)
                printf("  ");
        }
        printf("\n");

        // Mostrar inventario de cada banda (primeros 8 ingredientes)
        for (int ing = 0; ing < 8; ing++)
        {
            for (int banda = banda_inicio; banda < banda_fin; banda++)
            {
                Banda *b = &datos_compartidos->bandas[banda];
                pthread_mutex_lock(&b->dispensadores[ing].mutex);

                char nombre_corto[15];
                strncpy(nombre_corto, b->dispensadores[ing].nombre, 14);
                nombre_corto[14] = '\0';

                int cantidad = b->dispensadores[ing].cantidad;
                char alerta[8] = "";
                if (cantidad <= 2)
                    strcpy(alerta, " [BAJO]");
                if (cantidad == 0)
                    strcpy(alerta, " [VACIO]");

                printf("│%-14s: %2d%s%*s│", nombre_corto, cantidad, alerta,
                       12 - (int)strlen(alerta), "");
                pthread_mutex_unlock(&b->dispensadores[ing].mutex);

                if (banda < banda_fin - 1)
                    printf("  ");
            }
            printf("\n");
        }

        // Separador para preparación
        for (int banda = banda_inicio; banda < banda_fin; banda++)
        {
            printf("├─────────────────────────────────┤");
            if (banda < banda_fin - 1)
                printf("  ");
        }
        printf("\n");

        // SECCIÓN: Preparación
        for (int banda = banda_inicio; banda < banda_fin; banda++)
        {
            printf("│         PREPARACIÓN             │");
            if (banda < banda_fin - 1)
                printf("  ");
        }
        printf("\n");

        for (int banda = banda_inicio; banda < banda_fin; banda++)
        {
            printf("├─────────────────────────────────┤");
            if (banda < banda_fin - 1)
                printf("  ");
        }
        printf("\n");

        // Mostrar estado de preparación
        for (int linea = 0; linea < 6; linea++)
        {
            for (int banda = banda_inicio; banda < banda_fin; banda++)
            {
                Banda *b = &datos_compartidos->bandas[banda];
                pthread_mutex_lock(&b->mutex);

                switch (linea)
                {
                case 0:
                    if (b->procesando_orden)
                    {
                        printf("│Orden %-3d: %-16s│", b->orden_actual.id_orden,
                               b->orden_actual.nombre_hamburguesa);
                    }
                    else
                    {
                        printf("│%-31s│", "Sin orden activa");
                    }
                    break;
                case 1:
                    printf("│Estado: %-20s│", b->estado_actual);
                    break;
                case 2:
                    if (b->procesando_orden && strlen(b->ingrediente_actual) > 0)
                    {
                        printf("│Ingrediente: %-16s│", b->ingrediente_actual);
                    }
                    else
                    {
                        printf("│%-31s│", "");
                    }
                    break;
                case 3:
                    if (b->procesando_orden)
                    {
                        printf("│Progreso: %d/%d pasos         │",
                               b->orden_actual.paso_actual, b->orden_actual.num_ingredientes);
                    }
                    else
                    {
                        printf("│%-31s│", "");
                    }
                    break;
                case 4:
                    printf("│Procesadas: %-16d│", b->hamburguesas_procesadas);
                    break;
                case 5:
                    printf("│%-31s│", "");
                    break;
                }

                pthread_mutex_unlock(&b->mutex);
                if (banda < banda_fin - 1)
                    printf("  ");
            }
            printf("\n");
        }

        // Separador para logs
        for (int banda = banda_inicio; banda < banda_fin; banda++)
        {
            printf("├─────────────────────────────────┤");
            if (banda < banda_fin - 1)
                printf("  ");
        }
        printf("\n");

        // SECCIÓN: Logs
        for (int banda = banda_inicio; banda < banda_fin; banda++)
        {
            printf("│             LOGS                │");
            if (banda < banda_fin - 1)
                printf("  ");
        }
        printf("\n");

        for (int banda = banda_inicio; banda < banda_fin; banda++)
        {
            printf("├─────────────────────────────────┤");
            if (banda < banda_fin - 1)
                printf("  ");
        }
        printf("\n");

        // Mostrar últimos 8 logs de cada banda
        for (int log_line = 0; log_line < 8; log_line++)
        {
            for (int banda = banda_inicio; banda < banda_fin; banda++)
            {
                Banda *b = &datos_compartidos->bandas[banda];
                pthread_mutex_lock(&b->mutex);

                if (log_line < b->num_logs)
                {
                    // Mostrar desde el más reciente hacia atrás
                    int log_idx = b->num_logs - 1 - log_line;
                    if (log_idx >= 0)
                    {
                        char log_corto[32];
                        strncpy(log_corto, b->logs[log_idx].mensaje, 31);
                        log_corto[31] = '\0';
                        printf("│%-31s│", log_corto);
                    }
                    else
                    {
                        printf("│%-31s│", "");
                    }
                }
                else
                {
                    printf("│%-31s│", "");
                }

                pthread_mutex_unlock(&b->mutex);
                if (banda < banda_fin - 1)
                    printf("  ");
            }
            printf("\n");
        }

        // Cerrar columnas
        for (int banda = banda_inicio; banda < banda_fin; banda++)
        {
            printf("└─────────────────────────────────┘");
            if (banda < banda_fin - 1)
                printf("  ");
        }
        printf("\n\n");
    }

    printf("Presiona Ctrl+C para salir del sistema\n");
}

void generar_orden_especifica(Orden *orden, int id)
{
    // Seleccionar tipo de hamburguesa aleatoriamente
    int tipo = rand() % NUM_TIPOS_HAMBURGUESA;
    TipoHamburguesa *hamburguesa = &menu_hamburguesas[tipo];

    orden->id_orden = id;
    orden->tipo_hamburguesa = tipo;
    strcpy(orden->nombre_hamburguesa, hamburguesa->nombre);
    orden->num_ingredientes = hamburguesa->num_ingredientes;
    orden->tiempo_creacion = time(NULL);
    orden->paso_actual = 0;
    orden->completada = 0;
    orden->asignada_a_banda = -1;

    // Copiar ingredientes específicos de esta hamburguesa
    for (int i = 0; i < hamburguesa->num_ingredientes; i++)
    {
        strcpy(orden->ingredientes_solicitados[i], hamburguesa->ingredientes[i]);
    }
}

void reabastecer_banda(int banda_id)
{
    if (banda_id >= 0 && banda_id < datos_compartidos->num_bandas)
    {
        for (int i = 0; i < MAX_INGREDIENTES; i++)
        {
            pthread_mutex_lock(&datos_compartidos->bandas[banda_id].dispensadores[i].mutex);
            datos_compartidos->bandas[banda_id].dispensadores[i].cantidad = CAPACIDAD_DISPENSADOR;
            pthread_mutex_unlock(&datos_compartidos->bandas[banda_id].dispensadores[i].mutex);
        }

        agregar_log_banda(banda_id, "BANDA REABASTECIDA");
    }
}

void limpiar_sistema()
{
    datos_compartidos->sistema_activo = 0;

    // Despertar todos los hilos bloqueados
    for (int i = 0; i < datos_compartidos->num_bandas; i++)
    {
        pthread_cond_broadcast(&datos_compartidos->bandas[i].condicion);
    }
    pthread_cond_broadcast(&datos_compartidos->cola_espera.no_vacia);
    pthread_cond_broadcast(&datos_compartidos->nueva_orden);

    // Esperar que terminen los hilos de las bandas
    for (int i = 0; i < datos_compartidos->num_bandas; i++)
    {
        pthread_join(datos_compartidos->bandas[i].hilo, NULL);
    }

    // Esperar hilos del sistema
    pthread_join(hilo_generador_ordenes, NULL);
    pthread_join(hilo_asignador_ordenes, NULL);

    // Limpiar memoria compartida
    shm_unlink("/burger_system");
    printf("\nSistema terminado correctamente\n");
}

void manejar_senal(int sig)
{
    switch (sig)
    {
    case SIGINT:
    case SIGTERM:
        printf("\nRecibida señal de terminación...\n");
        limpiar_sistema();
        exit(0);
        break;
    case SIGUSR1:
        // Pausar banda aleatoria
        if (datos_compartidos->num_bandas > 0)
        {
            int banda = rand() % datos_compartidos->num_bandas;
            datos_compartidos->bandas[banda].pausada = 1;
            agregar_log_banda(banda, "BANDA PAUSADA POR SEÑAL");
        }
        break;
    case SIGUSR2:
        // Reanudar todas las bandas pausadas
        for (int i = 0; i < datos_compartidos->num_bandas; i++)
        {
            if (datos_compartidos->bandas[i].pausada)
            {
                datos_compartidos->bandas[i].pausada = 0;
                pthread_cond_signal(&datos_compartidos->bandas[i].condicion);
                agregar_log_banda(i, "BANDA REANUDADA");
            }
        }
        break;
    case SIGCONT:
        // Reabastecer banda aleatoria
        {
            int banda = rand() % datos_compartidos->num_bandas;
            reabastecer_banda(banda);
        }
        break;
    }
}

int validar_parametros(int argc, char *argv[], int *num_bandas)
{
    *num_bandas = 3; // Valor por defecto

    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0)
        {
            mostrar_ayuda();
            return 0;
        }
        else if (strcmp(argv[i], "-n") == 0 || strcmp(argv[i], "--bandas") == 0)
        {
            if (i + 1 < argc)
            {
                *num_bandas = atoi(argv[i + 1]);
                if (*num_bandas <= 0 || *num_bandas > MAX_BANDAS)
                {
                    printf("Error: Número de bandas debe estar entre 1 y %d\n", MAX_BANDAS);
                    return 0;
                }
                i++; // Saltar el siguiente argumento
            }
            else
            {
                printf("Error: -n requiere un número\n");
                return 0;
            }
        }
        else if (strcmp(argv[i], "-m") == 0 || strcmp(argv[i], "--menu") == 0)
        {
            mostrar_menu_hamburguesas();
            return 0;
        }
        else
        {
            printf("Parámetro desconocido: %s\n", argv[i]);
            mostrar_ayuda();
            return 0;
        }
    }

    return 1;
}

void mostrar_ayuda()
{
    printf("Sistema de Preparación Automatizada de Hamburguesas v4.0 - Asignación Inteligente\n");
    printf("Uso: ./burger_system [opciones]\n\n");
    printf("Opciones:\n");
    printf("  -n, --bandas <N>     Número de bandas de preparación (1-%d, default: 3)\n", MAX_BANDAS);
    printf("  -m, --menu          Mostrar menú de hamburguesas disponibles\n");
    printf("  -h, --help          Mostrar esta ayuda\n\n");
    printf("Características mejoradas:\n");
    printf("  • Soporte para hasta %d bandas dinámicamente\n", MAX_BANDAS);
    printf("  • Asignación inteligente - busca banda libre con recursos\n");
    printf("  • Cada banda tiene dispensadores independientes\n");
    printf("  • Generación automática cada 30 segundos\n");
    printf("  • Tiempo por ingrediente: 5 segundos\n");
    printf("  • Layout dinámico (3 bandas por fila)\n");
    printf("  • Logs detallados por banda en tiempo real\n\n");
    printf("Controles durante ejecución:\n");
    printf("  Ctrl+C              Terminar sistema\n");
    printf("  kill -USR1 <pid>    Pausar banda aleatoria\n");
    printf("  kill -USR2 <pid>    Reanudar bandas pausadas\n");
    printf("  kill -CONT <pid>    Reabastecer banda aleatoria\n");
}

int main(int argc, char *argv[])
{
    int num_bandas;

    // Validar parámetros de entrada
    if (!validar_parametros(argc, argv, &num_bandas))
    {
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
    for (int i = 0; i < num_bandas; i++)
    {
        banda_ids[i] = i;
        if (pthread_create(&datos_compartidos->bandas[i].hilo, NULL, banda_worker, &banda_ids[i]) != 0)
        {
            perror("Error creando hilo de banda");
            exit(1);
        }
    }

    // Crear hilo generador de órdenes
    pthread_create(&hilo_generador_ordenes, NULL, generador_ordenes, NULL);

    // Crear hilo asignador inteligente de órdenes
    pthread_create(&hilo_asignador_ordenes, NULL, asignador_ordenes, NULL);

    printf("Sistema iniciado exitosamente con %d bandas\n", num_bandas);
    printf("Asignación inteligente de órdenes activada\n");
    printf("Generando órdenes automáticamente cada 30 segundos\n");
    printf("Tiempo por ingrediente: 5 segundos\n");
    printf("PID del proceso: %d\n", getpid());
    printf("Presiona Ctrl+C para terminar\n\n");

    // Esperar un momento antes de comenzar la interfaz
    sleep(3);

    // Hilo principal maneja la interfaz de estado columnar
    while (datos_compartidos->sistema_activo)
    {
        mostrar_estado_columnar();
        sleep(2); // Actualizar cada 2 segundos
    }

    // Limpiar recursos al final
    limpiar_sistema();

    return 0;
}