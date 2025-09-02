/**
 * @file burger_system.c
 * @brief Sistema de Simulación de Preparación de Hamburguesas con Múltiples Bandas
 * @author Angelo Zurita
 * @date 01/09/2025
 * @version 1.0
 *
 * @section descripcion Descripción del Sistema
 *
 * Este sistema simula un restaurante de hamburguesas automatizado con múltiples
 * bandas de preparación. Cada banda puede procesar órdenes de hamburguesas
 * simultáneamente, utilizando un sistema de cola FIFO para gestionar las órdenes
 * pendientes y un sistema de inventario para controlar los ingredientes.
 *
 * @section arquitectura Arquitectura del Sistema
 *
 * El sistema está compuesto por los siguientes componentes principales:
 *
 * 1. GENERADOR DE ÓRDENES: Crea órdenes de hamburguesas automáticamente
 * 2. ASIGNADOR DE ÓRDENES: Distribuye las órdenes a las bandas disponibles
 * 3. BANDAS DE PREPARACIÓN: Procesan las órdenes paso a paso
 * 4. MONITOR DE INVENTARIO: Supervisa los niveles de ingredientes
 * 5. COLA FIFO: Gestiona las órdenes en espera
 * 6. SISTEMA DE LOGS: Registra todas las actividades del sistema
 *
 * @section algoritmos Algoritmos Principales
 *
 * - ALGORITMO DE ASIGNACIÓN INTELIGENTE: Busca la banda más adecuada basándose
 *   en disponibilidad y recursos de ingredientes
 * - GESTIÓN DE COLA FIFO: Implementa una cola circular thread-safe para
 *   manejar órdenes pendientes sin pérdidas
 * - SISTEMA DE INVENTARIO: Controla el consumo y reabastecimiento de
 *   ingredientes con alertas automáticas
 * - PROCESAMIENTO PARALELO: Utiliza hilos POSIX para operaciones concurrentes
 *
 * @section estructuras Estructuras de Datos Principales
 *
 * - DatosCompartidos: Contiene toda la información del sistema
 * - Banda: Representa una banda de preparación con su estado y recursos
 * - Orden: Define una orden de hamburguesa con sus ingredientes
 * - ColaFIFO: Implementa la cola de órdenes pendientes
 * - Ingrediente: Gestiona el inventario de cada ingrediente
 *
 * @section compilacion Compilación
 *
 * Para compilar el sistema:
 * @code
 * make burger_system
 * @endcode
 *
 * @section ejecucion Ejecución
 *
 * Uso básico:
 * @code
 * ./burger_system                    # 3 bandas, tiempos por defecto
 * ./burger_system -n 4              # 4 bandas, tiempos por defecto
 * ./burger_system -n 2 -t 3 -o 10   # 2 bandas, 3s/ingrediente, 10s entre órdenes
 * @endcode
 *
 * @section parametros Parámetros de Línea de Comandos
 *
 * - -n, --bandas <N>: Número de bandas (1-10, default: 3)
 * - -t, --tiempo-ingrediente <S>: Segundos por ingrediente (1-60, default: 2)
 * - -o, --tiempo-orden <S>: Segundos entre órdenes (1-300, default: 7)
 * - -m, --menu: Mostrar menú de hamburguesas disponibles
 * - -h, --help: Mostrar ayuda completa
 *
 * @section señales Señales del Sistema
 *
 * - SIGINT/SIGTERM: Terminación limpia del sistema
 * - SIGUSR1: Pausar una banda aleatoria
 * - SIGUSR2: Reanudar todas las bandas pausadas
 * - SIGCONT: Reabastecimiento automático de bandas críticas
 *
 * @section rendimiento Estimaciones de Rendimiento
 *
 * El sistema calcula automáticamente:
 * - Tiempo promedio por hamburguesa
 * - Órdenes generadas por minuto
 * - Capacidad teórica del sistema
 * - Advertencias de saturación
 *
 * @section dependencias Dependencias del Sistema
 *
 * - pthread: Para programación multi-hilo
 * - librt: Para memoria compartida POSIX
 * - Sistema operativo compatible con POSIX
 *
 * @section licencia Licencia
 *
 * Este software es propiedad de [Tu Institución/Organización]
 *
 * @section contacto Contacto
 *
 * Para soporte técnico o consultas: [tu-email@dominio.com]
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <time.h>
#include <signal.h>
#include <sys/ioctl.h>

/**
 * @defgroup constantes Constantes del Sistema
 * @{
 */

/** @brief Número máximo de bandas de preparación permitidas */
#define MAX_BANDAS 10

/** @brief Número máximo de ingredientes diferentes en el sistema */
#define MAX_INGREDIENTES 15

/** @brief Capacidad máxima de la cola de órdenes pendientes */
#define MAX_ORDENES 100

/** @brief Longitud máxima del nombre de un ingrediente */
#define MAX_NOMBRE_INGREDIENTE 30

/** @brief Número máximo de entradas de log por banda */
#define MAX_LOGS_POR_BANDA 10

/** @brief Capacidad máxima de cada dispensador de ingredientes */
#define CAPACIDAD_DISPENSADOR 10

/** @brief Número de tipos de hamburguesas disponibles en el menú */
#define NUM_TIPOS_HAMBURGUESA 6

/** @brief Umbral para considerar inventario bajo (menos de 3 unidades) */
#define UMBRAL_INVENTARIO_BAJO 2

/**
 * @brief Valores por defecto para tiempos de operación
 * @{
 */
/** @brief Tiempo por defecto para procesar cada ingrediente (segundos) */
#define TIEMPO_DEFAULT_INGREDIENTE 2

/** @brief Tiempo por defecto entre generación de nuevas órdenes (segundos) */
#define TIEMPO_DEFAULT_NUEVA_ORDEN 7
/** @} */
/** @} */

/**
 * @defgroup estructuras Estructuras de Datos del Sistema
 * @{
 */

/**
 * @brief Estructura que representa un ingrediente en el inventario de una banda
 *
 * Cada ingrediente tiene un nombre, cantidad disponible y un mutex para
 * garantizar acceso thread-safe al inventario.
 */
typedef struct
{
    /** @brief Nombre del ingrediente (ej: "carne", "queso", "pan_inferior") */
    char nombre[MAX_NOMBRE_INGREDIENTE];

    /** @brief Cantidad disponible en el dispensador (0 a CAPACIDAD_DISPENSADOR) */
    int cantidad;

    /** @brief Mutex para acceso exclusivo al inventario del ingrediente */
    pthread_mutex_t mutex;
} Ingrediente;

/**
 * @brief Estructura que define un tipo de hamburguesa disponible en el menú
 *
 * Contiene la receta completa de la hamburguesa, incluyendo todos los
 * ingredientes necesarios y su precio de venta.
 */
typedef struct
{
    /** @brief Nombre comercial de la hamburguesa (ej: "Clasica", "Cheeseburger") */
    char nombre[50];

    /** @brief Lista de ingredientes requeridos para preparar esta hamburguesa */
    char ingredientes[10][MAX_NOMBRE_INGREDIENTE];

    /** @brief Número total de ingredientes en la receta */
    int num_ingredientes;

    /** @brief Precio de venta de la hamburguesa en dólares */
    float precio;
} TipoHamburguesa;

/**
 * @brief Estructura para registrar eventos y actividades de cada banda
 *
 * Sistema de logging que mantiene un historial de las operaciones realizadas,
 * incluyendo timestamps y clasificación de alertas.
 */
typedef struct
{
    /** @brief Mensaje descriptivo del evento o actividad */
    char mensaje[100];

    /** @brief Timestamp Unix del momento en que ocurrió el evento */
    time_t timestamp;

    /** @brief Flag que indica si es una alerta crítica (1) o información normal (0) */
    int es_alerta;
} LogEntry;

/**
 * @brief Estructura que representa una orden de hamburguesa en el sistema
 *
 * Contiene toda la información necesaria para procesar una orden, incluyendo
 * el tipo de hamburguesa, ingredientes requeridos, estado de procesamiento
 * y metadatos de seguimiento.
 */
typedef struct
{
    /** @brief Identificador único de la orden (secuencial) */
    int id_orden;

    /** @brief Índice del tipo de hamburguesa en el menú */
    int tipo_hamburguesa;

    /** @brief Nombre de la hamburguesa solicitada */
    char nombre_hamburguesa[50];

    /** @brief Lista de ingredientes específicos requeridos para esta orden */
    char ingredientes_solicitados[MAX_INGREDIENTES][MAX_NOMBRE_INGREDIENTE];

    /** @brief Número total de ingredientes en esta orden específica */
    int num_ingredientes;

    /** @brief Timestamp de creación de la orden */
    time_t tiempo_creacion;

    /** @brief Paso actual en el proceso de preparación (0 a num_ingredientes) */
    int paso_actual;

    /** @brief Flag que indica si la orden ha sido completada */
    int completada;

    /** @brief ID de la banda asignada para procesar esta orden (-1 si no asignada) */
    int asignada_a_banda;

    /** @brief Contador de intentos de asignación a bandas */
    int intentos_asignacion;
} Orden;

