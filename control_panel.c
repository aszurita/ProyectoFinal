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

#define MAX_BANDAS 10
#define MAX_INGREDIENTES 15
#define MAX_ORDENES 100
#define MAX_NOMBRE_INGREDIENTE 30
#define MAX_LOGS_POR_BANDA 10
#define CAPACIDAD_DISPENSADOR 10
#define NUM_TIPOS_HAMBURGUESA 6
#define UMBRAL_INVENTARIO_BAJO 2

// Mismas estructuras que el sistema principal
typedef struct {
    char nombre[MAX_NOMBRE_INGREDIENTE];
    int cantidad;
    pthread_mutex_t mutex;
} Ingrediente;

typedef struct {
    char nombre[50];
    char ingredientes[10][MAX_NOMBRE_INGREDIENTE];
    int num_ingredientes;
    float precio;
} TipoHamburguesa;

typedef struct {
    char mensaje[100];
    time_t timestamp;
    int es_alerta;
} LogEntry;

typedef struct {
    int id_orden;
    int tipo_hamburguesa;
    char nombre_hamburguesa[50];
    char ingredientes_solicitados[MAX_INGREDIENTES][MAX_NOMBRE_INGREDIENTE];
    int num_ingredientes;
    time_t tiempo_creacion;
    int paso_actual;
    int completada;
    int asignada_a_banda;
    int intentos_asignacion;
} Orden;

typedef struct {
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
    int necesita_reabastecimiento;
    time_t ultima_alerta_inventario;
} Banda;

typedef struct {
    Orden ordenes[MAX_ORDENES];
    int frente;
    int atras;
    int tamano;
    pthread_mutex_t mutex;
    pthread_cond_t no_vacia;
    pthread_cond_t no_llena;
} ColaFIFO;

typedef struct {
    Banda bandas[MAX_BANDAS];
    ColaFIFO cola_espera;
    int num_bandas;
    int sistema_activo;
    int total_ordenes_procesadas;
    int total_ordenes_generadas;
    pthread_mutex_t mutex_global;
    pthread_cond_t nueva_orden;
} DatosCompartidos;

// Variables globales
DatosCompartidos *datos_compartidos;
WINDOW *win_main;
WINDOW *win_banda_detail;
WINDOW *win_commands;
WINDOW *win_status;
int banda_seleccionada = 0;
int modo_vista = 0; // 0=general, 1=detalle banda, 2=inventario global

// Menú de hamburguesas
TipoHamburguesa menu_hamburguesas[NUM_TIPOS_HAMBURGUESA] = {
    {"Clasica", {"pan_inferior", "carne", "lechuga", "tomate", "pan_superior"}, 5, 8.50},
    {"Cheeseburger", {"pan_inferior", "carne", "queso", "lechuga", "tomate", "pan_superior"}, 6, 9.25},
    {"BBQ Bacon", {"pan_inferior", "carne", "bacon", "queso", "cebolla", "salsa_bbq", "pan_superior"}, 7, 11.75},
    {"Vegetariana", {"pan_inferior", "vegetal", "lechuga", "tomate", "aguacate", "mayonesa", "pan_superior"}, 7, 10.25},
    {"Deluxe", {"pan_inferior", "carne", "queso", "bacon", "lechuga", "tomate", "cebolla", "mayonesa", "pan_superior"}, 9, 13.50},
    {"Spicy Mexican", {"pan_inferior", "carne", "queso", "jalapenos", "tomate", "cebolla", "salsa_picante", "pan_superior"}, 8, 12.00}
};

// Ingredientes base
char ingredientes_base[MAX_INGREDIENTES][MAX_NOMBRE_INGREDIENTE] = {
    "pan_inferior", "pan_superior", "carne", "queso", "tomate",
    "lechuga", "cebolla", "bacon", "mayonesa", "jalapenos",
    "aguacate", "vegetal", "salsa_bbq", "salsa_picante", "pepinillos"
};

// Prototipos
void inicializar_ncurses();
void conectar_memoria_compartida();
void mostrar_interfaz_general();
void mostrar_detalle_banda();
void mostrar_inventario_global();
void mostrar_comandos_disponibles();
void procesar_comando(int ch);
void pausar_reanudar_banda(int banda_id);
void reabastecer_banda_completa(int banda_id);
void reabastecer_ingrediente_especifico(int banda_id, int ingrediente_id);
void cambiar_banda_seleccionada(int direccion);
void mostrar_mensaje_temporal(const char *mensaje);
void mostrar_ayuda_detallada();
void limpiar_ncurses();
const char* obtener_color_ingrediente(int cantidad);

