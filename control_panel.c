/**
 * @file control_panel.c
 * @brief Panel de Control Interactivo para el Sistema de Hamburguesas
 * @author Angelo Zurita
 * @date 01/09/2025
 * @version 1.0
 *
 * @section descripcion Descripción del Panel de Control
 *
 * Este panel de control proporciona una interfaz interactiva y en tiempo real
 * para monitorear y controlar el sistema de hamburguesas. Utiliza la biblioteca
 * ncurses para crear una interfaz gráfica en terminal que permite:
 *
 * - Monitoreo en tiempo real del estado de todas las bandas
 * - Control directo de bandas (pausar/reanudar)
 * - Gestión de inventario y reabastecimiento
 * - Visualización de estadísticas del sistema
 * - Navegación intuitiva entre diferentes vistas
 *
 * @section arquitectura Arquitectura del Panel de Control
 *
 * El panel se conecta al sistema principal a través de memoria compartida POSIX
 * y proporciona las siguientes funcionalidades principales:
 *
 * 1. INTERFAZ GENERAL: Vista resumida de todas las bandas y estadísticas
 * 2. DETALLE DE BANDA: Información específica de una banda seleccionada
 * 3. INVENTARIO GLOBAL: Resumen de inventarios por ingrediente
 * 4. INVENTARIO DE BANDA: Gestión detallada del inventario de una banda
 * 5. MODO ABASTECIMIENTO: Operaciones masivas de reabastecimiento
 *
 * @section interfaz Características de la Interfaz
 *
 * - **INTERFAZ ADAPTATIVA**: Se ajusta automáticamente al tamaño de pantalla
 * - **COLORES INTELIGENTES**: Sistema de colores para alertas y estados
 * - **NAVEGACIÓN INTUITIVA**: Controles de teclado consistentes y fáciles de usar
 * - **ACTUALIZACIÓN EN TIEMPO REAL**: Refresco automático de información
 * - **MÚLTIPLES VISTAS**: Diferentes modos de visualización según necesidades
 *
 * @section controles Controles del Usuario
 *
 * - **NAVEGACIÓN**: Flechas arriba/abajo, TAB para cambiar vistas
 * - **CONTROL**: ESPACIO para pausar/reanudar, R para reabastecer
 * - **INVENTARIO**: +/- para ajustar cantidades, F para llenar
 * - **ABASTECIMIENTO**: 1-5 para opciones, A/C/E para operaciones rápidas
 * - **SISTEMA**: H para ayuda, Q para salir
 *
 * @section dependencias Dependencias del Sistema
 *
 * - ncurses: Para interfaz gráfica en terminal
 * - pthread: Para sincronización con el sistema principal
 * - librt: Para acceso a memoria compartida POSIX
 * - Sistema operativo compatible con POSIX
 *
 * @section compilacion Compilación
 *
 * Para compilar el panel de control:
 * @code
 * make control_panel
 * @endcode
 *
 * @section ejecucion Ejecución
 *
 * Uso:
 * @code
 * # Primero ejecutar el sistema principal
 * ./burger_system -n 4 &
 *
 * # Luego ejecutar el panel de control
 * ./control_panel
 * @endcode
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
#include <signal.h>
#include <ncurses.h>
#include <sys/types.h>
#include <time.h>

/**
 * @defgroup constantes_panel Constantes del Panel de Control
 * @{
 *
 * Las constantes deben coincidir exactamente con las del sistema principal
 * para garantizar compatibilidad en la memoria compartida.
 */

/** @brief Número máximo de bandas que puede manejar el panel */
#define MAX_BANDAS 10

/** @brief Número máximo de ingredientes diferentes en el sistema */
#define MAX_INGREDIENTES 15

/** @brief Capacidad máxima de la cola de órdenes */
#define MAX_ORDENES 100

/** @brief Longitud máxima del nombre de un ingrediente */
#define MAX_NOMBRE_INGREDIENTE 30

/** @brief Número máximo de entradas de log por banda */
#define MAX_LOGS_POR_BANDA 10

/** @brief Capacidad máxima de cada dispensador de ingredientes */
#define CAPACIDAD_DISPENSADOR 10

/** @brief Número de tipos de hamburguesas disponibles */
#define NUM_TIPOS_HAMBURGUESA 6

/** @brief Umbral para considerar inventario bajo */
#define UMBRAL_INVENTARIO_BAJO 2

/** @} */

/**
 * @defgroup estructuras_panel Estructuras de Datos del Panel de Control
 * @{
 *
 * IMPORTANTE: Estas estructuras deben ser IDÉNTICAS a las del sistema principal
 * para garantizar la correcta interpretación de la memoria compartida.
 * Cualquier cambio en el sistema principal debe reflejarse aquí.
 */

/**
 * @brief Estructura que representa un ingrediente en el inventario
 *
 * Mantiene la misma estructura que el sistema principal para compatibilidad
 * en memoria compartida.
 */
typedef struct
{
    /** @brief Nombre del ingrediente */
    char nombre[MAX_NOMBRE_INGREDIENTE];

    /** @brief Cantidad disponible en el dispensador */
    int cantidad;

    /** @brief Mutex para acceso thread-safe al inventario */
    pthread_mutex_t mutex;
} Ingrediente;

/**
 * @brief Estructura que define un tipo de hamburguesa disponible
 *
 * Contiene la receta completa y precio de cada tipo de hamburguesa.
 * Debe coincidir exactamente con la estructura del sistema principal.
 */
typedef struct
{
    /** @brief Nombre comercial de la hamburguesa */
    char nombre[50];

    /** @brief Lista de ingredientes requeridos para la receta */
    char ingredientes[10][MAX_NOMBRE_INGREDIENTE];

    /** @brief Número total de ingredientes en la receta */
    int num_ingredientes;

    /** @brief Precio de venta de la hamburguesa */
    float precio;
} TipoHamburguesa;

/**
 * @brief Estructura para entradas de log del sistema
 *
 * Mantiene un historial de eventos y actividades de cada banda.
 * Incluye timestamp y clasificación de alertas.
 */
typedef struct
{
    /** @brief Mensaje descriptivo del evento */
    char mensaje[100];

    /** @brief Timestamp Unix del momento del evento */
    time_t timestamp;

    /** @brief Flag que indica si es una alerta crítica */
    int es_alerta;
} LogEntry;

/**
 * @brief Estructura que representa una orden de hamburguesa
 *
 * Contiene toda la información necesaria para procesar una orden,
 * incluyendo estado de procesamiento y metadatos de seguimiento.
 */
typedef struct
{
    /** @brief Identificador único de la orden */
    int id_orden;

    /** @brief Índice del tipo de hamburguesa en el menú */
    int tipo_hamburguesa;

    /** @brief Nombre de la hamburguesa solicitada */
    char nombre_hamburguesa[50];

    /** @brief Lista de ingredientes específicos para esta orden */
    char ingredientes_solicitados[MAX_INGREDIENTES][MAX_NOMBRE_INGREDIENTE];

    /** @brief Número total de ingredientes en la orden */
    int num_ingredientes;

    /** @brief Timestamp de creación de la orden */
    time_t tiempo_creacion;

    /** @brief Paso actual en el proceso de preparación */
    int paso_actual;

    /** @brief Flag que indica si la orden está completada */
    int completada;

    /** @brief ID de la banda asignada para procesar la orden */
    int asignada_a_banda;

    /** @brief Contador de intentos de asignación a bandas */
    int intentos_asignacion;
} Orden;

/**
 * @brief Estructura que representa una banda de preparación
 *
 * Contiene el estado completo de una banda, incluyendo su inventario,
 * logs de actividad y mecanismos de control. Debe coincidir exactamente
 * con la estructura del sistema principal.
 */