/**
 * @brief Estructura que representa una banda de preparación de hamburguesas
 *
 * Cada banda es un hilo independiente que puede procesar órdenes de hamburguesas
 * simultáneamente. Incluye su propio inventario de ingredientes, sistema de logs,
 * y mecanismos de control para pausar/reanudar operaciones.
 */
typedef struct
{
    /** @brief Identificador único de la banda (0 a num_bandas-1) */
    int id;

    /** @brief Flag que indica si la banda está operativa */
    int activa;

    /** @brief Flag que indica si la banda está pausada temporalmente */
    int pausada;

    /** @brief Contador total de hamburguesas procesadas por esta banda */
    int hamburguesas_procesadas;

    /** @brief Flag que indica si la banda está procesando una orden actualmente */
    int procesando_orden;

    /** @brief Orden que está siendo procesada actualmente por esta banda */
    Orden orden_actual;

    /** @brief Array de dispensadores de ingredientes para esta banda */
    Ingrediente dispensadores[MAX_INGREDIENTES];

    /** @brief Historial de logs de actividades de esta banda */
    LogEntry logs[MAX_LOGS_POR_BANDA];

    /** @brief Número actual de entradas de log en el historial */
    int num_logs;

    /** @brief Hilo POSIX que ejecuta la lógica de la banda */
    pthread_t hilo;

    /** @brief Mutex para acceso exclusivo a los datos de la banda */
    pthread_mutex_t mutex;

    /** @brief Variable de condición para sincronización de pausa/reanudación */
    pthread_cond_t condicion;

    /** @brief Descripción del estado actual de la banda */
    char estado_actual[100];

    /** @brief Nombre del ingrediente que se está procesando actualmente */
    char ingrediente_actual[50];

    /** @brief Flag que indica si la banda necesita reabastecimiento urgente */
    int necesita_reabastecimiento;

    /** @brief Timestamp de la última alerta de inventario para evitar spam */
    time_t ultima_alerta_inventario;
} Banda;

/**
 * @brief Estructura que implementa una cola FIFO thread-safe para órdenes pendientes
 *
 * Implementa una cola circular con sincronización completa para múltiples hilos.
 * Utiliza variables de condición para bloquear hilos cuando la cola está vacía
 * o llena, garantizando un manejo eficiente de la memoria.
 */
typedef struct
{
    /** @brief Array circular que almacena las órdenes en la cola */
    Orden ordenes[MAX_ORDENES];

    /** @brief Índice del primer elemento en la cola (posición de desencolado) */
    int frente;

    /** @brief Índice de la siguiente posición libre en la cola (posición de encolado) */
    int atras;

    /** @brief Número actual de órdenes en la cola */
    int tamano;

    /** @brief Mutex para acceso exclusivo a la cola */
    pthread_mutex_t mutex;

    /** @brief Variable de condición para despertar hilos cuando la cola no está vacía */
    pthread_cond_t no_vacia;

    /** @brief Variable de condición para despertar hilos cuando la cola no está llena */
    pthread_cond_t no_llena;
} ColaFIFO;

/**
 * @brief Estructura principal que contiene todos los datos compartidos del sistema
 *
 * Esta estructura se almacena en memoria compartida POSIX para permitir la
 * comunicación entre el sistema principal y el panel de control. Contiene
 * el estado global del sistema, todas las bandas, la cola de órdenes y
 * parámetros de configuración.
 */
typedef struct
{
    /** @brief Array de todas las bandas de preparación del sistema */
    Banda bandas[MAX_BANDAS];

    /** @brief Cola FIFO que gestiona las órdenes pendientes de asignación */
    ColaFIFO cola_espera;

    /** @brief Número actual de bandas activas en el sistema */
    int num_bandas;

    /** @brief Flag que indica si el sistema está operativo (1) o en proceso de cierre (0) */
    int sistema_activo;

    /** @brief Contador total de órdenes procesadas exitosamente por todas las bandas */
    int total_ordenes_procesadas;

    /** @brief Contador total de órdenes generadas por el sistema */
    int total_ordenes_generadas;

    /** @brief Mutex global para operaciones que afectan a todo el sistema */
    pthread_mutex_t mutex_global;

    /** @brief Variable de condición para notificar cuando hay nuevas órdenes disponibles */
    pthread_cond_t nueva_orden;

    /** @brief Tiempo configurado para procesar cada ingrediente (segundos) */
    int tiempo_por_ingrediente;

    /** @brief Tiempo configurado entre la generación de nuevas órdenes (segundos) */
    int tiempo_nueva_orden;
} DatosCompartidos;

/**
 * @defgroup variables_globales Variables Globales del Sistema
 * @{
 */

/** @brief Puntero a la estructura de datos compartidos en memoria compartida */
DatosCompartidos *datos_compartidos;

/** @brief Hilo que genera órdenes de hamburguesas automáticamente */
pthread_t hilo_generador_ordenes;

/** @brief Hilo que asigna órdenes a las bandas disponibles */
pthread_t hilo_asignador_ordenes;

/** @brief Hilo que monitorea el inventario de todas las bandas */
pthread_t hilo_monitor_inventario;

/** @} */

/**
 * @brief Menú completo de hamburguesas disponibles en el sistema
 *
 * Define todos los tipos de hamburguesas que se pueden preparar, incluyendo
 * sus ingredientes específicos, cantidad de ingredientes y precios de venta.
 * Cada hamburguesa tiene una receta única que determina el orden de
 * preparación de los ingredientes.
 */
TipoHamburguesa menu_hamburguesas[NUM_TIPOS_HAMBURGUESA] = {
    /** @brief Hamburguesa Clásica: Pan, carne, lechuga, tomate */
    {"Clasica", {"pan_inferior", "carne", "lechuga", "tomate", "pan_superior"}, 5, 8.50},

    /** @brief Cheeseburger: Clásica + queso */
    {"Cheeseburger", {"pan_inferior", "carne", "queso", "lechuga", "tomate", "pan_superior"}, 6, 9.25},

    /** @brief BBQ Bacon: Con bacon, queso, cebolla y salsa BBQ */
    {"BBQ Bacon", {"pan_inferior", "carne", "bacon", "queso", "cebolla", "salsa_bbq", "pan_superior"}, 7, 11.75},

    /** @brief Vegetariana: Sin carne, con vegetal y aguacate */
    {"Vegetariana", {"pan_inferior", "vegetal", "lechuga", "tomate", "aguacate", "mayonesa", "pan_superior"}, 7, 10.25},

    /** @brief Deluxe: Hamburguesa premium con todos los ingredientes */
    {"Deluxe", {"pan_inferior", "carne", "queso", "bacon", "lechuga", "tomate", "cebolla", "mayonesa", "pan_superior"}, 9, 13.50},

    /** @brief Spicy Mexican: Con jalapeños y salsa picante */
    {"Spicy Mexican", {"pan_inferior", "carne", "queso", "jalapenos", "tomate", "cebolla", "salsa_picante", "pan_superior"}, 8, 12.00}};

/**
 * @brief Lista completa de ingredientes base disponibles en el sistema
 *
 * Define todos los ingredientes que pueden estar presentes en cualquier
 * hamburguesa. Cada banda tiene dispensadores para todos estos ingredientes,
 * permitiendo preparar cualquier tipo de hamburguesa del menú.
 */
char ingredientes_base[MAX_INGREDIENTES][MAX_NOMBRE_INGREDIENTE] = {
    "pan_inferior", "pan_superior", "carne", "queso", "tomate",
    "lechuga", "cebolla", "bacon", "mayonesa", "jalapenos",
    "aguacate", "vegetal", "salsa_bbq", "salsa_picante", "pepinillos"};

/**
 * @defgroup funciones_formato Funciones Auxiliares para Formato de Pantalla
 * @{
 *
 * Este grupo de funciones proporciona utilidades para formatear y mostrar
 * información en la consola de manera estructurada y legible. Incluye
 * funciones para centrar texto, crear tablas y manejar buffers de formato.
 */

/** @brief Buffers circulares para formateo de texto (evita desbordamientos) */
static char format_buffers[8][50];

/** @brief Índice del buffer actual en uso (implementa rotación circular) */
static int current_buffer = 0;

/**
 * @brief Formatea un texto para que tenga un ancho fijo específico
 *
 * Esta función toma un texto de entrada y lo ajusta al ancho especificado.
 * Si el texto es más largo que el ancho, se trunca y se añaden puntos suspensivos.
 * Si es más corto, se rellena con espacios en blanco.
 *
 * @param texto Puntero al texto de entrada (puede ser NULL)
 * @param ancho Ancho deseado para el texto formateado
 * @return Puntero al buffer formateado (no liberar, es buffer interno)
 *
 * @note Utiliza un sistema de buffers circulares para evitar desbordamientos
 * @note Si ancho < 3, se rellena completamente con puntos
 */