// ═══════════════════════════════════════════════════════════════
// INICIALIZACIÓN Y CONFIGURACIÓN
// ═══════════════════════════════════════════════════════════════

void inicializar_ncurses()
{
    initscr();
    cbreak();
    noecho();
    nodelay(stdscr, TRUE);
    keypad(stdscr, TRUE);
    curs_set(0);

    // Configurar colores
    if (has_colors()) {
        start_color();
        init_pair(1, COLOR_GREEN, COLOR_BLACK);   // Verde - Normal
        init_pair(2, COLOR_YELLOW, COLOR_BLACK);  // Amarillo - Advertencia
        init_pair(3, COLOR_RED, COLOR_BLACK);     // Rojo - Crítico/Error
        init_pair(4, COLOR_CYAN, COLOR_BLACK);    // Cian - Encabezados
        init_pair(5, COLOR_WHITE, COLOR_BLUE);    // Selección
        init_pair(6, COLOR_MAGENTA, COLOR_BLACK); // Destacado
        init_pair(7, COLOR_BLACK, COLOR_WHITE);   // Invertido
    }

    // Calcular dimensiones
    int altura, ancho;
    getmaxyx(stdscr, altura, ancho);

    // Crear ventanas principales
    win_main = newwin(altura - 8, ancho - 2, 1, 1);
    win_banda_detail = newwin(altura - 8, ancho/2, 1, ancho/2);
    win_commands = newwin(6, ancho/2, altura - 7, 1);
    win_status = newwin(6, ancho/2, altura - 7, ancho/2);

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
    if (shm_fd == -1) {
        endwin();
        printf("❌ Error: No se pudo conectar con el sistema principal.\n");
        printf("   Asegúrate de que ./burger_system esté ejecutándose.\n");
        printf("   Uso: ./burger_system -n 4 &\n");
        printf("        ./control_panel\n");
        exit(1);
    }

    datos_compartidos = mmap(0, sizeof(DatosCompartidos), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);

    if (datos_compartidos == MAP_FAILED) {
        endwin();
        perror("Error mapeando memoria compartida");
        exit(1);
    }

    close(shm_fd);
}

// ═══════════════════════════════════════════════════════════════
// FUNCIONES DE VISUALIZACIÓN
// ═══════════════════════════════════════════════════════════════