typedef struct
{
    /** @brief Identificador único de la banda */
    int id;

    /** @brief Flag que indica si la banda está operativa */
    int activa;

    /** @brief Flag que indica si la banda está pausada */
    int pausada;

    /** @brief Contador total de hamburguesas procesadas */
    int hamburguesas_procesadas;

    /** @brief Flag que indica si está procesando una orden */
    int procesando_orden;

    /** @brief Orden que está siendo procesada actualmente */
    Orden orden_actual;

    /** @brief Array de dispensadores de ingredientes de la banda */
    Ingrediente dispensadores[MAX_INGREDIENTES];

    /** @brief Historial de logs de actividades de la banda */
    LogEntry logs[MAX_LOGS_POR_BANDA];

    /** @brief Número actual de entradas de log */
    int num_logs;

    /** @brief Hilo POSIX que ejecuta la lógica de la banda */
    pthread_t hilo;

    /** @brief Mutex para acceso exclusivo a los datos de la banda */
    pthread_mutex_t mutex;

    /** @brief Variable de condición para sincronización */
    pthread_cond_t condicion;

    /** @brief Descripción del estado actual de la banda */
    char estado_actual[100];

    /** @brief Nombre del ingrediente que se está procesando */
    char ingrediente_actual[50];

    /** @brief Flag que indica si necesita reabastecimiento */
    int necesita_reabastecimiento;

    /** @brief Timestamp de la última alerta de inventario */
    time_t ultima_alerta_inventario;
} Banda;

/**
 * @brief Estructura que implementa la cola FIFO de órdenes
 *
 * Gestiona las órdenes pendientes de asignación a bandas.
 * Implementa una cola circular thread-safe con sincronización completa.
 */
typedef struct
{
    /** @brief Array circular que almacena las órdenes */
    Orden ordenes[MAX_ORDENES];

    /** @brief Índice del primer elemento en la cola */
    int frente;

    /** @brief Índice de la siguiente posición libre */
    int atras;

    /** @brief Número actual de órdenes en la cola */
    int tamano;

    /** @brief Mutex para acceso exclusivo a la cola */
    pthread_mutex_t mutex;

    /** @brief Variable de condición para cola no vacía */
    pthread_cond_t no_vacia;

    /** @brief Variable de condición para cola no llena */
    pthread_cond_t no_llena;
} ColaFIFO;

/**
 * @brief Estructura principal de datos compartidos del sistema
 *
 * Contiene toda la información del sistema que se comparte entre
 * el sistema principal y el panel de control a través de memoria compartida.
 */
typedef struct
{
    /** @brief Array de todas las bandas del sistema */
    Banda bandas[MAX_BANDAS];

    /** @brief Cola FIFO de órdenes pendientes */
    ColaFIFO cola_espera;

    /** @brief Número actual de bandas activas */
    int num_bandas;

    /** @brief Flag que indica si el sistema está operativo */
    int sistema_activo;

    /** @brief Contador total de órdenes procesadas */
    int total_ordenes_procesadas;

    /** @brief Contador total de órdenes generadas */
    int total_ordenes_generadas;

    /** @brief Mutex global para operaciones del sistema */
    pthread_mutex_t mutex_global;

    /** @brief Variable de condición para nuevas órdenes */
    pthread_cond_t nueva_orden;
} DatosCompartidos;

/**
 * @defgroup variables_globales Variables Globales del Panel de Control
 * @{
 */

/** @brief Puntero a la estructura de datos compartidos del sistema principal */
DatosCompartidos *datos_compartidos;

/** @brief Ventana principal que muestra la vista general del sistema */
WINDOW *win_main;

/** @brief Ventana que muestra detalles de banda o inventario */
WINDOW *win_banda_detail;

/** @brief Ventana que muestra los comandos disponibles según el modo actual */
WINDOW *win_commands;

/** @brief Ventana que muestra el estado del sistema y la vista actual */
WINDOW *win_status;

/** @brief Índice de la banda actualmente seleccionada (0 a num_bandas-1) */
int banda_seleccionada = 0;

/** @brief Índice del ingrediente seleccionado en modo inventario (0 a MAX_INGREDIENTES-1) */
int ingrediente_seleccionado = 0;

/** @brief Modo de vista actual del panel de control
 *
 * Valores posibles:
 * - 0: Vista general (resumen de todas las bandas)
 * - 1: Detalle de banda específica
 * - 2: Inventario global (resumen por ingrediente)
 * - 3: Inventario de banda específica (editable)
 * - 4: Modo abastecimiento (operaciones masivas)
 */
int modo_vista = 0;

/** @brief Flag que indica si el panel está en modo abastecimiento */
int en_modo_abastecimiento = 0;

/** @} */

/**
 * @defgroup datos_estaticos Datos Estáticos del Sistema
 * @{
 */

/**
 * @brief Menú completo de hamburguesas disponibles
 *
 * Define todos los tipos de hamburguesas que se pueden preparar en el sistema,
 * incluyendo sus ingredientes específicos, cantidad de ingredientes y precios.
 * Cada hamburguesa tiene una receta única que determina el orden de preparación.
 *
 * @note Debe coincidir exactamente con el menú del sistema principal
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
 * @brief Lista completa de ingredientes base disponibles
 *
 * Define todos los ingredientes que pueden estar presentes en cualquier
 * hamburguesa del sistema. Cada banda tiene dispensadores para todos estos
 * ingredientes, permitiendo preparar cualquier tipo de hamburguesa del menú.
 *
 * @note Debe coincidir exactamente con la lista del sistema principal
 */
char ingredientes_base[MAX_INGREDIENTES][MAX_NOMBRE_INGREDIENTE] = {
    "pan_inferior", "pan_superior", "carne", "queso", "tomate",
    "lechuga", "cebolla", "bacon", "mayonesa", "jalapenos",
    "aguacate", "vegetal", "salsa_bbq", "salsa_picante", "pepinillos"};

/** @} */

/**
 * @defgroup prototipos_panel Prototipos de Funciones del Panel de Control
 * @{
 *
 * Declaraciones de todas las funciones del panel de control, organizadas
 * por categorías funcionales para facilitar la comprensión del código.
 */

// ============================================================================
// FUNCIONES DE INICIALIZACIÓN Y CONFIGURACIÓN
// ============================================================================

/**
 * @brief Inicializa la biblioteca ncurses y configura la interfaz gráfica
 * @note Configura colores, ventanas y parámetros de la terminal
 */
void inicializar_ncurses();

/**
 * @brief Conecta el panel con el sistema principal a través de memoria compartida
 * @warning Si no puede conectar, el programa termina con mensaje de error
 */
void conectar_memoria_compartida();

// ============================================================================
// FUNCIONES DE VISUALIZACIÓN PRINCIPAL
// ============================================================================

/**
 * @brief Muestra la vista general del sistema con resumen de todas las bandas
 * @note Incluye estadísticas generales y estado de cada banda
 */
void mostrar_interfaz_general();

/**
 * @brief Muestra información detallada de la banda seleccionada
 * @note Incluye estado, orden actual, inventario crítico y logs
 */
void mostrar_detalle_banda();

/**
 * @brief Muestra resumen global del inventario por ingrediente
 * @note Agrupa información de todas las bandas por tipo de ingrediente
 */
void mostrar_inventario_global();

/**
 * @brief Muestra inventario completo de la banda seleccionada
 * @note Permite edición interactiva de cantidades de ingredientes
 */
void mostrar_inventario_banda();

/**
 * @brief Muestra el modo de abastecimiento con opciones masivas
 * @note Incluye opciones para reabastecer bandas específicas o todas
 */
void mostrar_modo_abastecimiento();

/**
 * @brief Muestra los comandos disponibles según el modo de vista actual
 * @note Se actualiza dinámicamente según el contexto del usuario
 */
void mostrar_comandos_disponibles();

// ============================================================================
// FUNCIONES DE CONTROL Y PROCESAMIENTO
// ============================================================================

/**
 * @brief Procesa un comando de teclado del usuario
 * @param ch Código del carácter o tecla especial presionada
 * @note Maneja navegación, control de bandas y operaciones de inventario
 */
void procesar_comando(int ch);