char *formatear_texto_fijo(const char *texto, int ancho)
{
    // Rotar al siguiente buffer disponible
    current_buffer = (current_buffer + 1) % 8;
    char *buffer = format_buffers[current_buffer];

    // Inicializar buffer con espacios en blanco
    memset(buffer, ' ', ancho);
    buffer[ancho] = '\0';

    // Manejar casos especiales
    if (texto == NULL || strlen(texto) == 0)
    {
        return buffer;
    }

    int len = strlen(texto);
    if (len <= ancho)
    {
        // Texto cabe completamente
        memcpy(buffer, texto, len);
    }
    else
    {
        // Texto es muy largo, truncar con "..."
        if (ancho >= 3)
        {
            memcpy(buffer, texto, ancho - 3);
            memcpy(buffer + ancho - 3, "...", 3);
        }
        else
        {
            // Ancho muy pequeño, rellenar con puntos
            memset(buffer, '.', ancho);
        }
    }
    return buffer;
}

/**
 * @brief Centra un texto dentro de un ancho específico
 *
 * Esta función toma un texto y lo centra horizontalmente dentro del ancho
 * especificado, rellenando con espacios en blanco a ambos lados.
 * Si el texto es más largo que el ancho, se delega a formatear_texto_fijo().
 *
 * @param texto Puntero al texto a centrar (puede ser NULL)
 * @param ancho Ancho total disponible para centrar el texto
 * @return Puntero al buffer con el texto centrado (no liberar, es buffer interno)
 *
 * @note Utiliza el mismo sistema de buffers circulares que formatear_texto_fijo()
 * @note Si el texto es NULL, retorna un buffer lleno de espacios
 */
char *centrar_texto(const char *texto, int ancho)
{
    // Rotar al siguiente buffer disponible
    current_buffer = (current_buffer + 1) % 8;
    char *buffer = format_buffers[current_buffer];

    // Inicializar buffer con espacios en blanco
    memset(buffer, ' ', ancho);
    buffer[ancho] = '\0';

    // Manejar texto NULL
    if (texto == NULL)
        return buffer;

    int len = strlen(texto);
    if (len >= ancho)
    {
        // Texto es muy largo, usar formateo fijo
        return formatear_texto_fijo(texto, ancho);
    }

    // Calcular posición de inicio para centrar
    int inicio = (ancho - len) / 2;
    memcpy(buffer + inicio, texto, len);
    return buffer;
}

/**
 * @brief Imprime una línea de separación para tablas con formato personalizable
 *
 * Esta función crea líneas de separación para tablas, permitiendo especificar
 * los caracteres de borde izquierdo, centro, derecho y el relleno interno.
 * Es útil para crear tablas con diferentes estilos de bordes.
 *
 * @param num_columnas Número de columnas en la tabla
 * @param ancho_columna Ancho de cada columna (incluyendo bordes)
 * @param izq Carácter o cadena para el borde izquierdo de la tabla
 * @param centro Carácter o cadena para separar columnas internas
 * @param der Carácter o cadena para el borde derecho de la tabla
 * @param relleno Carácter o cadena para rellenar el espacio interno de las columnas
 *
 * @note Ejemplo de uso: imprimir_linea_separacion(3, 20, "╔", "╦", "╗", "═")
 * @note Produce: ╔══════════════════╦══════════════════╦══════════════════╗
 */
void imprimir_linea_separacion(int num_columnas, int ancho_columna, const char *izq, const char *centro, const char *der, const char *relleno)
{
    for (int col = 0; col < num_columnas; col++)
    {
        if (col == 0)
        {
            // Primera columna: borde izquierdo
            printf("%s", izq);
        }

        // Rellenar el contenido de la columna
        for (int i = 0; i < ancho_columna - 2; i++)
        {
            printf("%s", relleno);
        }

        if (col == num_columnas - 1)
        {
            // Última columna: borde derecho
            printf("%s", der);
        }
        else
        {
            // Columnas intermedias: separador
            printf("%s", centro);
        }
    }
    printf("\n");
}

/**
 * @brief Imprime una fila de contenido de tabla con formato uniforme
 *
 * Esta función toma un array de contenidos y los imprime en formato de tabla,
 * aplicando formateo consistente a cada columna y separando las columnas
 * con espacios para mejor legibilidad.
 *
 * @param contenidos Array bidimensional con el contenido de cada columna
 * @param num_columnas Número de columnas a imprimir
 * @param ancho_columna Ancho total de cada columna (incluyendo bordes)
 *
 * @note Cada contenido se formatea automáticamente al ancho de la columna
 * @note Las columnas se separan con dos espacios para mejor legibilidad
 * @note Utiliza caracteres Unicode para bordes de tabla (║)
 */
void imprimir_fila_contenido(char contenidos[][50], int num_columnas, int ancho_columna)
{
    for (int col = 0; col < num_columnas; col++)
    {
        // Imprimir contenido de la columna con bordes y formateo
        printf("║%s║", formatear_texto_fijo(contenidos[col], ancho_columna - 2));

        // Separar columnas con espacios (excepto la última)
        if (col < num_columnas - 1)
        {
            printf("  ");
        }
    }
    printf("\n");
}

/**
 * @defgroup prototipos Prototipos de Funciones del Sistema
 * @{
 *
 * Declaraciones de todas las funciones principales del sistema, organizadas
 * por categorías funcionales para facilitar la comprensión del código.
 */

// ============================================================================
// FUNCIONES DE INICIALIZACIÓN Y CONFIGURACIÓN
// ============================================================================

/**
 * @brief Inicializa el sistema completo con la configuración especificada
 * @param num_bandas Número de bandas de preparación a crear
 * @param tiempo_ingrediente Tiempo en segundos para procesar cada ingrediente
 * @param tiempo_orden Tiempo en segundos entre generación de nuevas órdenes
 */
void inicializar_sistema(int num_bandas, int tiempo_ingrediente, int tiempo_orden);

/**
 * @brief Muestra el menú completo de hamburguesas disponibles
 * @note Se ejecuta automáticamente al inicializar el sistema
 */
void mostrar_menu_hamburguesas();

// ============================================================================
// FUNCIONES DE HILOS DE TRABAJO (WORKER THREADS)
// ============================================================================

/**
 * @brief Función principal del hilo de cada banda de preparación
 * @param arg Puntero al ID de la banda (int*)
 * @return NULL al terminar
 */
void *banda_worker(void *arg);

/**
 * @brief Hilo que genera órdenes de hamburguesas automáticamente
 * @param arg Parámetro no utilizado (NULL)
 * @return NULL al terminar
 */
void *generador_ordenes(void *arg);

/**
 * @brief Hilo que asigna órdenes a las bandas disponibles
 * @param arg Parámetro no utilizado (NULL)
 * @return NULL al terminar
 */
void *asignador_ordenes(void *arg);

/**
 * @brief Hilo que monitorea el inventario de todas las bandas
 * @param arg Parámetro no utilizado (NULL)
 * @return NULL al terminar
 */
void *monitor_inventario(void *arg);

// ============================================================================
// FUNCIONES DE PROCESAMIENTO DE ÓRDENES
// ============================================================================

/**
 * @brief Procesa una orden completa en la banda especificada
 * @param banda_id ID de la banda que procesará la orden
 * @param orden Puntero a la orden a procesar
 */
void procesar_orden(int banda_id, Orden *orden);

/**
 * @brief Verifica si una banda tiene suficientes ingredientes para una orden
 * @param banda_id ID de la banda a verificar
 * @param orden Puntero a la orden a verificar
 * @return 1 si hay suficientes ingredientes, 0 en caso contrario
 */
int verificar_ingredientes_banda(int banda_id, Orden *orden);

/**
 * @brief Consume los ingredientes necesarios para una orden
 * @param banda_id ID de la banda donde se consumirán los ingredientes
 * @param orden Puntero a la orden que consume los ingredientes
 */
void consumir_ingredientes_banda(int banda_id, Orden *orden);

/**
 * @brief Encuentra una banda disponible para procesar una orden
 * @param orden Puntero a la orden que necesita asignación
 * @return ID de la banda disponible o -1 si no hay ninguna
 */
int encontrar_banda_disponible(Orden *orden);

// ============================================================================
// FUNCIONES DE GESTIÓN DE LOGS E INVENTARIO
// ============================================================================

/**
 * @brief Añade una entrada de log a una banda específica
 * @param banda_id ID de la banda donde se registrará el log
 * @param mensaje Texto del mensaje a registrar
 * @param es_alerta 1 si es una alerta crítica, 0 si es información normal
 */