void mostrar_interfaz_general()
{
    werase(win_main);
    
    if (has_colors()) wattron(win_main, COLOR_PAIR(4));
    wborder(win_main, '|', '|', '-', '-', '+', '+', '+', '+');
    mvwprintw(win_main, 0, 2, " SISTEMA DE HAMBURGUESAS - VISTA GENERAL ");
    if (has_colors()) wattroff(win_main, COLOR_PAIR(4));

    // Estadísticas generales
    mvwprintw(win_main, 2, 2, "📊 ESTADÍSTICAS DEL SISTEMA:");
    mvwprintw(win_main, 3, 4, "• Órdenes generadas:  %d", datos_compartidos->total_ordenes_generadas);
    mvwprintw(win_main, 4, 4, "• Órdenes procesadas: %d", datos_compartidos->total_ordenes_procesadas);
    mvwprintw(win_main, 5, 4, "• Órdenes en cola:    %d", datos_compartidos->cola_espera.tamano);
    mvwprintw(win_main, 6, 4, "• Bandas activas:     %d", datos_compartidos->num_bandas);

    // Eficiencia
    float eficiencia = 0;
    if (datos_compartidos->total_ordenes_generadas > 0) {
        eficiencia = (float)datos_compartidos->total_ordenes_procesadas / datos_compartidos->total_ordenes_generadas * 100;
    }
    mvwprintw(win_main, 7, 4, "• Eficiencia:         %.1f%%", eficiencia);

    // Estado de bandas
    mvwprintw(win_main, 9, 2, "🏭 ESTADO DE BANDAS:");
    
    for (int i = 0; i < datos_compartidos->num_bandas; i++) {
        Banda *banda = &datos_compartidos->bandas[i];
        int linea = 11 + i;
        
        // Determinar color según estado
        int color_pair = 1; // Verde por defecto
        char estado_icono[10] = "✅";
        
        pthread_mutex_lock(&banda->mutex);
        
        if (!banda->activa) {
            color_pair = 3;
            strcpy(estado_icono, "❌");
        } else if (banda->pausada) {
            color_pair = 2;
            strcpy(estado_icono, "⏸️ ");
        } else if (banda->necesita_reabastecimiento) {
            color_pair = 2;
            strcpy(estado_icono, "⚠️ ");
        }
        
        if (i == banda_seleccionada) {
            color_pair = 5; // Destacar banda seleccionada
        }
        
        if (has_colors()) wattron(win_main, COLOR_PAIR(color_pair));
        
        mvwprintw(win_main, linea, 4, "%s BANDA %d:", estado_icono, i + 1);
        
        if (banda->procesando_orden) {
            mvwprintw(win_main, linea, 18, "Procesando %s (#%d) - %d/%d", 
                     banda->orden_actual.nombre_hamburguesa,
                     banda->orden_actual.id_orden,
                     banda->orden_actual.paso_actual,
                     banda->orden_actual.num_ingredientes);
        } else {
            mvwprintw(win_main, linea, 18, "%s", banda->estado_actual);
        }
        
        mvwprintw(win_main, linea, 55, "Completadas: %d", banda->hamburguesas_procesadas);
        
        if (has_colors()) wattroff(win_main, COLOR_PAIR(color_pair));
        
        pthread_mutex_unlock(&banda->mutex);
    }
    
    // Alertas de inventario
    int bandas_con_alertas = 0;
    for (int i = 0; i < datos_compartidos->num_bandas; i++) {
        if (datos_compartidos->bandas[i].necesita_reabastecimiento) {
            bandas_con_alertas++;
        }
    }
    
    if (bandas_con_alertas > 0) {
        if (has_colors()) wattron(win_main, COLOR_PAIR(3));
        mvwprintw(win_main, 11 + datos_compartidos->num_bandas + 1, 2, 
                 "🚨 ALERTAS: %d bandas necesitan reabastecimiento", bandas_con_alertas);
        if (has_colors()) wattroff(win_main, COLOR_PAIR(3));
    }

    wrefresh(win_main);
}