/**
 * @brief Pausa o reanuda una banda específica
 * @param banda_id ID de la banda a pausar/reanudar
 * @note Cambia el estado de la banda y muestra mensaje de confirmación
 */
void pausar_reanudar_banda(int banda_id);

/**
 * @brief Reabastece completamente el inventario de una banda
 * @param banda_id ID de la banda a reabastecer
 * @note Llena todos los dispensadores a capacidad máxima
 */
void reabastecer_banda_completa(int banda_id);

/**
 * @brief Reabastece un ingrediente específico de una banda
 * @param banda_id ID de la banda
 * @param ingrediente_id ID del ingrediente a reabastecer
 * @note Llena solo el ingrediente especificado
 */
void reabastecer_ingrediente_especifico(int banda_id, int ingrediente_id);

// ============================================================================
// FUNCIONES DE NAVEGACIÓN Y SELECCIÓN
// ============================================================================

/**
 * @brief Cambia la banda seleccionada en la dirección especificada
 * @param direccion +1 para siguiente banda, -1 para anterior
 * @note Implementa navegación circular entre bandas
 */
void cambiar_banda_seleccionada(int direccion);

/**
 * @brief Cambia el ingrediente seleccionado en modo inventario
 * @param direccion +1 para siguiente ingrediente, -1 para anterior
 * @note Implementa navegación circular entre ingredientes
 */
void cambiar_ingrediente_seleccionado(int direccion);

// ============================================================================
// FUNCIONES DE UTILIDAD Y AYUDA
// ============================================================================

/**
 * @brief Muestra un mensaje temporal en la parte inferior de la pantalla
 * @param mensaje Texto del mensaje a mostrar
 * @note El mensaje se muestra por 1.5 segundos y luego se borra
 */
void mostrar_mensaje_temporal(const char *mensaje);

/**
 * @brief Muestra la ayuda detallada del panel de control
 * @note Incluye todos los controles disponibles y ejemplos de uso
 */
void mostrar_ayuda_detallada();

/**
 * @brief Limpia todos los recursos de ncurses y restaura la terminal
 * @note Debe ser llamada antes de terminar el programa
 */
void limpiar_ncurses();

/**
 * @brief Determina el color apropiado para mostrar un ingrediente según su cantidad
 * @param cantidad Cantidad actual del ingrediente
 * @return Cadena con el código de color apropiado
 * @note Verde para normal, amarillo para crítico, rojo para agotado
 */
const char *obtener_color_ingrediente(int cantidad);

/**
 * @brief Muestra el menú de opciones de abastecimiento
 * @note Función auxiliar para mostrar_modo_abastecimiento()
 */
void mostrar_menu_abastecimiento();

/** @} */

// ================================================================
// INICIALIZACION Y CONFIGURACION
// ================================================================

void inicializar_ncurses()
{
    initscr();
    cbreak();
    noecho();
    nodelay(stdscr, TRUE);
    keypad(stdscr, TRUE);
    curs_set(0);

    // Configurar colores
    if (has_colors())
    {
        start_color();
        init_pair(1, COLOR_GREEN, COLOR_BLACK);   // Verde - Normal
        init_pair(2, COLOR_YELLOW, COLOR_BLACK);  // Amarillo - Advertencia
        init_pair(3, COLOR_RED, COLOR_BLACK);     // Rojo - Critico/Error
        init_pair(4, COLOR_CYAN, COLOR_BLACK);    // Cian - Encabezados
        init_pair(5, COLOR_WHITE, COLOR_BLUE);    // Seleccion
        init_pair(6, COLOR_MAGENTA, COLOR_BLACK); // Destacado
        init_pair(7, COLOR_BLACK, COLOR_WHITE);   // Invertido
        init_pair(8, COLOR_WHITE, COLOR_GREEN);   // Seleccion verde
        init_pair(9, COLOR_BLACK, COLOR_YELLOW);  // Seleccion amarilla
    }

    // Calcular dimensiones
    int altura, ancho;
    getmaxyx(stdscr, altura, ancho);

    // Crear ventanas principales
    win_main = newwin(altura - 8, ancho - 2, 1, 1);
    win_banda_detail = newwin(altura - 8, ancho / 2, 1, ancho / 2);
    win_commands = newwin(6, ancho / 2, altura - 7, 1);
    win_status = newwin(6, ancho / 2, altura - 7, ancho / 2);

    // Habilitar teclas especiales
    keypad(win_main, TRUE);
    keypad(win_banda_detail, TRUE);
    keypad(win_commands, TRUE);
    keypad(win_status, TRUE);

    refresh();
}

void conectar_memoria_compartida()
{
    int shm_fd = shm_open("/burger_system", O_RDWR, 0666);
    if (shm_fd == -1)
    {
        endwin();
        printf("Error: No se pudo conectar con el sistema principal.\n");
        printf("   Asegurate de que ./burger_system este ejecutandose.\n");
        printf("   Uso: ./burger_system -n 4 &\n");
        printf("        ./control_panel\n");
        exit(1);
    }

    datos_compartidos = mmap(0, sizeof(DatosCompartidos), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);

    if (datos_compartidos == MAP_FAILED)
    {
        endwin();
        perror("Error mapeando memoria compartida");
        exit(1);
    }

    close(shm_fd);
}

// ================================================================
// FUNCIONES DE VISUALIZACION
// ================================================================

void mostrar_interfaz_general()
{
    werase(win_main);

    if (has_colors())
        wattron(win_main, COLOR_PAIR(4));
    wborder(win_main, '|', '|', '-', '-', '+', '+', '+', '+');
    mvwprintw(win_main, 0, 2, " SISTEMA DE HAMBURGUESAS - VISTA GENERAL ");
    if (has_colors())
        wattroff(win_main, COLOR_PAIR(4));

    // Estadisticas generales
    mvwprintw(win_main, 2, 2, "ESTADISTICAS DEL SISTEMA:");
    mvwprintw(win_main, 3, 4, "* Ordenes generadas:  %d", datos_compartidos->total_ordenes_generadas);
    mvwprintw(win_main, 4, 4, "* Ordenes procesadas: %d", datos_compartidos->total_ordenes_procesadas);
    mvwprintw(win_main, 5, 4, "* Ordenes en cola:    %d", datos_compartidos->cola_espera.tamano);
    mvwprintw(win_main, 6, 4, "* Bandas activas:     %d", datos_compartidos->num_bandas);

    // Eficiencia
    float eficiencia = 0;
    if (datos_compartidos->total_ordenes_generadas > 0)
    {
        eficiencia = (float)datos_compartidos->total_ordenes_procesadas / datos_compartidos->total_ordenes_generadas * 100;
    }
    mvwprintw(win_main, 7, 4, "* Eficiencia:         %.1f%%", eficiencia);

    // Estado de bandas
    mvwprintw(win_main, 9, 2, "ESTADO DE BANDAS:");

    for (int i = 0; i < datos_compartidos->num_bandas; i++)
    {
        Banda *banda = &datos_compartidos->bandas[i];
        int linea = 11 + i;

        // Determinar color segun estado
        int color_pair = 1; // Verde por defecto
        char estado_icono[10] = "[OK]";

        pthread_mutex_lock(&banda->mutex);

        if (!banda->activa)
        {
            color_pair = 3;
            strcpy(estado_icono, "[X]");
        }
        else if (banda->pausada)
        {
            color_pair = 2;
            strcpy(estado_icono, "[PAUSE]");
        }
        else if (banda->necesita_reabastecimiento)
        {
            color_pair = 2;
            strcpy(estado_icono, "[!]");
        }

        if (i == banda_seleccionada)
        {
            color_pair = 5; // Destacar banda seleccionada
        }

        if (has_colors())
            wattron(win_main, COLOR_PAIR(color_pair));

        mvwprintw(win_main, linea, 4, "%s BANDA %d:", estado_icono, i + 1);

        if (banda->procesando_orden)
        {
            mvwprintw(win_main, linea, 18, "Procesando %s (#%d) - %d/%d",
                      banda->orden_actual.nombre_hamburguesa,
                      banda->orden_actual.id_orden,
                      banda->orden_actual.paso_actual,
                      banda->orden_actual.num_ingredientes);
        }
        else
        {
            mvwprintw(win_main, linea, 18, "%s", banda->estado_actual);
        }

        mvwprintw(win_main, linea, 55, "Completadas: %d", banda->hamburguesas_procesadas);

        if (has_colors())
            wattroff(win_main, COLOR_PAIR(color_pair));

        pthread_mutex_unlock(&banda->mutex);
    }

    // Alertas de inventario
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
        if (has_colors())
            wattron(win_main, COLOR_PAIR(3));
        mvwprintw(win_main, 11 + datos_compartidos->num_bandas + 1, 2,
                  "ALERTAS: %d bandas necesitan reabastecimiento", bandas_con_alertas);
        if (has_colors())
            wattroff(win_main, COLOR_PAIR(3));
    }

    wrefresh(win_main);
}