void agregar_log_banda(int banda_id, const char *mensaje, int es_alerta);

/**
 * @brief Verifica el estado del inventario de una banda y genera alertas
 * @param banda_id ID de la banda a verificar
 */
void verificar_inventario_banda(int banda_id);

// ============================================================================
// FUNCIONES DE GESTIÓN DE COLA FIFO
// ============================================================================

/**
 * @brief Añade una orden al final de la cola de espera
 * @param orden Puntero a la orden a encolar
 */
void encolar_orden(Orden *orden);

/**
 * @brief Extrae la primera orden de la cola de espera
 * @return Puntero a la orden extraída o NULL si la cola está vacía
 */
Orden *desencolar_orden();

// ============================================================================
// FUNCIONES DE VISUALIZACIÓN Y MONITOREO
// ============================================================================

/**
 * @brief Muestra el estado del sistema en formato de tabla columnar
 * @note Formato detallado con múltiples columnas por banda
 */
void mostrar_estado_columnar();

/**
 * @brief Muestra el estado del sistema en formato compacto
 * @note Formato resumido para pantallas pequeñas
 */
void mostrar_estado_compacto();

/**
 * @brief Muestra el estado del sistema adaptándose al tamaño de pantalla
 * @note Selecciona automáticamente entre columnar y compacto
 */
void mostrar_estado_adaptativo();

// ============================================================================
// FUNCIONES DE UTILIDAD Y CONTROL
// ============================================================================

/**
 * @brief Genera una orden específica con el ID proporcionado
 * @param orden Puntero a la estructura de orden a llenar
 * @param id ID único para la nueva orden
 */
void generar_orden_especifica(Orden *orden, int id);

/**
 * @brief Reabastece completamente el inventario de una banda
 * @param banda_id ID de la banda a reabastecer
 */
void reabastecer_banda(int banda_id);

/**
 * @brief Limpia todos los recursos del sistema y termina los hilos
 * @note Se ejecuta automáticamente al recibir señales de terminación
 */
void limpiar_sistema();

/**
 * @brief Manejador de señales del sistema operativo
 * @param sig Número de la señal recibida
 */
void manejar_senal(int sig);

/**
 * @brief Valida y procesa los parámetros de línea de comandos
 * @param argc Número de argumentos
 * @param argv Array de argumentos
 * @param num_bandas Puntero donde se almacenará el número de bandas
 * @param tiempo_ingrediente Puntero donde se almacenará el tiempo por ingrediente
 * @param tiempo_orden Puntero donde se almacenará el tiempo entre órdenes
 * @return 1 si los parámetros son válidos, 0 en caso contrario
 */
int validar_parametros(int argc, char *argv[], int *num_bandas, int *tiempo_ingrediente, int *tiempo_orden);

/**
 * @brief Muestra la ayuda completa del sistema con ejemplos de uso
 */
void mostrar_ayuda();

/** @} */

// ═══════════════════════════════════════════════════════════════
// FUNCIONES DE INICIALIZACIÓN
// ═══════════════════════════════════════════════════════════════

/**
 * @brief Inicializa completamente el sistema de hamburguesas
 *
 * Esta función es el punto de entrada principal para la configuración del sistema.
 * Realiza las siguientes operaciones críticas:
 *
 * 1. Configura la memoria compartida POSIX para comunicación entre procesos
 * 2. Inicializa todas las estructuras de datos del sistema
 * 3. Configura los mutexes y variables de condición para sincronización
 * 4. Inicializa todas las bandas de preparación con inventario completo
 * 5. Configura la cola FIFO para gestión de órdenes pendientes
 * 6. Establece los parámetros de tiempo configurables
 *
 * @param num_bandas Número de bandas de preparación a crear (1-10)
 * @param tiempo_ingrediente Tiempo en segundos para procesar cada ingrediente
 * @param tiempo_orden Tiempo en segundos entre generación de nuevas órdenes
 *
 * @note Esta función debe ser llamada antes de crear cualquier hilo del sistema
 * @note La memoria compartida se crea con permisos 0666 para acceso del panel de control
 * @note Todas las bandas se inicializan con inventario completo (CAPACIDAD_DISPENSADOR)
 *
 * @warning Si la función falla, el programa termina con exit(1)
 * @warning La memoria compartida previa se elimina automáticamente
 */
void inicializar_sistema(int num_bandas, int tiempo_ingrediente, int tiempo_orden)
{
    // Limpiar memoria compartida previa para evitar conflictos
    shm_unlink("/burger_system");

    // Crear nueva memoria compartida POSIX
    int shm_fd = shm_open("/burger_system", O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1)
    {
        perror("Error creando memoria compartida");
        exit(1);
    }

    // Configurar tamaño y mapear la memoria
    ftruncate(shm_fd, sizeof(DatosCompartidos));
    datos_compartidos = mmap(0, sizeof(DatosCompartidos), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);

    if (datos_compartidos == MAP_FAILED)
    {
        perror("Error mapeando memoria compartida");
        exit(1);
    }

    // Inicializar estructura de datos compartidos
    memset(datos_compartidos, 0, sizeof(DatosCompartidos));
    datos_compartidos->num_bandas = num_bandas;
    datos_compartidos->sistema_activo = 1;
    datos_compartidos->total_ordenes_procesadas = 0;
    datos_compartidos->total_ordenes_generadas = 0;

    // Configurar parámetros de tiempo configurables
    datos_compartidos->tiempo_por_ingrediente = tiempo_ingrediente;
    datos_compartidos->tiempo_nueva_orden = tiempo_orden;

    // Inicializar mecanismos de sincronización globales
    pthread_mutex_init(&datos_compartidos->mutex_global, NULL);
    pthread_cond_init(&datos_compartidos->nueva_orden, NULL);

    // Inicializar todas las bandas de preparación
    for (int i = 0; i < num_bandas; i++)
    {
        // Configuración básica de la banda
        datos_compartidos->bandas[i].id = i;
        datos_compartidos->bandas[i].activa = 1;
        datos_compartidos->bandas[i].pausada = 0;
        datos_compartidos->bandas[i].hamburguesas_procesadas = 0;
        datos_compartidos->bandas[i].procesando_orden = 0;
        datos_compartidos->bandas[i].num_logs = 0;
        datos_compartidos->bandas[i].necesita_reabastecimiento = 0;
        datos_compartidos->bandas[i].ultima_alerta_inventario = 0;

        // Estado inicial de la banda
        strcpy(datos_compartidos->bandas[i].estado_actual, "ESPERANDO");
        strcpy(datos_compartidos->bandas[i].ingrediente_actual, "");

        // Inicializar mecanismos de sincronización de la banda
        pthread_mutex_init(&datos_compartidos->bandas[i].mutex, NULL);
        pthread_cond_init(&datos_compartidos->bandas[i].condicion, NULL);

        // Inicializar dispensadores de ingredientes con inventario completo
        for (int j = 0; j < MAX_INGREDIENTES; j++)
        {
            strcpy(datos_compartidos->bandas[i].dispensadores[j].nombre, ingredientes_base[j]);
            datos_compartidos->bandas[i].dispensadores[j].cantidad = CAPACIDAD_DISPENSADOR;
            pthread_mutex_init(&datos_compartidos->bandas[i].dispensadores[j].mutex, NULL);
        }

        // Registrar inicio de la banda en el sistema de logs
        agregar_log_banda(i, "BANDA INICIADA", 0);
    }

    // Inicializar cola FIFO para gestión de órdenes pendientes
    datos_compartidos->cola_espera.frente = 0;
    datos_compartidos->cola_espera.atras = 0;
    datos_compartidos->cola_espera.tamano = 0;
    pthread_mutex_init(&datos_compartidos->cola_espera.mutex, NULL);
    pthread_cond_init(&datos_compartidos->cola_espera.no_vacia, NULL);
    pthread_cond_init(&datos_compartidos->cola_espera.no_llena, NULL);

    // Mostrar información de configuración del sistema
    printf("Sistema inicializado con %d bandas de preparación\n", num_bandas);
    printf("Configuración de tiempos:\n");
    printf("  • Tiempo por ingrediente: %d segundos\n", tiempo_ingrediente);
    printf("  • Tiempo entre órdenes: %d segundos\n", tiempo_orden);

    // Mostrar menú de hamburguesas disponibles
    mostrar_menu_hamburguesas();
}

void mostrar_menu_hamburguesas()
{
    printf("\n╔══════════════════════════════════════════════════════════════════╗\n");
    printf("║                         MENU DE HAMBURGUESAS                     ║\n");
    printf("╠══════════════════════════════════════════════════════════════════╣\n");

    for (int i = 0; i < NUM_TIPOS_HAMBURGUESA; i++)
    {
        printf("║ %d. %-20s - $%6.2f                                ║\n",
               i + 1, menu_hamburguesas[i].nombre, menu_hamburguesas[i].precio);
    }
    printf("╚══════════════════════════════════════════════════════════════════╝\n");
}