void mostrar_detalle_banda()
{
    werase(win_banda_detail);
    
    if (has_colors()) wattron(win_banda_detail, COLOR_PAIR(6));
    wborder(win_banda_detail, '|', '|', '-', '-', '+', '+', '+', '+');
    mvwprintw(win_banda_detail, 0, 2, " DETALLE BANDA %d ", banda_seleccionada + 1);
    if (has_colors()) wattroff(win_banda_detail, COLOR_PAIR(6));

    if (banda_seleccionada >= datos_compartidos->num_bandas) {
        mvwprintw(win_banda_detail, 2, 2, "Banda no válida");
        wrefresh(win_banda_detail);
        return;
    }

    Banda *banda = &datos_compartidos->bandas[banda_seleccionada];
    pthread_mutex_lock(&banda->mutex);

    // Estado actual
    mvwprintw(win_banda_detail, 2, 2, "📍 ESTADO:");
    if (banda->pausada) {
        if (has_colors()) wattron(win_banda_detail, COLOR_PAIR(2));
        mvwprintw(win_banda_detail, 3, 4, "⏸️  PAUSADA");
        if (has_colors()) wattroff(win_banda_detail, COLOR_PAIR(2));
    } else if (!banda->activa) {
        if (has_colors()) wattron(win_banda_detail, COLOR_PAIR(3));
        mvwprintw(win_banda_detail, 3, 4, "❌ INACTIVA");
        if (has_colors()) wattroff(win_banda_detail, COLOR_PAIR(3));
    } else {
        if (has_colors()) wattron(win_banda_detail, COLOR_PAIR(1));
        mvwprintw(win_banda_detail, 3, 4, "✅ ACTIVA");
        if (has_colors()) wattroff(win_banda_detail, COLOR_PAIR(1));
    }

    mvwprintw(win_banda_detail, 4, 4, "Estado: %s", banda->estado_actual);

    // Orden actual
    mvwprintw(win_banda_detail, 6, 2, "🍔 ORDEN ACTUAL:");
    if (banda->procesando_orden) {
        mvwprintw(win_banda_detail, 7, 4, "• ID: #%d", banda->orden_actual.id_orden);
        mvwprintw(win_banda_detail, 8, 4, "• Tipo: %s", banda->orden_actual.nombre_hamburguesa);
        mvwprintw(win_banda_detail, 9, 4, "• Progreso: %d/%d ingredientes", 
                 banda->orden_actual.paso_actual, banda->orden_actual.num_ingredientes);
        if (strlen(banda->ingrediente_actual) > 0) {
            mvwprintw(win_banda_detail, 10, 4, "• Agregando: %s", banda->ingrediente_actual);
        }
    } else {
        mvwprintw(win_banda_detail, 7, 4, "Sin orden asignada");
    }

    // Estadísticas
    mvwprintw(win_banda_detail, 12, 2, "📈 ESTADÍSTICAS:");
    mvwprintw(win_banda_detail, 13, 4, "• Hamburguesas procesadas: %d", banda->hamburguesas_procesadas);

    // Inventario crítico
    mvwprintw(win_banda_detail, 15, 2, "📦 INVENTARIO CRÍTICO:");
    int items_criticos = 0;
    int linea_inv = 16;
    
    for (int j = 0; j < MAX_INGREDIENTES && items_criticos < 8; j++) {
        pthread_mutex_lock(&banda->dispensadores[j].mutex);
        int cantidad = banda->dispensadores[j].cantidad;
        
        if (cantidad == 0 || cantidad <= UMBRAL_INVENTARIO_BAJO) {
            int color = (cantidad == 0) ? 3 : 2;
            if (has_colors()) wattron(win_banda_detail, COLOR_PAIR(color));
            
            char nombre_corto[15];
            strncpy(nombre_corto, banda->dispensadores[j].nombre, 14);
            nombre_corto[14] = '\0';
            
            mvwprintw(win_banda_detail, linea_inv, 4, "• %-14s: %2d %s", 
                     nombre_corto, cantidad, 
                     cantidad == 0 ? "[AGOTADO]" : "[CRÍTICO]");
            
            if (has_colors()) wattroff(win_banda_detail, COLOR_PAIR(color));
            linea_inv++;
            items_criticos++;
        }
        pthread_mutex_unlock(&banda->dispensadores[j].mutex);
    }
    
    if (items_criticos == 0) {
        if (has_colors()) wattron(win_banda_detail, COLOR_PAIR(1));
        mvwprintw(win_banda_detail, linea_inv, 4, "✅ Inventario en niveles normales");
        if (has_colors()) wattroff(win_banda_detail, COLOR_PAIR(1));
    }

    pthread_mutex_unlock(&banda->mutex);
    wrefresh(win_banda_detail);
}

void mostrar_inventario_global()
{
    werase(win_banda_detail);
    
    if (has_colors()) wattron(win_banda_detail, COLOR_PAIR(4));
    wborder(win_banda_detail, '|', '|', '-', '-', '+', '+', '+', '+');
    mvwprintw(win_banda_detail, 0, 2, " INVENTARIO GLOBAL ");
    if (has_colors()) wattroff(win_banda_detail, COLOR_PAIR(4));

    mvwprintw(win_banda_detail, 2, 2, "📊 RESUMEN POR INGREDIENTE:");
    
    for (int ing = 0; ing < MAX_INGREDIENTES; ing++) {
        int total_ingrediente = 0;
        int bandas_agotadas = 0;
        int bandas_criticas = 0;
        
        // Calcular totales
        for (int banda = 0; banda < datos_compartidos->num_bandas; banda++) {
            pthread_mutex_lock(&datos_compartidos->bandas[banda].dispensadores[ing].mutex);
            int cantidad = datos_compartidos->bandas[banda].dispensadores[ing].cantidad;
            total_ingrediente += cantidad;
            
            if (cantidad == 0) bandas_agotadas++;
            else if (cantidad <= UMBRAL_INVENTARIO_BAJO) bandas_criticas++;
            
            pthread_mutex_unlock(&datos_compartidos->bandas[banda].dispensadores[ing].mutex);
        }
        
        // Mostrar solo primeros 12 ingredientes para no llenar la pantalla
        if (ing < 12) {
            int linea = 4 + ing;
            char nombre_corto[15];
            strncpy(nombre_corto, ingredientes_base[ing], 14);
            nombre_corto[14] = '\0';
            
            int color = 1; // Verde por defecto
            if (bandas_agotadas > 0) color = 3; // Rojo si hay agotadas
            else if (bandas_criticas > 0) color = 2; // Amarillo si hay críticas
            
            if (has_colors()) wattron(win_banda_detail, COLOR_PAIR(color));
            
            mvwprintw(win_banda_detail, linea, 2, "%-14s: Total:%3d", 
                     nombre_corto, total_ingrediente);
            
            if (bandas_agotadas > 0) {
                mvwprintw(win_banda_detail, linea, 30, "❌%d", bandas_agotadas);
            }
            if (bandas_criticas > 0) {
                mvwprintw(win_banda_detail, linea, 35, "⚠️%d", bandas_criticas);
            }
            
            if (has_colors()) wattroff(win_banda_detail, COLOR_PAIR(color));
        }
    }
    
    wrefresh(win_banda_detail);
}