void mostrar_detalle_banda()
{
    werase(win_banda_detail);

    if (has_colors())
        wattron(win_banda_detail, COLOR_PAIR(6));
    wborder(win_banda_detail, '|', '|', '-', '-', '+', '+', '+', '+');
    mvwprintw(win_banda_detail, 0, 2, " DETALLE BANDA %d ", banda_seleccionada + 1);
    if (has_colors())
        wattroff(win_banda_detail, COLOR_PAIR(6));

    if (banda_seleccionada >= datos_compartidos->num_bandas)
    {
        mvwprintw(win_banda_detail, 2, 2, "Banda no valida");
        wrefresh(win_banda_detail);
        return;
    }

    Banda *banda = &datos_compartidos->bandas[banda_seleccionada];
    pthread_mutex_lock(&banda->mutex);

    // Estado actual
    mvwprintw(win_banda_detail, 2, 2, "ESTADO:");
    if (banda->pausada)
    {
        if (has_colors())
            wattron(win_banda_detail, COLOR_PAIR(2));
        mvwprintw(win_banda_detail, 3, 4, "[PAUSE] PAUSADA");
        if (has_colors())
            wattroff(win_banda_detail, COLOR_PAIR(2));
    }
    else if (!banda->activa)
    {
        if (has_colors())
            wattron(win_banda_detail, COLOR_PAIR(3));
        mvwprintw(win_banda_detail, 3, 4, "[X] INACTIVA");
        if (has_colors())
            wattroff(win_banda_detail, COLOR_PAIR(3));
    }
    else
    {
        if (has_colors())
            wattron(win_banda_detail, COLOR_PAIR(1));
        mvwprintw(win_banda_detail, 3, 4, "[OK] ACTIVA");
        if (has_colors())
            wattroff(win_banda_detail, COLOR_PAIR(1));
    }

    mvwprintw(win_banda_detail, 4, 4, "Estado: %s", banda->estado_actual);

    // Orden actual
    mvwprintw(win_banda_detail, 6, 2, "ORDEN ACTUAL:");
    if (banda->procesando_orden)
    {
        mvwprintw(win_banda_detail, 7, 4, "* ID: #%d", banda->orden_actual.id_orden);
        mvwprintw(win_banda_detail, 8, 4, "* Tipo: %s", banda->orden_actual.nombre_hamburguesa);
        mvwprintw(win_banda_detail, 9, 4, "* Progreso: %d/%d ingredientes",
                  banda->orden_actual.paso_actual, banda->orden_actual.num_ingredientes);
        if (strlen(banda->ingrediente_actual) > 0)
        {
            mvwprintw(win_banda_detail, 10, 4, "* Agregando: %s", banda->ingrediente_actual);
        }
    }
    else
    {
        mvwprintw(win_banda_detail, 7, 4, "Sin orden asignada");
    }

    // Estadisticas
    mvwprintw(win_banda_detail, 12, 2, "ESTADISTICAS:");
    mvwprintw(win_banda_detail, 13, 4, "* Hamburguesas procesadas: %d", banda->hamburguesas_procesadas);

    // Inventario critico
    mvwprintw(win_banda_detail, 15, 2, "INVENTARIO CRITICO:");
    int items_criticos = 0;
    int linea_inv = 16;

    for (int j = 0; j < MAX_INGREDIENTES && items_criticos < 8; j++)
    {
        pthread_mutex_lock(&banda->dispensadores[j].mutex);
        int cantidad = banda->dispensadores[j].cantidad;

        if (cantidad == 0 || cantidad <= UMBRAL_INVENTARIO_BAJO)
        {
            int color = (cantidad == 0) ? 3 : 2;
            if (has_colors())
                wattron(win_banda_detail, COLOR_PAIR(color));

            char nombre_corto[15];
            strncpy(nombre_corto, banda->dispensadores[j].nombre, 14);
            nombre_corto[14] = '\0';

            mvwprintw(win_banda_detail, linea_inv, 4, "* %-14s: %2d %s",
                      nombre_corto, cantidad,
                      cantidad == 0 ? "[AGOTADO]" : "[CRITICO]");

            if (has_colors())
                wattroff(win_banda_detail, COLOR_PAIR(color));
            linea_inv++;
            items_criticos++;
        }
        pthread_mutex_unlock(&banda->dispensadores[j].mutex);
    }

    if (items_criticos == 0)
    {
        if (has_colors())
            wattron(win_banda_detail, COLOR_PAIR(1));
        mvwprintw(win_banda_detail, linea_inv, 4, "[OK] Inventario en niveles normales");
        if (has_colors())
            wattroff(win_banda_detail, COLOR_PAIR(1));
    }

    pthread_mutex_unlock(&banda->mutex);
    wrefresh(win_banda_detail);
}

void mostrar_inventario_banda()
{
    werase(win_banda_detail);

    if (has_colors())
        wattron(win_banda_detail, COLOR_PAIR(4));
    wborder(win_banda_detail, '|', '|', '-', '-', '+', '+', '+', '+');
    mvwprintw(win_banda_detail, 0, 2, " INVENTARIO BANDA %d ", banda_seleccionada + 1);
    if (has_colors())
        wattroff(win_banda_detail, COLOR_PAIR(4));

    if (banda_seleccionada >= datos_compartidos->num_bandas)
    {
        mvwprintw(win_banda_detail, 2, 2, "Banda no valida");
        wrefresh(win_banda_detail);
        return;
    }

    Banda *banda = &datos_compartidos->bandas[banda_seleccionada];

    mvwprintw(win_banda_detail, 2, 2, "INVENTARIO COMPLETO:");
    mvwprintw(win_banda_detail, 3, 2, "Use ^/v para navegar, +/- para ajustar");

    for (int i = 0; i < MAX_INGREDIENTES; i++)
    {
        int linea = 5 + i;
        if (linea > getmaxy(win_banda_detail) - 3)
            break; // No exceder ventana

        pthread_mutex_lock(&banda->dispensadores[i].mutex);
        int cantidad = banda->dispensadores[i].cantidad;

        char nombre_corto[15];
        strncpy(nombre_corto, banda->dispensadores[i].nombre, 14);
        nombre_corto[14] = '\0';

        // Determinar color y selección
        int color = 1; // Verde por defecto
        if (cantidad == 0)
            color = 3; // Rojo
        else if (cantidad <= UMBRAL_INVENTARIO_BAJO)
            color = 2; // Amarillo

        // Destacar ingrediente seleccionado
        if (i == ingrediente_seleccionado)
        {
            if (has_colors())
                wattron(win_banda_detail, COLOR_PAIR(5));
            mvwprintw(win_banda_detail, linea, 2, "> %-14s: %2d/%2d %s",
                      nombre_corto, cantidad, CAPACIDAD_DISPENSADOR,
                      cantidad == 0 ? "[AGOTADO]" : (cantidad <= UMBRAL_INVENTARIO_BAJO ? "[CRITICO]" : ""));
            if (has_colors())
                wattroff(win_banda_detail, COLOR_PAIR(5));
        }
        else
        {
            if (has_colors())
                wattron(win_banda_detail, COLOR_PAIR(color));
            mvwprintw(win_banda_detail, linea, 2, "  %-14s: %2d/%2d %s",
                      nombre_corto, cantidad, CAPACIDAD_DISPENSADOR,
                      cantidad == 0 ? "[AGOTADO]" : (cantidad <= UMBRAL_INVENTARIO_BAJO ? "[CRITICO]" : ""));
            if (has_colors())
                wattroff(win_banda_detail, COLOR_PAIR(color));
        }

        pthread_mutex_unlock(&banda->dispensadores[i].mutex);
    }

    // Mostrar controles
    int linea_controles = MAX_INGREDIENTES + 7;
    if (has_colors())
        wattron(win_banda_detail, COLOR_PAIR(6));
    mvwprintw(win_banda_detail, linea_controles, 2, "CONTROLES:");
    mvwprintw(win_banda_detail, linea_controles + 1, 2, "+ : Añadir 1 unidad");
    mvwprintw(win_banda_detail, linea_controles + 2, 2, "- : Quitar 1 unidad");
    mvwprintw(win_banda_detail, linea_controles + 3, 2, "F : Llenar completamente");
    if (has_colors())
        wattroff(win_banda_detail, COLOR_PAIR(6));

    wrefresh(win_banda_detail);
}