// ═══════════════════════════════════════════════════════════════
// FUNCIONES DE MANEJO DE LOGS
// ═══════════════════════════════════════════════════════════════

void agregar_log_banda(int banda_id, const char *mensaje, int es_alerta)
{
    if (banda_id < 0 || banda_id >= datos_compartidos->num_bandas)
        return;

    Banda *banda = &datos_compartidos->bandas[banda_id];

    pthread_mutex_lock(&banda->mutex);

    if (banda->num_logs >= MAX_LOGS_POR_BANDA)
    {
        for (int i = 1; i < MAX_LOGS_POR_BANDA; i++)
        {
            banda->logs[i - 1] = banda->logs[i];
        }
        banda->num_logs = MAX_LOGS_POR_BANDA - 1;
    }

    strcpy(banda->logs[banda->num_logs].mensaje, mensaje);
    banda->logs[banda->num_logs].timestamp = time(NULL);
    banda->logs[banda->num_logs].es_alerta = es_alerta;
    banda->num_logs++;

    pthread_mutex_unlock(&banda->mutex);
}

// ═══════════════════════════════════════════════════════════════
// FUNCIONES DE VERIFICACIÓN DE INVENTARIO
// ═══════════════════════════════════════════════════════════════

void verificar_inventario_banda(int banda_id)
{
    if (banda_id < 0 || banda_id >= datos_compartidos->num_bandas)
        return;

    Banda *banda = &datos_compartidos->bandas[banda_id];
    time_t ahora = time(NULL);

    // Evitar spam de alertas (máximo una cada 30 segundos)
    if (ahora - banda->ultima_alerta_inventario < 30)
        return;

    int ingredientes_bajos = 0;
    int ingredientes_agotados = 0;
    char ingredientes_criticos[200] = "";

    for (int i = 0; i < MAX_INGREDIENTES; i++)
    {
        pthread_mutex_lock(&banda->dispensadores[i].mutex);
        int cantidad = banda->dispensadores[i].cantidad;

        if (cantidad == 0)
        {
            ingredientes_agotados++;
            if (strlen(ingredientes_criticos) > 0)
                strcat(ingredientes_criticos, ", ");
            strcat(ingredientes_criticos, banda->dispensadores[i].nombre);
        }
        else if (cantidad <= UMBRAL_INVENTARIO_BAJO)
        {
            ingredientes_bajos++;
        }

        pthread_mutex_unlock(&banda->dispensadores[i].mutex);
    }

    if (ingredientes_agotados > 0)
    {
        char mensaje_alerta[250];
        snprintf(mensaje_alerta, sizeof(mensaje_alerta),
                 "ALERTA! BANDA %d SIN: %s", banda_id + 1, ingredientes_criticos);
        agregar_log_banda(banda_id, mensaje_alerta, 1);
        banda->necesita_reabastecimiento = 1;
        banda->ultima_alerta_inventario = ahora;
    }
    else if (ingredientes_bajos >= 3)
    {
        char mensaje_alerta[100];
        snprintf(mensaje_alerta, sizeof(mensaje_alerta),
                 "AVISO: Banda %d necesita reabastecimiento", banda_id + 1);
        agregar_log_banda(banda_id, mensaje_alerta, 1);
        banda->necesita_reabastecimiento = 1;
        banda->ultima_alerta_inventario = ahora;
    }
    else
    {
        banda->necesita_reabastecimiento = 0;
    }
}

void *monitor_inventario(void *arg)
{
    (void)arg;

    while (datos_compartidos->sistema_activo)
    {
        for (int i = 0; i < datos_compartidos->num_bandas; i++)
        {
            verificar_inventario_banda(i);
        }
        sleep(15); // Chequear cada 15 segundos
    }
    return NULL;
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

        // Si no está procesando, esperar
        if (!banda->procesando_orden)
        {
            strcpy(banda->estado_actual, "ESPERANDO");
            pthread_mutex_unlock(&banda->mutex);
            usleep(100000);
            continue;
        }

        pthread_mutex_unlock(&banda->mutex);

        // Procesar la orden asignada
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
        agregar_log_banda(banda_id, log_msg, 0);

        verificar_inventario_banda(banda_id);
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
        nueva_orden.intentos_asignacion = 0;
        encolar_orden(&nueva_orden);

        pthread_mutex_lock(&datos_compartidos->mutex_global);
        datos_compartidos->total_ordenes_generadas++;
        pthread_mutex_unlock(&datos_compartidos->mutex_global);

        printf("\n[NUEVA ORDEN] %s #%d generada - En cola\n",
               nueva_orden.nombre_hamburguesa, nueva_orden.id_orden);

        pthread_cond_broadcast(&datos_compartidos->nueva_orden);

        // MODIFICADO: Usar tiempo configurado en lugar de valor fijo
        sleep(datos_compartidos->tiempo_nueva_orden);
    }
    return NULL;
}

void *asignador_ordenes(void *arg)
{
    (void)arg;

    while (datos_compartidos->sistema_activo)
    {
        Orden *orden = desencolar_orden();
        if (orden != NULL)
        {
            orden->intentos_asignacion++;
            int banda_asignada = encontrar_banda_disponible(orden);

            if (banda_asignada >= 0)
            {
                // Asignar orden a la banda encontrada
                Banda *banda = &datos_compartidos->bandas[banda_asignada];

                pthread_mutex_lock(&banda->mutex);
                banda->procesando_orden = 1;
                banda->orden_actual = *orden;
                banda->orden_actual.asignada_a_banda = banda_asignada;
                sprintf(banda->estado_actual, "PREPARANDO %s", orden->nombre_hamburguesa);
                pthread_mutex_unlock(&banda->mutex);

                char log_msg[100];
                sprintf(log_msg, "ASIGNADA %s #%d", orden->nombre_hamburguesa, orden->id_orden);
                agregar_log_banda(banda_asignada, log_msg, 0);
            }
            else
            {
                // Re-encolar para intentar más tarde
                if (orden->intentos_asignacion < 20)
                { // Máximo 20 intentos
                    encolar_orden(orden);
                    sleep(3); // Esperar antes del siguiente intento
                }
                else
                {
                    // Después de muchos intentos, la orden se pierde (timeout)
                    printf("\n⚠️  [TIMEOUT] Orden %s #%d descartada por timeout\n",
                           orden->nombre_hamburguesa, orden->id_orden);
                }
            }
        }
        else
        {
            usleep(200000); // No hay órdenes, esperar 200ms
        }
    }
    return NULL;
}

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
            return i;
        }
    }
    return -1;
}

// ═══════════════════════════════════════════════════════════════
// FUNCIONES DE PROCESAMIENTO DE ÓRDENES
// ═══════════════════════════════════════════════════════════════