void mostrar_comandos_disponibles()
{
    werase(win_commands);
    
    if (has_colors()) wattron(win_commands, COLOR_PAIR(5));
    wborder(win_commands, '|', '|', '-', '-', '+', '+', '+', '+');
    mvwprintw(win_commands, 0, 2, " COMANDOS DISPONIBLES ");
    if (has_colors()) wattroff(win_commands, COLOR_PAIR(5));

    mvwprintw(win_commands, 1, 2, "NAVEGACIÓN:");
    mvwprintw(win_commands, 2, 2, "  ↑/↓  Cambiar banda    TAB  Cambiar vista");
    mvwprintw(win_commands, 3, 2, "CONTROL:");
    mvwprintw(win_commands, 4, 2, "  ESPACIO Pausar/Reanudar  R  Reabastecer banda");
    mvwprintw(win_commands, 5, 2, "  H  Ayuda    Q  Salir");

    wrefresh(win_commands);
}

void mostrar_estado_sistema()
{
    werase(win_status);
    
    if (has_colors()) wattron(win_status, COLOR_PAIR(4));
    wborder(win_status, '|', '|', '-', '-', '+', '+', '+', '+');
    
    switch(modo_vista) {
        case 0: mvwprintw(win_status, 0, 2, " VISTA GENERAL "); break;
        case 1: mvwprintw(win_status, 0, 2, " DETALLE BANDA %d ", banda_seleccionada + 1); break;
        case 2: mvwprintw(win_status, 0, 2, " INVENTARIO GLOBAL "); break;
    }
    
    if (has_colors()) wattroff(win_status, COLOR_PAIR(4));

    // Estado del sistema
    if (datos_compartidos->sistema_activo) {
        if (has_colors()) wattron(win_status, COLOR_PAIR(1));
        mvwprintw(win_status, 2, 2, "🟢 Sistema ACTIVO");
        if (has_colors()) wattroff(win_status, COLOR_PAIR(1));
    } else {
        if (has_colors()) wattron(win_status, COLOR_PAIR(3));
        mvwprintw(win_status, 2, 2, "🔴 Sistema INACTIVO");
        if (has_colors()) wattroff(win_status, COLOR_PAIR(3));
    }

    // Hora actual
    time_t ahora = time(NULL);
    struct tm *info_tiempo = localtime(&ahora);
    mvwprintw(win_status, 3, 2, "⏰ %02d:%02d:%02d", 
             info_tiempo->tm_hour, info_tiempo->tm_min, info_tiempo->tm_sec);

    // Banda seleccionada
    if (has_colors()) wattron(win_status, COLOR_PAIR(6));
    mvwprintw(win_status, 4, 2, "👆 Banda seleccionada: %d", banda_seleccionada + 1);
    if (has_colors()) wattroff(win_status, COLOR_PAIR(6));

    wrefresh(win_status);
}

// ═══════════════════════════════════════════════════════════════
// FUNCIONES DE CONTROL
// ═══════════════════════════════════════════════════════════════