void mostrar_modo_abastecimiento()
{
    werase(win_banda_detail);

    if (has_colors())
        wattron(win_banda_detail, COLOR_PAIR(6));
    wborder(win_banda_detail, '|', '|', '-', '-', '+', '+', '+', '+');
    mvwprintw(win_banda_detail, 0, 2, " MODO ABASTECIMIENTO ");
    if (has_colors())
        wattroff(win_banda_detail, COLOR_PAIR(6));

    mvwprintw(win_banda_detail, 2, 2, "OPCIONES DE ABASTECIMIENTO:");

    // Mostrar opciones
    const char *opciones[] = {
        "1. Reabastecer banda seleccionada",
        "2. Reabastecer TODAS las bandas",
        "3. Reabastecer solo ingredientes criticos",
        "4. Reabastecer ingredientes agotados",
        "5. Modo personalizado (ingrediente por ingrediente)"};

    for (int i = 0; i < 5; i++)
    {
        mvwprintw(win_banda_detail, 4 + i, 4, "%s", opciones[i]);
    }

    // Mostrar estado de bandas que necesitan abastecimiento
    mvwprintw(win_banda_detail, 11, 2, "BANDAS QUE NECESITAN ABASTECIMIENTO:");
    int bandas_criticas = 0;
    for (int i = 0; i < datos_compartidos->num_bandas; i++)
    {
        if (datos_compartidos->bandas[i].necesita_reabastecimiento)
        {
            if (has_colors())
                wattron(win_banda_detail, COLOR_PAIR(3));
            mvwprintw(win_banda_detail, 13 + bandas_criticas, 4, "* Banda %d - Requiere atencion", i + 1);
            if (has_colors())
                wattroff(win_banda_detail, COLOR_PAIR(3));
            bandas_criticas++;
        }
    }

    if (bandas_criticas == 0)
    {
        if (has_colors())
            wattron(win_banda_detail, COLOR_PAIR(1));
        mvwprintw(win_banda_detail, 13, 4, "[OK] Todas las bandas en niveles normales");
        if (has_colors())
            wattroff(win_banda_detail, COLOR_PAIR(1));
    }

    // Mostrar controles
    mvwprintw(win_banda_detail, 18, 2, "CONTROLES:");
    mvwprintw(win_banda_detail, 19, 2, "1-5: Ejecutar opcion");
    mvwprintw(win_banda_detail, 20, 2, "ESC: Salir del modo abastecimiento");

    wrefresh(win_banda_detail);
}

void mostrar_inventario_global()
{
    werase(win_banda_detail);

    if (has_colors())
        wattron(win_banda_detail, COLOR_PAIR(4));
    wborder(win_banda_detail, '|', '|', '-', '-', '+', '+', '+', '+');
    mvwprintw(win_banda_detail, 0, 2, " INVENTARIO GLOBAL ");
    if (has_colors())
        wattroff(win_banda_detail, COLOR_PAIR(4));

    mvwprintw(win_banda_detail, 2, 2, "RESUMEN POR INGREDIENTE:");

    for (int ing = 0; ing < MAX_INGREDIENTES; ing++)
    {
        int total_ingrediente = 0;
        int bandas_agotadas = 0;
        int bandas_criticas = 0;

        // Calcular totales
        for (int banda = 0; banda < datos_compartidos->num_bandas; banda++)
        {
            pthread_mutex_lock(&datos_compartidos->bandas[banda].dispensadores[ing].mutex);
            int cantidad = datos_compartidos->bandas[banda].dispensadores[ing].cantidad;
            total_ingrediente += cantidad;

            if (cantidad == 0)
                bandas_agotadas++;
            else if (cantidad <= UMBRAL_INVENTARIO_BAJO)
                bandas_criticas++;

            pthread_mutex_unlock(&datos_compartidos->bandas[banda].dispensadores[ing].mutex);
        }

        // Mostrar solo primeros 12 ingredientes para no llenar la pantalla
        if (ing < 12)
        {
            int linea = 4 + ing;
            char nombre_corto[15];
            strncpy(nombre_corto, ingredientes_base[ing], 14);
            nombre_corto[14] = '\0';

            int color = 1; // Verde por defecto
            if (bandas_agotadas > 0)
                color = 3; // Rojo si hay agotadas
            else if (bandas_criticas > 0)
                color = 2; // Amarillo si hay criticas

            if (has_colors())
                wattron(win_banda_detail, COLOR_PAIR(color));

            mvwprintw(win_banda_detail, linea, 2, "%-14s: Total:%3d",
                      nombre_corto, total_ingrediente);

            if (bandas_agotadas > 0)
            {
                mvwprintw(win_banda_detail, linea, 30, "[X]%d", bandas_agotadas);
            }
            if (bandas_criticas > 0)
            {
                mvwprintw(win_banda_detail, linea, 35, "[!]%d", bandas_criticas);
            }

            if (has_colors())
                wattroff(win_banda_detail, COLOR_PAIR(color));
        }
    }

    wrefresh(win_banda_detail);
}