void procesar_orden(int banda_id, Orden *orden)
{
    Banda *banda = &datos_compartidos->bandas[banda_id];

    char log_msg[100];
    sprintf(log_msg, "INICIANDO %s #%d", orden->nombre_hamburguesa, orden->id_orden);
    agregar_log_banda(banda_id, log_msg, 0);

    // Consumir ingredientes
    consumir_ingredientes_banda(banda_id, orden);

    // Simular preparación paso a paso
    for (int i = 0; i < orden->num_ingredientes; i++)
    {
        pthread_mutex_lock(&banda->mutex);
        orden->paso_actual = i + 1;
        strcpy(banda->ingrediente_actual, orden->ingredientes_solicitados[i]);
        sprintf(banda->estado_actual, "AGREGANDO %s", orden->ingredientes_solicitados[i]);
        pthread_mutex_unlock(&banda->mutex);

        sprintf(log_msg, "Agregando %s...", orden->ingredientes_solicitados[i]);
        agregar_log_banda(banda_id, log_msg, 0);

        // MODIFICADO: Usar tiempo configurado en lugar de valor fijo
        sleep(datos_compartidos->tiempo_por_ingrediente);
    }

    pthread_mutex_lock(&banda->mutex);
    sprintf(banda->estado_actual, "FINALIZANDO %s", orden->nombre_hamburguesa);
    pthread_mutex_unlock(&banda->mutex);

    agregar_log_banda(banda_id, "HAMBURGUESA LISTA!", 0);
    sleep(1); // Tiempo final
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
// FUNCIONES DE DISPLAY
// ═══════════════════════════════════════════════════════════════

void mostrar_estado_columnar()
{
    printf("\033[2J\033[H"); // Limpiar pantalla

    // Encabezado del sistema
    printf("╔═══════════════════════════════════════════════════════════════════════════════════════════════════════════════╗\n");
    printf("║                                      SISTEMA DE HAMBURGUESAS - ESTADO                                         ║\n");
    printf("║ Generadas: %-6d  │  Procesadas: %-6d  │  En cola: %-6d  │  Bandas: %-6d                              ║\n",
           datos_compartidos->total_ordenes_generadas,
           datos_compartidos->total_ordenes_procesadas,
           datos_compartidos->cola_espera.tamano,
           datos_compartidos->num_bandas);
    printf("║ Nueva orden cada %ds │ Ingrediente cada %ds │ Asignacion inteligente FIFO                                  ║\n",
           datos_compartidos->tiempo_nueva_orden,
           datos_compartidos->tiempo_por_ingrediente);
    printf("╚═══════════════════════════════════════════════════════════════════════════════════════════════════════════════╝\n");

    // Mostrar alertas de inventario
    int bandas_con_alertas = 0;
    for (int i = 0; i < datos_compartidos->num_bandas; i++)
    {
        if (datos_compartidos->bandas[i].necesita_reabastecimiento)
        {
            bandas_con_alertas++;
        }
    }

    if (bandas_con_alertas > 0)
    {
        printf("\n🚨 ALERTAS DE INVENTARIO: %d bandas necesitan reabastecimiento 🚨\n", bandas_con_alertas);
        for (int i = 0; i < datos_compartidos->num_bandas; i++)
        {
            if (datos_compartidos->bandas[i].necesita_reabastecimiento)
            {
                printf("   ⚠️  BANDA %d requiere reabastecimiento urgente\n", i + 1);
            }
        }
        printf("\n");
    }

    // Layout columnar
    const int ANCHO_COLUMNA = 40;
    const int MAX_COLUMNAS = 3;

    int filas_bandas = (datos_compartidos->num_bandas + MAX_COLUMNAS - 1) / MAX_COLUMNAS;

    for (int fila = 0; fila < filas_bandas; fila++)
    {
        int banda_inicio = fila * MAX_COLUMNAS;
        int banda_fin = (banda_inicio + MAX_COLUMNAS > datos_compartidos->num_bandas) ? datos_compartidos->num_bandas : banda_inicio + MAX_COLUMNAS;
        int num_columnas_fila = banda_fin - banda_inicio;

        // Bordes superiores
        imprimir_linea_separacion(num_columnas_fila, ANCHO_COLUMNA, "╔", "╗  ╔", "╗", "═");

        // Encabezados de bandas
        char titulos[MAX_COLUMNAS][50];
        for (int col = 0; col < num_columnas_fila; col++)
        {
            int banda_actual = banda_inicio + col;
            sprintf(titulos[col], "BANDA %d", banda_actual + 1);
            strcpy(titulos[col], centrar_texto(titulos[col], ANCHO_COLUMNA - 2));
        }
        imprimir_fila_contenido(titulos, num_columnas_fila, ANCHO_COLUMNA);

        // Separador
        imprimir_linea_separacion(num_columnas_fila, ANCHO_COLUMNA, "╠", "╣  ╠", "╣", "═");

        // Sección inventario
        char subtitulo_inv[MAX_COLUMNAS][50];
        for (int col = 0; col < num_columnas_fila; col++)
        {
            strcpy(subtitulo_inv[col], centrar_texto("INVENTARIO", ANCHO_COLUMNA - 2));
        }
        imprimir_fila_contenido(subtitulo_inv, num_columnas_fila, ANCHO_COLUMNA);

        imprimir_linea_separacion(num_columnas_fila, ANCHO_COLUMNA, "╠", "╣  ╠", "╣", "─");

        // Mostrar inventario (8 líneas)
        for (int ing = 0; ing < 8; ing++)
        {
            char lineas_inventario[MAX_COLUMNAS][50];

            for (int col = 0; col < num_columnas_fila; col++)
            {
                int banda = banda_inicio + col;
                if (banda < datos_compartidos->num_bandas && ing < MAX_INGREDIENTES)
                {
                    Banda *b = &datos_compartidos->bandas[banda];
                    pthread_mutex_lock(&b->dispensadores[ing].mutex);

                    char nombre_corto[15];
                    strncpy(nombre_corto, b->dispensadores[ing].nombre, 14);
                    nombre_corto[14] = '\0';

                    int cantidad = b->dispensadores[ing].cantidad;

                    if (cantidad == 0)
                    {
                        sprintf(lineas_inventario[col], "%-14s: %2d [AGOTADO]", nombre_corto, cantidad);
                    }
                    else if (cantidad <= UMBRAL_INVENTARIO_BAJO)
                    {
                        sprintf(lineas_inventario[col], "%-14s: %2d [CRITICO]", nombre_corto, cantidad);
                    }
                    else
                    {
                        sprintf(lineas_inventario[col], "%-14s: %2d", nombre_corto, cantidad);
                    }

                    pthread_mutex_unlock(&b->dispensadores[ing].mutex);
                }
                else
                {
                    strcpy(lineas_inventario[col], "");
                }
            }
            imprimir_fila_contenido(lineas_inventario, num_columnas_fila, ANCHO_COLUMNA);
        }

        // Separador para preparación
        imprimir_linea_separacion(num_columnas_fila, ANCHO_COLUMNA, "╠", "╣  ╠", "╣", "═");

        // Sección preparación
        char subtitulo_prep[MAX_COLUMNAS][50];
        for (int col = 0; col < num_columnas_fila; col++)
        {
            strcpy(subtitulo_prep[col], centrar_texto("PREPARACION", ANCHO_COLUMNA - 2));
        }
        imprimir_fila_contenido(subtitulo_prep, num_columnas_fila, ANCHO_COLUMNA);

        imprimir_linea_separacion(num_columnas_fila, ANCHO_COLUMNA, "╠", "╣  ╠", "╣", "─");

        // Estado de preparación (6 líneas)
        for (int linea = 0; linea < 6; linea++)
        {
            char lineas_prep[MAX_COLUMNAS][50];

            for (int col = 0; col < num_columnas_fila; col++)
            {
                int banda = banda_inicio + col;
                if (banda < datos_compartidos->num_bandas)
                {
                    Banda *b = &datos_compartidos->bandas[banda];
                    pthread_mutex_lock(&b->mutex);

                    switch (linea)
                    {
                    case 0:
                        if (b->procesando_orden)
                        {
                            char nombre_corto[18];
                            strncpy(nombre_corto, b->orden_actual.nombre_hamburguesa, 17);
                            nombre_corto[17] = '\0';
                            sprintf(lineas_prep[col], "Orden %d: %s",
                                    b->orden_actual.id_orden, nombre_corto);
                        }
                        else
                        {
                            strcpy(lineas_prep[col], "Sin orden activa");
                        }
                        break;
                    case 1:
                    {
                        char estado_corto[25];
                        strncpy(estado_corto, b->estado_actual, 24);
                        estado_corto[24] = '\0';
                        sprintf(lineas_prep[col], "Estado: %s", estado_corto);
                    }
                    break;
                    case 2:
                        if (b->procesando_orden && strlen(b->ingrediente_actual) > 0)
                        {
                            char ing_corto[18];
                            strncpy(ing_corto, b->ingrediente_actual, 17);
                            ing_corto[17] = '\0';
                            sprintf(lineas_prep[col], "Ingrediente: %s", ing_corto);
                        }
                        else
                        {
                            strcpy(lineas_prep[col], "");
                        }
                        break;
                    case 3:
                        if (b->procesando_orden)
                        {
                            sprintf(lineas_prep[col], "Progreso: %d/%d pasos",
                                    b->orden_actual.paso_actual, b->orden_actual.num_ingredientes);
                        }
                        else
                        {
                            strcpy(lineas_prep[col], "");
                        }
                        break;
                    case 4:
                        sprintf(lineas_prep[col], "Procesadas: %d", b->hamburguesas_procesadas);
                        break;
                    case 5:
                        if (b->pausada)
                        {
                            strcpy(lineas_prep[col], "[PAUSADA]");
                        }
                        else if (b->necesita_reabastecimiento)
                        {
                            strcpy(lineas_prep[col], "[NECESITA INVENTARIO]");
                        }
                        else
                        {
                            strcpy(lineas_prep[col], "");
                        }
                        break;
                    }

                    pthread_mutex_unlock(&b->mutex);
                }
                else
                {
                    strcpy(lineas_prep[col], "");
                }
            }
            imprimir_fila_contenido(lineas_prep, num_columnas_fila, ANCHO_COLUMNA);
        }

        // Separador para logs
        imprimir_linea_separacion(num_columnas_fila, ANCHO_COLUMNA, "╠", "╣  ╠", "╣", "═");

        // Sección logs
        char subtitulo_logs[MAX_COLUMNAS][50];
        for (int col = 0; col < num_columnas_fila; col++)
        {
            strcpy(subtitulo_logs[col], centrar_texto("LOGS", ANCHO_COLUMNA - 2));
        }
        imprimir_fila_contenido(subtitulo_logs, num_columnas_fila, ANCHO_COLUMNA);

        imprimir_linea_separacion(num_columnas_fila, ANCHO_COLUMNA, "╠", "╣  ╠", "╣", "─");

        // Mostrar logs (6 líneas)
        for (int log_line = 0; log_line < 6; log_line++)
        {
            char lineas_logs[MAX_COLUMNAS][50];

            for (int col = 0; col < num_columnas_fila; col++)
            {
                int banda = banda_inicio + col;
                if (banda < datos_compartidos->num_bandas)
                {
                    Banda *b = &datos_compartidos->bandas[banda];
                    pthread_mutex_lock(&b->mutex);

                    if (log_line < b->num_logs)
                    {
                        int log_idx = b->num_logs - 1 - log_line;
                        if (log_idx >= 0)
                        {
                            char log_texto[44];
                            strncpy(log_texto, b->logs[log_idx].mensaje, 35);
                            log_texto[35] = '\0';
                            if (b->logs[log_idx].es_alerta)
                            {
                                sprintf(lineas_logs[col], "🚨 %s", log_texto);
                            }
                            else
                            {
                                strcpy(lineas_logs[col], log_texto);
                            }
                        }
                        else
                        {
                            strcpy(lineas_logs[col], "");
                        }
                    }
                    else
                    {
                        strcpy(lineas_logs[col], "");
                    }

                    pthread_mutex_unlock(&b->mutex);
                }
                else
                {
                    strcpy(lineas_logs[col], "");
                }
            }
            imprimir_fila_contenido(lineas_logs, num_columnas_fila, ANCHO_COLUMNA);
        }

        // Bordes inferiores
        imprimir_linea_separacion(num_columnas_fila, ANCHO_COLUMNA, "╚", "╝  ╚", "╝", "═");
        printf("\n");
    }

    printf("Presiona Ctrl+C para salir del sistema\n");
    printf("⏱️  Tiempos: %ds por ingrediente, %ds entre órdenes\n",
           datos_compartidos->tiempo_por_ingrediente,
           datos_compartidos->tiempo_nueva_orden);
}

void mostrar_estado_compacto()
{
    printf("\033[2J\033[H");

    printf("╔═══════════════════════════════════════════════════════════════════╗\n");
    printf("║              SISTEMA DE HAMBURGUESAS - COMPACTO                   ║\n");
    printf("╚═══════════════════════════════════════════════════════════════════╝\n");
    printf("Generadas: %d │ Procesadas: %d │ En cola: %d │ Bandas: %d\n",
           datos_compartidos->total_ordenes_generadas,
           datos_compartidos->total_ordenes_procesadas,
           datos_compartidos->cola_espera.tamano,
           datos_compartidos->num_bandas);
    printf("⏱️ Tiempos: %ds/ingrediente │ %ds entre órdenes\n\n",
           datos_compartidos->tiempo_por_ingrediente,
           datos_compartidos->tiempo_nueva_orden);

    // Mostrar alertas
    int bandas_con_alertas = 0;
    for (int i = 0; i < datos_compartidos->num_bandas; i++)
    {
        if (datos_compartidos->bandas[i].necesita_reabastecimiento)
        {
            bandas_con_alertas++;
        }
    }

    if (bandas_con_alertas > 0)
    {
        printf("🚨 ALERTAS: %d bandas necesitan reabastecimiento\n\n", bandas_con_alertas);
    }

    for (int i = 0; i < datos_compartidos->num_bandas; i++)
    {
        Banda *b = &datos_compartidos->bandas[i];

        pthread_mutex_lock(&b->mutex);

        printf("BANDA %d: %s%s", i + 1,
               b->pausada ? "[PAUSADA]" : (b->activa ? "[ACTIVA]" : "[INACT]"),
               b->necesita_reabastecimiento ? " ⚠️" : "");

        if (b->procesando_orden)
        {
            char nombre_corto[16];
            strncpy(nombre_corto, b->orden_actual.nombre_hamburguesa, 15);
            nombre_corto[15] = '\0';
            printf(" │ Orden %d: %s - Progreso: %d/%d",
                   b->orden_actual.id_orden, nombre_corto,
                   b->orden_actual.paso_actual, b->orden_actual.num_ingredientes);
        }
        else
        {
            char estado_corto[20];
            strncpy(estado_corto, b->estado_actual, 19);
            estado_corto[19] = '\0';
            printf(" │ %s", estado_corto);
        }

        printf(" │ Procesadas: %d\n", b->hamburguesas_procesadas);

        // Mostrar inventario crítico
        printf("  Stock crítico: ");
        int items_criticos = 0;
        for (int j = 0; j < MAX_INGREDIENTES && items_criticos < 5; j++)
        {
            pthread_mutex_lock(&b->dispensadores[j].mutex);
            if (b->dispensadores[j].cantidad == 0)
            {
                char nombre_muy_corto[8];
                strncpy(nombre_muy_corto, b->dispensadores[j].nombre, 7);
                nombre_muy_corto[7] = '\0';
                printf("%s(AGOTADO) ", nombre_muy_corto);
                items_criticos++;
            }
            else if (b->dispensadores[j].cantidad <= UMBRAL_INVENTARIO_BAJO)
            {
                char nombre_muy_corto[8];
                strncpy(nombre_muy_corto, b->dispensadores[j].nombre, 7);
                nombre_muy_corto[7] = '\0';
                printf("%s(%d) ", nombre_muy_corto, b->dispensadores[j].cantidad);
                items_criticos++;
            }
            pthread_mutex_unlock(&b->dispensadores[j].mutex);
        }
        if (items_criticos == 0)
            printf("Ninguno");
        printf("\n\n");

        pthread_mutex_unlock(&b->mutex);
    }

    printf("Presiona Ctrl+C para salir del sistema\n");
}

void mostrar_estado_adaptativo()
{
#ifdef TIOCGWINSZ
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0)
    {
        if (w.ws_col < 120)
        {
            mostrar_estado_compacto();
            return;
        }
    }
#endif
    mostrar_estado_columnar();
}