void procesar_comando(int ch)
{
    switch (ch) {
    case 'q':
    case 'Q':
        return;

    case 'h':
    case 'H':
        mostrar_ayuda_detallada();
        break;

    case KEY_UP:
        cambiar_banda_seleccionada(-1);
        break;

    case KEY_DOWN:
        cambiar_banda_seleccionada(1);
        break;

    case '\t':
    case KEY_RIGHT:
        modo_vista = (modo_vista + 1) % 3;
        break;

    case KEY_LEFT:
        modo_vista = (modo_vista - 1 + 3) % 3;
        break;

    case ' ': // ESPACIO
        pausar_reanudar_banda(banda_seleccionada);
        break;

    case 'r':
    case 'R':
        reabastecer_banda_completa(banda_seleccionada);
        break;

    case 'a': // Reabastecer todos los inventarios
        for (int i = 0; i < datos_compartidos->num_bandas; i++) {
            reabastecer_banda_completa(i);
        }
        mostrar_mensaje_temporal("✅ Todas las bandas reabastecidas");
        break;

    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
        {
            int banda_num = ch - '1';
            if (banda_num < datos_compartidos->num_bandas) {
                banda_seleccionada = banda_num;
            }
        }
        break;
    }
}

void cambiar_banda_seleccionada(int direccion)
{
    banda_seleccionada += direccion;
    if (banda_seleccionada < 0) {
        banda_seleccionada = datos_compartidos->num_bandas - 1;
    }
    if (banda_seleccionada >= datos_compartidos->num_bandas) {
        banda_seleccionada = 0;
    }
}

void pausar_reanudar_banda(int banda_id)
{
    if (banda_id >= 0 && banda_id < datos_compartidos->num_bandas) {
        Banda *banda = &datos_compartidos->bandas[banda_id];
        
        if (banda->pausada) {
            banda->pausada = 0;
            pthread_cond_signal(&banda->condicion);
            mostrar_mensaje_temporal("✅ Banda REANUDADA");
        } else {
            banda->pausada = 1;
            mostrar_mensaje_temporal("⏸️  Banda PAUSADA");
        }
    }
}

void reabastecer_banda_completa(int banda_id)
{
    if (banda_id >= 0 && banda_id < datos_compartidos->num_bandas) {
        for (int i = 0; i < MAX_INGREDIENTES; i++) {
            pthread_mutex_lock(&datos_compartidos->bandas[banda_id].dispensadores[i].mutex);
            datos_compartidos->bandas[banda_id].dispensadores[i].cantidad = CAPACIDAD_DISPENSADOR;
            pthread_mutex_unlock(&datos_compartidos->bandas[banda_id].dispensadores[i].mutex);
        }
        
        datos_compartidos->bandas[banda_id].necesita_reabastecimiento = 0;
        datos_compartidos->bandas[banda_id].ultima_alerta_inventario = 0;
        
        char mensaje[50];
        snprintf(mensaje, sizeof(mensaje), "✅ Banda %d REABASTECIDA", banda_id + 1);
        mostrar_mensaje_temporal(mensaje);
    }
}

void mostrar_mensaje_temporal(const char *mensaje)
{
    // Mostrar mensaje en la línea inferior de la pantalla
    int altura, ancho;
    getmaxyx(stdscr, altura, ancho);
    
    if (has_colors()) attron(COLOR_PAIR(7));
    mvprintw(altura - 1, 0, "%s", mensaje);
    for (int i = strlen(mensaje); i < ancho; i++) {
        addch(' ');
    }
    if (has_colors()) attroff(COLOR_PAIR(7));
    
    refresh();
    usleep(1500000); // 1.5 segundos
    
    // Limpiar línea
    mvprintw(altura - 1, 0, "%*s", ancho, "");
    refresh();
}