void mostrar_comandos_disponibles()
{
    werase(win_commands);

    if (has_colors())
        wattron(win_commands, COLOR_PAIR(5));
    wborder(win_commands, '|', '|', '-', '-', '+', '+', '+', '+');
    mvwprintw(win_commands, 0, 2, " COMANDOS DISPONIBLES ");
    if (has_colors())
        wattroff(win_commands, COLOR_PAIR(5));

    // Mostrar comandos según el modo actual
    switch (modo_vista)
    {
    case 0: // Vista general
        mvwprintw(win_commands, 1, 2, "NAVEGACION:");
        mvwprintw(win_commands, 2, 2, "  ^/v  Cambiar banda    TAB  Cambiar vista");
        mvwprintw(win_commands, 3, 2, "CONTROL:");
        mvwprintw(win_commands, 4, 2, "  ESPACIO Pausar/Reanudar  R  Reabastecer");
        mvwprintw(win_commands, 5, 2, "  S  Modo abastecimiento  H  Ayuda  Q  Salir");
        break;

    case 1: // Detalle banda
        mvwprintw(win_commands, 1, 2, "NAVEGACION:");
        mvwprintw(win_commands, 2, 2, "  ^/v  Cambiar banda    TAB  Cambiar vista");
        mvwprintw(win_commands, 3, 2, "CONTROL:");
        mvwprintw(win_commands, 4, 2, "  ESPACIO Pausar/Reanudar  R  Reabastecer");
        mvwprintw(win_commands, 5, 2, "  I  Ver inventario  S  Abastecimiento");
        break;

    case 2: // Inventario global
        mvwprintw(win_commands, 1, 2, "NAVEGACION:");
        mvwprintw(win_commands, 2, 2, "  TAB  Cambiar vista");
        mvwprintw(win_commands, 3, 2, "CONTROL:");
        mvwprintw(win_commands, 4, 2, "  A  Reabastecer todas  S  Modo abastecimiento");
        mvwprintw(win_commands, 5, 2, "  H  Ayuda    Q  Salir");
        break;

    case 3: // Inventario banda
        mvwprintw(win_commands, 1, 2, "NAVEGACION:");
        mvwprintw(win_commands, 2, 2, "  ^/v  Cambiar ingrediente  TAB  Vista");
        mvwprintw(win_commands, 3, 2, "EDICION:");
        mvwprintw(win_commands, 4, 2, "  +/-  Ajustar cantidad  F  Llenar");
        mvwprintw(win_commands, 5, 2, "  R  Reabastecer banda completa");
        break;

    case 4: // Modo abastecimiento
        mvwprintw(win_commands, 1, 2, "ABASTECIMIENTO:");
        mvwprintw(win_commands, 2, 2, "  1-5  Seleccionar opcion");
        mvwprintw(win_commands, 3, 2, "  ESC  Salir del modo");
        mvwprintw(win_commands, 4, 2, "RAPIDO:");
        mvwprintw(win_commands, 5, 2, "  A  Todas  C  Criticas  E  Agotadas");
        break;

    default:
        mvwprintw(win_commands, 1, 2, "NAVEGACION:");
        mvwprintw(win_commands, 2, 2, "  ^/v  Cambiar banda    TAB  Cambiar vista");
        mvwprintw(win_commands, 3, 2, "CONTROL:");
        mvwprintw(win_commands, 4, 2, "  ESPACIO Pausar/Reanudar  R  Reabastecer");
        mvwprintw(win_commands, 5, 2, "  H  Ayuda    Q  Salir");
        break;
    }

    wrefresh(win_commands);
}

void mostrar_estado_sistema()
{
    werase(win_status);

    if (has_colors())
        wattron(win_status, COLOR_PAIR(4));
    wborder(win_status, '|', '|', '-', '-', '+', '+', '+', '+');

    switch (modo_vista)
    {
    case 0:
        mvwprintw(win_status, 0, 2, " VISTA GENERAL ");
        break;
    case 1:
        mvwprintw(win_status, 0, 2, " DETALLE BANDA %d ", banda_seleccionada + 1);
        break;
    case 2:
        mvwprintw(win_status, 0, 2, " INVENTARIO GLOBAL ");
        break;
    case 3:
        mvwprintw(win_status, 0, 2, " INVENTARIO BANDA %d ", banda_seleccionada + 1);
        break;
    case 4:
        mvwprintw(win_status, 0, 2, " MODO ABASTECIMIENTO ");
        break;
    }

    if (has_colors())
        wattroff(win_status, COLOR_PAIR(4));

    // Estado del sistema
    if (datos_compartidos->sistema_activo)
    {
        if (has_colors())
            wattron(win_status, COLOR_PAIR(1));
        mvwprintw(win_status, 2, 2, "[OK] Sistema ACTIVO");
        if (has_colors())
            wattroff(win_status, COLOR_PAIR(1));
    }
    else
    {
        if (has_colors())
            wattron(win_status, COLOR_PAIR(3));
        mvwprintw(win_status, 2, 2, "[X] Sistema INACTIVO");
        if (has_colors())
            wattroff(win_status, COLOR_PAIR(3));
    }

    // Hora actual
    time_t ahora = time(NULL);
    struct tm *info_tiempo = localtime(&ahora);
    mvwprintw(win_status, 3, 2, "[TIME] %02d:%02d:%02d",
              info_tiempo->tm_hour, info_tiempo->tm_min, info_tiempo->tm_sec);

    // Banda seleccionada
    if (has_colors())
        wattron(win_status, COLOR_PAIR(6));
    mvwprintw(win_status, 4, 2, "[SEL] Banda: %d", banda_seleccionada + 1);
    if (modo_vista == 3)
    {
        mvwprintw(win_status, 5, 2, "[ING] %s", ingredientes_base[ingrediente_seleccionado]);
    }
    if (has_colors())
        wattroff(win_status, COLOR_PAIR(6));

    wrefresh(win_status);
}

// ================================================================
// FUNCIONES DE CONTROL
// ================================================================