void generar_orden_especifica(Orden *orden, int id)
{
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
    orden->intentos_asignacion = 0;

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

        datos_compartidos->bandas[banda_id].necesita_reabastecimiento = 0;
        datos_compartidos->bandas[banda_id].ultima_alerta_inventario = 0;

        agregar_log_banda(banda_id, "BANDA REABASTECIDA", 0);
        printf("\n✅ Banda %d reabastecida completamente\n", banda_id + 1);
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

    // Esperar que terminen los hilos
    for (int i = 0; i < datos_compartidos->num_bandas; i++)
    {
        pthread_join(datos_compartidos->bandas[i].hilo, NULL);
    }

    pthread_join(hilo_generador_ordenes, NULL);
    pthread_join(hilo_asignador_ordenes, NULL);
    pthread_join(hilo_monitor_inventario, NULL);

    shm_unlink("/burger_system");
    printf("\nSistema terminado correctamente\n");
    printf("Estadísticas finales:\n");
    printf("- Órdenes generadas: %d\n", datos_compartidos->total_ordenes_generadas);
    printf("- Órdenes completadas: %d\n", datos_compartidos->total_ordenes_procesadas);
    printf("- Órdenes pendientes: %d\n", datos_compartidos->cola_espera.tamano);
    printf("- Configuración de tiempos:\n");
    printf("  • %d segundos por ingrediente\n", datos_compartidos->tiempo_por_ingrediente);
    printf("  • %d segundos entre órdenes\n", datos_compartidos->tiempo_nueva_orden);
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
        if (datos_compartidos->num_bandas > 0)
        {
            int banda = rand() % datos_compartidos->num_bandas;
            datos_compartidos->bandas[banda].pausada = 1;
            agregar_log_banda(banda, "BANDA PAUSADA POR SEÑAL", 0);
        }
        break;
    case SIGUSR2:
        for (int i = 0; i < datos_compartidos->num_bandas; i++)
        {
            if (datos_compartidos->bandas[i].pausada)
            {
                datos_compartidos->bandas[i].pausada = 0;
                pthread_cond_signal(&datos_compartidos->bandas[i].condicion);
                agregar_log_banda(i, "BANDA REANUDADA", 0);
            }
        }
        break;
    case SIGCONT:
    {
        int bandas_reabastecidas = 0;
        for (int i = 0; i < datos_compartidos->num_bandas; i++)
        {
            if (datos_compartidos->bandas[i].necesita_reabastecimiento)
            {
                reabastecer_banda(i);
                bandas_reabastecidas++;
            }
        }
        if (bandas_reabastecidas == 0)
        {
            int banda = rand() % datos_compartidos->num_bandas;
            reabastecer_banda(banda);
        }
        printf("\n📦 Reabastecimiento automático completado\n");
    }
    break;
    }
}