void mostrar_ayuda_detallada()
{
    clear();
    
    if (has_colors()) attron(COLOR_PAIR(4));
    mvprintw(2, 5, "╔════════════════════════════════════════════════════════════════════════════════╗");
    mvprintw(3, 5, "║                           PANEL DE CONTROL - AYUDA                             ║");
    mvprintw(4, 5, "╠════════════════════════════════════════════════════════════════════════════════╣");
    if (has_colors()) attroff(COLOR_PAIR(4));
    
    mvprintw(6, 5, "║ NAVEGACIÓN:                                                                    ║");
    mvprintw(7, 5, "║   ↑/↓              Cambiar banda seleccionada                                  ║");
    mvprintw(8, 5, "║   TAB / ←/→        Cambiar vista (General/Detalle/Inventario)                 ║");
    mvprintw(9, 5, "║   1-9              Seleccionar banda directamente                             ║");
    mvprintw(10, 5, "║                                                                                ║");
    mvprintw(11, 5, "║ CONTROL DE BANDAS:                                                             ║");
    mvprintw(12, 5, "║   ESPACIO          Pausar/Reanudar banda seleccionada                        ║");
    mvprintw(13, 5, "║   R                Reabastecer banda seleccionada completamente               ║");
    mvprintw(14, 5, "║   A                Reabastecer TODAS las bandas                               ║");
    mvprintw(15, 5, "║                                                                                ║");
    mvprintw(16, 5, "║ VISTAS DISPONIBLES:                                                            ║");
    mvprintw(17, 5, "║   Vista General    Resumen de todas las bandas y estadísticas                ║");
    mvprintw(18, 5, "║   Detalle Banda    Información específica de la banda seleccionada           ║");
    mvprintw(19, 5, "║   Inventario Global Resumen de inventarios por ingrediente                   ║");
    mvprintw(20, 5, "║                                                                                ║");
    mvprintw(21, 5, "║ INDICADORES:                                                                   ║");
    mvprintw(22, 5, "║   ✅ Verde         Funcionamiento normal                                       ║");
    mvprintw(23, 5, "║   ⚠️  Amarillo      Advertencia/Crítico                                        ║");
    mvprintw(24, 5, "║   ❌ Rojo          Error/Agotado/Inactivo                                       ║");
    mvprintw(25, 5, "║                                                                                ║");
    mvprintw(26, 5, "║ SISTEMA:                                                                       ║");
    mvprintw(27, 5, "║   H                Mostrar esta ayuda                                         ║");
    mvprintw(28, 5, "║   Q                Salir del panel de control                                 ║");
    mvprintw(29, 5, "╚════════════════════════════════════════════════════════════════════════════════╝");
    
    if (has_colors()) attron(COLOR_PAIR(5));
    mvprintw(31, 5, "Presiona cualquier tecla para continuar...");
    if (has_colors()) attroff(COLOR_PAIR(5));
    
    refresh();
    getch();
}

void limpiar_ncurses()
{
    if (win_main) delwin(win_main);
    if (win_banda_detail) delwin(win_banda_detail);
    if (win_commands) delwin(win_commands);
    if (win_status) delwin(win_status);
    endwin();
}

// ═══════════════════════════════════════════════════════════════
// FUNCIÓN PRINCIPAL
// ═══════════════════════════════════════════════════════════════

int main()
{
    printf("🚀 Iniciando Panel de Control del Sistema de Hamburguesas...\n");
    printf("📡 Conectando con el sistema principal...\n");

    // Conectar con el sistema principal
    conectar_memoria_compartida();
    printf("✅ Conexión establecida exitosamente\n");

    // Verificar que el sistema esté activo
    if (!datos_compartidos->sistema_activo) {
        printf("❌ El sistema principal no está activo\n");
        printf("   Inicia primero: ./burger_system -n 4\n");
        exit(1);
    }

    printf("🎛️  Iniciando interfaz de control...\n");
    sleep(1);

    // Inicializar interfaz
    inicializar_ncurses();

    // Bucle principal
    int ch;
    time_t ultimo_refresh = time(NULL);

    while ((ch = getch()) != 'q' && ch != 'Q') {
        time_t ahora = time(NULL);

        // Actualizar interfaz cada segundo o con comando
        if (ahora - ultimo_refresh >= 1 || ch != ERR) {
            // Verificar si el sistema sigue activo
            if (!datos_compartidos->sistema_activo) {
                break;
            }

            // Mostrar interfaces según el modo
            switch (modo_vista) {
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
            }

            mostrar_comandos_disponibles();
            mostrar_estado_sistema();
            ultimo_refresh = ahora;
        }

        // Procesar comando si hay uno
        if (ch != ERR) {
            procesar_comando(ch);
            if (ch == 'q' || ch == 'Q') break;
        }

        usleep(50000); // 50ms de espera para no saturar CPU
    }

    // Limpiar recursos
    limpiar_ncurses();

    printf("\n✅ Panel de control terminado correctamente\n");
    printf("📊 Estadísticas finales:\n");
    printf("   • Órdenes procesadas: %d\n", datos_compartidos->total_ordenes_procesadas);
    printf("   • Órdenes en cola: %d\n", datos_compartidos->cola_espera.tamano);
    printf("   • Bandas monitoreadas: %d\n", datos_compartidos->num_bandas);
    
    return 0;
}