void procesar_comando(int ch)
{
    switch (ch)
    {
    case 'q':
    case 'Q':
        return;

    case 'h':
    case 'H':
        mostrar_ayuda_detallada();
        break;

    case KEY_UP:
        if (modo_vista == 3) // Inventario banda
            cambiar_ingrediente_seleccionado(-1);
        else
            cambiar_banda_seleccionada(-1);
        break;

    case KEY_DOWN:
        if (modo_vista == 3) // Inventario banda
            cambiar_ingrediente_seleccionado(1);
        else
            cambiar_banda_seleccionada(1);
        break;

    case '\t':
    case KEY_RIGHT:
        if (modo_vista == 4)
            break;                         // No cambiar vista en modo abastecimiento
        modo_vista = (modo_vista + 1) % 4; // 0-3 (excluir modo abastecimiento)
        ingrediente_seleccionado = 0;      // Reset ingrediente
        break;

    case KEY_LEFT:
        if (modo_vista == 4)
            break;                             // No cambiar vista en modo abastecimiento
        modo_vista = (modo_vista - 1 + 4) % 4; // 0-3 (excluir modo abastecimiento)
        ingrediente_seleccionado = 0;          // Reset ingrediente
        break;

    case ' ': // ESPACIO
        pausar_reanudar_banda(banda_seleccionada);
        break;

    case 'r':
    case 'R':
        reabastecer_banda_completa(banda_seleccionada);
        break;

    case 'i':
    case 'I':
        modo_vista = 3; // Cambiar a inventario de banda
        ingrediente_seleccionado = 0;
        break;

    case 's':
    case 'S':
        modo_vista = 4; // Cambiar a modo abastecimiento
        break;

    case 27: // ESC
        if (modo_vista == 4)
        {
            modo_vista = 0; // Volver a vista general
        }
        break;

    case 'a':
    case 'A':
        if (modo_vista == 4) // Modo abastecimiento
        {
            // Reabastecer todas las bandas
            for (int i = 0; i < datos_compartidos->num_bandas; i++)
            {
                reabastecer_banda_completa(i);
            }
            mostrar_mensaje_temporal("[OK] Todas las bandas reabastecidas");
        }
        break;

    case 'c':
    case 'C':
        if (modo_vista == 4) // Modo abastecimiento
        {
            // Reabastecer solo ingredientes críticos
            int bandas_reabastecidas = 0;
            for (int banda = 0; banda < datos_compartidos->num_bandas; banda++)
            {
                int tenia_criticos = 0;
                for (int ing = 0; ing < MAX_INGREDIENTES; ing++)
                {
                    pthread_mutex_lock(&datos_compartidos->bandas[banda].dispensadores[ing].mutex);
                    if (datos_compartidos->bandas[banda].dispensadores[ing].cantidad <= UMBRAL_INVENTARIO_BAJO)
                    {
                        datos_compartidos->bandas[banda].dispensadores[ing].cantidad = CAPACIDAD_DISPENSADOR;
                        tenia_criticos = 1;
                    }
                    pthread_mutex_unlock(&datos_compartidos->bandas[banda].dispensadores[ing].mutex);
                }
                if (tenia_criticos)
                {
                    bandas_reabastecidas++;
                    datos_compartidos->bandas[banda].necesita_reabastecimiento = 0;
                }
            }
            char mensaje[60];
            snprintf(mensaje, sizeof(mensaje), "[OK] %d bandas con ingredientes críticos reabastecidas", bandas_reabastecidas);
            mostrar_mensaje_temporal(mensaje);
        }
        break;

    case 'e':
    case 'E':
        if (modo_vista == 4) // Modo abastecimiento
        {
            // Reabastecer solo ingredientes agotados
            int ingredientes_reabastecidos = 0;
            for (int banda = 0; banda < datos_compartidos->num_bandas; banda++)
            {
                for (int ing = 0; ing < MAX_INGREDIENTES; ing++)
                {
                    pthread_mutex_lock(&datos_compartidos->bandas[banda].dispensadores[ing].mutex);
                    if (datos_compartidos->bandas[banda].dispensadores[ing].cantidad == 0)
                    {
                        datos_compartidos->bandas[banda].dispensadores[ing].cantidad = CAPACIDAD_DISPENSADOR;
                        ingredientes_reabastecidos++;
                    }
                    pthread_mutex_unlock(&datos_compartidos->bandas[banda].dispensadores[ing].mutex);
                }
            }
            char mensaje[60];
            snprintf(mensaje, sizeof(mensaje), "[OK] %d ingredientes agotados reabastecidos", ingredientes_reabastecidos);
            mostrar_mensaje_temporal(mensaje);
        }
        break;

    case '+':
    case '=':
        if (modo_vista == 3) // Inventario banda
        {
            Banda *banda = &datos_compartidos->bandas[banda_seleccionada];
            pthread_mutex_lock(&banda->dispensadores[ingrediente_seleccionado].mutex);
            if (banda->dispensadores[ingrediente_seleccionado].cantidad < CAPACIDAD_DISPENSADOR)
            {
                banda->dispensadores[ingrediente_seleccionado].cantidad++;
                mostrar_mensaje_temporal("[+] Ingrediente añadido");
            }
            pthread_mutex_unlock(&banda->dispensadores[ingrediente_seleccionado].mutex);
        }
        break;

    case '-':
    case '_':
        if (modo_vista == 3) // Inventario banda
        {
            Banda *banda = &datos_compartidos->bandas[banda_seleccionada];
            pthread_mutex_lock(&banda->dispensadores[ingrediente_seleccionado].mutex);
            if (banda->dispensadores[ingrediente_seleccionado].cantidad > 0)
            {
                banda->dispensadores[ingrediente_seleccionado].cantidad--;
                mostrar_mensaje_temporal("[-] Ingrediente removido");
            }
            pthread_mutex_unlock(&banda->dispensadores[ingrediente_seleccionado].mutex);
        }
        break;

    case 'f':
    case 'F':
        if (modo_vista == 3) // Inventario banda
        {
            Banda *banda = &datos_compartidos->bandas[banda_seleccionada];
            pthread_mutex_lock(&banda->dispensadores[ingrediente_seleccionado].mutex);
            banda->dispensadores[ingrediente_seleccionado].cantidad = CAPACIDAD_DISPENSADOR;
            pthread_mutex_unlock(&banda->dispensadores[ingrediente_seleccionado].mutex);
            char mensaje[80];
            snprintf(mensaje, sizeof(mensaje), "[F] %s llenado completamente",
                     ingredientes_base[ingrediente_seleccionado]);
            mostrar_mensaje_temporal(mensaje);
        }
        break;

    // Números para opciones de abastecimiento
    case '1':
        if (modo_vista == 4)
        {
            reabastecer_banda_completa(banda_seleccionada);
        }
        else
        {
            if (0 < datos_compartidos->num_bandas)
                banda_seleccionada = 0;
        }
        break;
    case '2':
        if (modo_vista == 4)
        {
            for (int i = 0; i < datos_compartidos->num_bandas; i++)
            {
                reabastecer_banda_completa(i);
            }
            mostrar_mensaje_temporal("[OK] Todas las bandas reabastecidas");
        }
        else
        {
            if (1 < datos_compartidos->num_bandas)
                banda_seleccionada = 1;
        }
        break;
    case '3':
        if (modo_vista == 4)
        {
            // Reabastecer solo críticos (igual que 'C')
            procesar_comando('C');
        }
        else
        {
            if (2 < datos_compartidos->num_bandas)
                banda_seleccionada = 2;
        }
        break;
    case '4':
        if (modo_vista == 4)
        {
            // Reabastecer solo agotados (igual que 'E')
            procesar_comando('E');
        }
        else
        {
            if (3 < datos_compartidos->num_bandas)
                banda_seleccionada = 3;
        }
        break;
    case '5':
        if (modo_vista == 4)
        {
            modo_vista = 3; // Cambiar a modo personalizado (inventario banda)
            mostrar_mensaje_temporal("[OK] Modo personalizado activado");
        }
        else
        {
            if (4 < datos_compartidos->num_bandas)
                banda_seleccionada = 4;
        }
        break;
    case '6':
    case '7':
    case '8':
    case '9':
        if (modo_vista != 4)
        {
            int banda_num = ch - '1';
            if (banda_num < datos_compartidos->num_bandas)
            {
                banda_seleccionada = banda_num;
            }
        }
        break;
    }
}

void cambiar_banda_seleccionada(int direccion)
{
    banda_seleccionada += direccion;
    if (banda_seleccionada < 0)
    {
        banda_seleccionada = datos_compartidos->num_bandas - 1;
    }
    if (banda_seleccionada >= datos_compartidos->num_bandas)
    {
        banda_seleccionada = 0;
    }
}

void cambiar_ingrediente_seleccionado(int direccion)
{
    ingrediente_seleccionado += direccion;
    if (ingrediente_seleccionado < 0)
    {
        ingrediente_seleccionado = MAX_INGREDIENTES - 1;
    }
    if (ingrediente_seleccionado >= MAX_INGREDIENTES)
    {
        ingrediente_seleccionado = 0;
    }
}

void pausar_reanudar_banda(int banda_id)
{
    if (banda_id >= 0 && banda_id < datos_compartidos->num_bandas)
    {
        Banda *banda = &datos_compartidos->bandas[banda_id];

        if (banda->pausada)
        {
            banda->pausada = 0;
            pthread_cond_signal(&banda->condicion);
            char mensaje[50];
            snprintf(mensaje, sizeof(mensaje), "[OK] Banda %d REANUDADA", banda_id + 1);
            mostrar_mensaje_temporal(mensaje);
        }
        else
        {
            banda->pausada = 1;
            char mensaje[50];
            snprintf(mensaje, sizeof(mensaje), "[PAUSE] Banda %d PAUSADA", banda_id + 1);
            mostrar_mensaje_temporal(mensaje);
        }
    }
}

void reabastecer_banda_completa(int banda_id)
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

        char mensaje[50];
        snprintf(mensaje, sizeof(mensaje), "[OK] Banda %d REABASTECIDA", banda_id + 1);
        mostrar_mensaje_temporal(mensaje);
    }
}

void reabastecer_ingrediente_especifico(int banda_id, int ingrediente_id)
{
    if (banda_id >= 0 && banda_id < datos_compartidos->num_bandas &&
        ingrediente_id >= 0 && ingrediente_id < MAX_INGREDIENTES)
    {
        pthread_mutex_lock(&datos_compartidos->bandas[banda_id].dispensadores[ingrediente_id].mutex);
        datos_compartidos->bandas[banda_id].dispensadores[ingrediente_id].cantidad = CAPACIDAD_DISPENSADOR;
        pthread_mutex_unlock(&datos_compartidos->bandas[banda_id].dispensadores[ingrediente_id].mutex);

        char mensaje[70];
        snprintf(mensaje, sizeof(mensaje), "[OK] %s en Banda %d reabastecido",
                 ingredientes_base[ingrediente_id], banda_id + 1);
        mostrar_mensaje_temporal(mensaje);
    }
}

void mostrar_mensaje_temporal(const char *mensaje)
{
    // Mostrar mensaje en la linea inferior de la pantalla
    int altura, ancho;
    getmaxyx(stdscr, altura, ancho);

    if (has_colors())
        attron(COLOR_PAIR(7));
    mvprintw(altura - 1, 0, "%s", mensaje);
    for (int i = strlen(mensaje); i < ancho; i++)
    {
        addch(' ');
    }
    if (has_colors())
        attroff(COLOR_PAIR(7));

    refresh();
    usleep(1500000); // 1.5 segundos

    // Limpiar linea
    mvprintw(altura - 1, 0, "%*s", ancho, "");
    refresh();
}