int validar_parametros(int argc, char *argv[], int *num_bandas, int *tiempo_ingrediente, int *tiempo_orden)
{
    *num_bandas = 3;                                  // Valor por defecto
    *tiempo_ingrediente = TIEMPO_DEFAULT_INGREDIENTE; // 2 segundos por defecto
    *tiempo_orden = TIEMPO_DEFAULT_NUEVA_ORDEN;       // 7 segundos por defecto

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
                i++;
            }
            else
            {
                printf("Error: -n requiere un número\n");
                return 0;
            }
        }
        else if (strcmp(argv[i], "-t") == 0 || strcmp(argv[i], "--tiempo-ingrediente") == 0)
        {
            if (i + 1 < argc)
            {
                *tiempo_ingrediente = atoi(argv[i + 1]);
                if (*tiempo_ingrediente <= 0 || *tiempo_ingrediente > 60)
                {
                    printf("Error: Tiempo por ingrediente debe estar entre 1 y 60 segundos\n");
                    return 0;
                }
                i++;
            }
            else
            {
                printf("Error: -t requiere un número (segundos)\n");
                return 0;
            }
        }
        else if (strcmp(argv[i], "-o") == 0 || strcmp(argv[i], "--tiempo-orden") == 0)
        {
            if (i + 1 < argc)
            {
                *tiempo_orden = atoi(argv[i + 1]);
                if (*tiempo_orden <= 0 || *tiempo_orden > 300)
                {
                    printf("Error: Tiempo entre órdenes debe estar entre 1 y 300 segundos\n");
                    return 0;
                }
                i++;
            }
            else
            {
                printf("Error: -o requiere un número (segundos)\n");
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
    printf("-----------------------------------------------------------------\n");
    printf("Uso: ./burger_system [opciones]\n\n");
    printf("Opciones:\n");
    printf("  -n, --bandas <N>           Número de bandas de preparación (1-%d, default: 3)\n", MAX_BANDAS);
    printf("  -t, --tiempo-ingrediente <S> Segundos por ingrediente (1-60, default: %d)\n", TIEMPO_DEFAULT_INGREDIENTE);
    printf("  -o, --tiempo-orden <S>     Segundos entre órdenes (1-300, default: %d)\n", TIEMPO_DEFAULT_NUEVA_ORDEN);
    printf("  -m, --menu                Mostrar menú de hamburguesas disponibles\n");
    printf("  -h, --help                Mostrar esta ayuda\n\n");
    printf("Ejemplos de uso:\n");
    printf("  ./burger_system -n 4                    # 4 bandas, tiempos por defecto\n");
    printf("  ./burger_system -n 2 -t 3 -o 10         # 2 bandas, 3s/ingrediente, 10s entre órdenes\n");
    printf("  ./burger_system -t 1 -o 5               # Tiempos rápidos: 1s/ingrediente, 5s entre órdenes\n");
    printf("  ./burger_system -n 6 -t 5 -o 15         # 6 bandas, preparación lenta\n\n");
    printf("-----------------------------------------------------------------\n");
}

/**
 * @brief Función principal del sistema de hamburguesas
 *
 * Esta es la función principal que coordina todo el funcionamiento del sistema.
 * Realiza las siguientes operaciones en secuencia:
 *
 * 1. VALIDACIÓN: Procesa y valida los parámetros de línea de comandos
 * 2. CONFIGURACIÓN: Establece manejadores de señales del sistema operativo
 * 3. INICIALIZACIÓN: Configura el sistema y la memoria compartida
 * 4. CREACIÓN DE HILOS: Inicia todos los hilos de trabajo del sistema
 * 5. MONITOREO: Ejecuta el bucle principal de visualización del estado
 * 6. LIMPIEZA: Libera recursos al terminar
 *
 * @param argc Número de argumentos de línea de comandos
 * @param argv Array de argumentos de línea de comandos
 * @return 0 si el sistema termina correctamente, 1 en caso de error
 *
 * @note El sistema responde a las siguientes señales:
 *       - SIGINT/SIGTERM: Terminación limpia
 *       - SIGUSR1: Pausar banda aleatoria
 *       - SIGUSR2: Reanudar bandas pausadas
 *       - SIGCONT: Reabastecimiento automático
 *
 * @note El sistema calcula automáticamente estimaciones de rendimiento
 *       y advierte sobre posibles saturaciones
 *
 * @warning Si falla la creación de hilos, el programa termina con exit(1)
 * @warning El sistema debe ser terminado con Ctrl+C para limpieza adecuada
 */
int main(int argc, char *argv[])
{
    int num_bandas, tiempo_ingrediente, tiempo_orden;

    // Validar y procesar parámetros de línea de comandos
    if (!validar_parametros(argc, argv, &num_bandas, &tiempo_ingrediente, &tiempo_orden))
    {
        return 0;
    }

    // Configurar manejadores de señales del sistema operativo
    signal(SIGINT, manejar_senal);  // Ctrl+C
    signal(SIGTERM, manejar_senal); // Terminación del sistema
    signal(SIGUSR1, manejar_senal); // Pausar banda aleatoria
    signal(SIGUSR2, manejar_senal); // Reanudar bandas pausadas
    signal(SIGCONT, manejar_senal); // Reabastecimiento automático

    // Inicializar generador de números aleatorios y sistema
    srand(time(NULL));
    inicializar_sistema(num_bandas, tiempo_ingrediente, tiempo_orden);

    // Crear hilos de trabajo para cada banda de preparación
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

    // Crear hilos del sistema principal
    pthread_create(&hilo_generador_ordenes, NULL, generador_ordenes, NULL);
    pthread_create(&hilo_asignador_ordenes, NULL, asignador_ordenes, NULL);
    pthread_create(&hilo_monitor_inventario, NULL, monitor_inventario, NULL);

    // Mostrar información de inicio del sistema
    printf("Sistema iniciado exitosamente con %d bandas\n", num_bandas);
    printf("Cola FIFO implementada - Sin rechazos por inventario\n");
    printf("Asignación inteligente activada\n");
    printf("Monitor de inventario ejecutándose\n");
    printf("⏱️  CONFIGURACIÓN DE TIEMPOS:\n");
    printf("   • %d segundos por ingrediente\n", tiempo_ingrediente);
    printf("   • %d segundos entre órdenes nuevas\n", tiempo_orden);

    // Calcular estadísticas estimadas de rendimiento del sistema
    float hamburguesa_promedio = 6.5;                                                  // Promedio de ingredientes por hamburguesa
    float tiempo_promedio_preparacion = hamburguesa_promedio * tiempo_ingrediente + 1; // +1 segundo final
    float ordenes_por_minuto = 60.0 / tiempo_orden;
    float capacidad_teorica = (60.0 / tiempo_promedio_preparacion) * num_bandas;

    printf("📊 ESTIMACIONES DE RENDIMIENTO:\n");
    printf("   • Tiempo promedio por hamburguesa: %.1f segundos\n", tiempo_promedio_preparacion);
    printf("   • Órdenes generadas por minuto: %.1f\n", ordenes_por_minuto);
    printf("   • Capacidad teórica del sistema: %.1f hamburguesas/minuto\n", capacidad_teorica);

    // Analizar si el sistema podría saturarse
    if (ordenes_por_minuto > capacidad_teorica)
    {
        printf("⚠️  ADVERTENCIA: El sistema podría saturarse (cola crecerá)\n");
    }
    else
    {
        printf("✅ CONFIGURACIÓN: Sistema balanceado para esta carga\n");
    }

    printf("PID del proceso: %d\n\n", getpid());

    // Pausa inicial para estabilización del sistema
    sleep(3);

    // Bucle principal de visualización del estado del sistema
    while (datos_compartidos->sistema_activo)
    {
        mostrar_estado_adaptativo();
        sleep(2);
    }

    // Limpieza final de recursos del sistema
    limpiar_sistema();
    return 0;
}

/**
 * @}
 *
 * @page conclusiones Conclusiones del Sistema
 *
 * Este sistema de simulación de hamburguesas demuestra las siguientes características técnicas:
 *
 * - **PROGRAMACIÓN MULTI-HILO**: Utiliza hilos POSIX para operaciones concurrentes
 * - **SINCRONIZACIÓN AVANZADA**: Mutexes y variables de condición para acceso seguro
 * - **MEMORIA COMPARTIDA**: Comunicación eficiente entre procesos
 * - **GESTIÓN DE RECURSOS**: Sistema de inventario con alertas automáticas
 * - **INTERFAZ ADAPTATIVA**: Visualización que se adapta al tamaño de pantalla
 * - **CONFIGURACIÓN FLEXIBLE**: Parámetros ajustables en tiempo de ejecución
 *
 * El sistema está diseñado para ser robusto, eficiente y fácil de mantener,
 * proporcionando una base sólida para aplicaciones de simulación industrial.
 *
 * @author [Tu Nombre]
 * @date [Fecha de Finalización]
 * @version 1.0
 */