void mostrar_ayuda_detallada()
{
    clear();

    if (has_colors())
        attron(COLOR_PAIR(4));
    mvprintw(2, 5, "+===============================================================================+");
    mvprintw(3, 5, "|                      PANEL DE CONTROL MEJORADO - AYUDA                         |");
    mvprintw(4, 5, "+===============================================================================+");
    if (has_colors())
        attroff(COLOR_PAIR(4));

    mvprintw(6, 5, "| NAVEGACION:                                                                    |");
    mvprintw(7, 5, "|   ^/v              Cambiar banda/ingrediente seleccionado                     |");
    mvprintw(8, 5, "|   TAB / <-/->      Cambiar vista (General/Detalle/Global/Inventario)           |");
    mvprintw(9, 5, "|   1-9              Seleccionar banda directamente                             |");
    mvprintw(10, 5, "|                                                                                |");
    mvprintw(11, 5, "| CONTROL DE BANDAS:                                                             |");
    mvprintw(12, 5, "|   ESPACIO          Pausar/Reanudar banda seleccionada                        |");
    mvprintw(13, 5, "|   R                Reabastecer banda seleccionada completamente               |");
    mvprintw(14, 5, "|   I                Ver inventario detallado de la banda                       |");
    mvprintw(15, 5, "|   S                Entrar al modo de abastecimiento                           |");
    mvprintw(16, 5, "|                                                                                |");
    mvprintw(17, 5, "| MODO INVENTARIO BANDA:                                                         |");
    mvprintw(18, 5, "|   +/-              Añadir/quitar 1 unidad del ingrediente seleccionado       |");
    mvprintw(19, 5, "|   F                Llenar completamente el ingrediente seleccionado           |");
    mvprintw(20, 5, "|                                                                                |");
    mvprintw(21, 5, "| MODO ABASTECIMIENTO:                                                           |");
    mvprintw(22, 5, "|   1                Reabastecer banda seleccionada                             |");
    mvprintw(23, 5, "|   2 o A            Reabastecer TODAS las bandas                               |");
    mvprintw(24, 5, "|   3 o C            Reabastecer solo ingredientes críticos                     |");
    mvprintw(25, 5, "|   4 o E            Reabastecer solo ingredientes agotados                     |");
    mvprintw(26, 5, "|   5                Modo personalizado (ingrediente por ingrediente)           |");
    mvprintw(27, 5, "|   ESC              Salir del modo abastecimiento                              |");
    mvprintw(28, 5, "|                                                                                |");
    mvprintw(29, 5, "| VISTAS DISPONIBLES:                                                            |");
    mvprintw(30, 5, "|   General          Resumen de todas las bandas y estadísticas                |");
    mvprintw(31, 5, "|   Detalle          Información específica de la banda seleccionada           |");
    mvprintw(32, 5, "|   Global           Resumen de inventarios por ingrediente                     |");
    mvprintw(33, 5, "|   Inventario       Inventario completo de la banda seleccionada              |");
    mvprintw(34, 5, "|   Abastecimiento   Opciones de reabastecimiento masivo                       |");
    mvprintw(35, 5, "|                                                                                |");
    mvprintw(36, 5, "| INDICADORES:                                                                   |");
    mvprintw(37, 5, "|   [OK] Verde       Funcionamiento normal                                       |");
    mvprintw(38, 5, "|   [!]  Amarillo    Advertencia/Crítico                                        |");
    mvprintw(39, 5, "|   [X]  Rojo        Error/Agotado/Inactivo                                       |");
    mvprintw(40, 5, "|   >    Azul        Elemento seleccionado                                       |");
    mvprintw(41, 5, "+===============================================================================+");

    if (has_colors())
        attron(COLOR_PAIR(5));
    mvprintw(43, 5, "Presiona cualquier tecla para continuar...");
    if (has_colors())
        attroff(COLOR_PAIR(5));

    refresh();
    getch();
}

void limpiar_ncurses()
{
    if (win_main)
        delwin(win_main);
    if (win_banda_detail)
        delwin(win_banda_detail);
    if (win_commands)
        delwin(win_commands);
    if (win_status)
        delwin(win_status);
    endwin();
}

// ================================================================
// FUNCION PRINCIPAL
// ================================================================

int main()
{
    printf("Iniciando Panel de Control Mejorado del Sistema de Hamburguesas...\n");
    printf("Conectando con el sistema principal...\n");

    // Conectar con el sistema principal
    conectar_memoria_compartida();
    printf("Conexión establecida exitosamente\n");

    // Verificar que el sistema este activo
    if (!datos_compartidos->sistema_activo)
    {
        printf("El sistema principal no está activo\n");
        printf("   Inicia primero: ./burger_system -n 4\n");
        exit(1);
    }

    sleep(2);

    // Inicializar interfaz
    inicializar_ncurses();

    // Bucle principal
    int ch;
    time_t ultimo_refresh = time(NULL);

    while ((ch = getch()) != 'q' && ch != 'Q')
    {
        time_t ahora = time(NULL);

        // Actualizar interfaz cada segundo o con comando
        if (ahora - ultimo_refresh >= 1 || ch != ERR)
        {
            // Verificar si el sistema sigue activo
            if (!datos_compartidos->sistema_activo)
            {
                break;
            }

            // Mostrar interfaces según el modo
            switch (modo_vista)
            {
            case 0: // Vista general
                mostrar_interfaz_general();
                werase(win_banda_detail);
                wrefresh(win_banda_detail);
                break;
            case 1: // Detalle de banda
                mostrar_interfaz_general();
                mostrar_detalle_banda();
                break;
            case 2: // Inventario global
                mostrar_interfaz_general();
                mostrar_inventario_global();
                break;
            case 3: // Inventario de banda
                mostrar_interfaz_general();
                mostrar_inventario_banda();
                break;
            case 4: // Modo abastecimiento
                mostrar_interfaz_general();
                mostrar_modo_abastecimiento();
                break;
            }

            mostrar_comandos_disponibles();
            mostrar_estado_sistema();
            ultimo_refresh = ahora;
        }

        // Procesar comando si hay uno
        if (ch != ERR)
        {
            procesar_comando(ch);
            if (ch == 'q' || ch == 'Q')
                break;
        }

        usleep(50000); // 50ms de espera para no saturar CPU
    }

    // Limpiar recursos
    limpiar_ncurses();

    printf("\nPanel de control terminado correctamente\n");
    printf("Estadísticas finales:\n");
    // Mostrar estadísticas finales del sistema
    printf("   * Órdenes procesadas: %d\n", datos_compartidos->total_ordenes_procesadas);
    printf("   * Órdenes en cola: %d\n", datos_compartidos->cola_espera.tamano);
    printf("   * Bandas monitoreadas: %d\n", datos_compartidos->num_bandas);
    printf("   * Funciones de abastecimiento utilizadas\n");

    return 0;
}

/**
 * @}
 *
 * @page conclusiones_panel Conclusiones del Panel de Control
 *
 * Este panel de control demuestra las siguientes características técnicas:
 *
 * - **INTERFAZ GRÁFICA AVANZADA**: Utiliza ncurses para interfaz profesional en terminal
 * - **COMUNICACIÓN EN TIEMPO REAL**: Conexión directa con el sistema principal
 * - **MÚLTIPLES VISTAS**: Diferentes modos de visualización según necesidades
 * - **CONTROL INTERACTIVO**: Operaciones directas sobre bandas e inventario
 * - **NAVEGACIÓN INTUITIVA**: Controles de teclado consistentes y fáciles de usar
 * - **GESTIÓN DE RECURSOS**: Manejo eficiente de memoria y sincronización
 *
 * El panel está diseñado para ser robusto, eficiente y fácil de usar,
 * proporcionando control total sobre el sistema de hamburguesas.
 *
 * @author [Tu Nombre]
 * @date [Fecha de Finalización]
 * @version 1.0
 */