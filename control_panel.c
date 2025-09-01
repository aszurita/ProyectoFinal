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
#define MAX_NOMBRE_INGREDIENTE 50
#define CAPACIDAD_DISPENSADOR 20
#define NUM_TIPOS_HAMBURGUESA 6

// Estructura para ingredientes por banda
typedef struct
{
    char nombre[MAX_NOMBRE_INGREDIENTE];
    int cantidad;
    pthread_mutex_t mutex;
} Ingrediente;

// Definición de tipos de hamburguesas (igual que en el sistema principal)
typedef struct
{
    char nombre[50];
    char ingredientes[10][MAX_NOMBRE_INGREDIENTE];
    int num_ingredientes;
    float precio;
} TipoHamburguesa;

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
    pthread_t hilo;
    pthread_mutex_t mutex;
    pthread_cond_t condicion;
    char estado_actual[100];
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
WINDOW *win_bandas;
WINDOW *win_inventario;
WINDOW *win_comandos;
WINDOW *win_menu;
int banda_seleccionada = 0;

// Menú de hamburguesas (igual que en el sistema principal)
TipoHamburguesa menu_hamburguesas[NUM_TIPOS_HAMBURGUESA] = {
    {"Clásica", {"pan_inferior", "carne", "lechuga", "tomate", "pan_superior"}, 5, 8.50},
    {"Cheeseburger", {"pan_inferior", "carne", "queso", "lechuga", "tomate", "pan_superior"}, 6, 9.25},
    {"BBQ Bacon", {"pan_inferior", "carne", "bacon", "queso", "cebolla", "salsa_bbq", "pan_superior"}, 7, 11.75},
    {"Vegetariana", {"pan_inferior", "hamburguesa_vegetal", "lechuga", "tomate", "aguacate", "mayonesa", "pan_superior"}, 7, 10.25},
    {"Deluxe", {"pan_inferior", "carne", "queso", "bacon", "lechuga", "tomate", "cebolla", "mayonesa", "pan_superior"}, 9, 13.50},
    {"Spicy Mexican", {"pan_inferior", "carne", "queso", "jalapeños", "tomate", "cebolla", "salsa_picante", "pan_superior"}, 8, 12.00}};

// Prototipos
void inicializar_ncurses();
void conectar_memoria_compartida();
void mostrar_interfaz_completa();
void procesar_comando(int ch);
void pausar_banda(int banda_id);
void reanudar_banda(int banda_id);
void reanudar_todas_bandas();
void reabastecer_banda_especifica(int banda_id);
void reabastecer_ingrediente_banda(int banda_id, int ingrediente_id);
void mostrar_menu_hamburguesas_panel();
void cambiar_banda_seleccionada(int direccion);
void limpiar_ncurses();
void mostrar_ayuda_control();

void inicializar_ncurses()
{
    initscr();
    cbreak();
    noecho();
    nodelay(stdscr, TRUE);
    keypad(stdscr, TRUE);
    curs_set(0);

    // Verificar si el terminal soporta colores
    if (has_colors())
    {
        start_color();
        init_pair(1, COLOR_GREEN, COLOR_BLACK);   // Verde para estado activo
        init_pair(2, COLOR_YELLOW, COLOR_BLACK);  // Amarillo para pausado
        init_pair(3, COLOR_RED, COLOR_BLACK);     // Rojo para alertas
        init_pair(4, COLOR_CYAN, COLOR_BLACK);    // Cian para encabezados
        init_pair(5, COLOR_WHITE, COLOR_BLUE);    // Blanco sobre azul para selección
        init_pair(6, COLOR_MAGENTA, COLOR_BLACK); // Magenta para bandas seleccionadas
    }

    // Crear ventanas
    int altura_terminal, ancho_terminal;
    getmaxyx(stdscr, altura_terminal, ancho_terminal);

    // Ventana de bandas (superior izquierda)
    win_bandas = newwin(altura_terminal / 2, ancho_terminal / 2, 0, 0);

    // Ventana de inventario (superior derecha)
    win_inventario = newwin(altura_terminal / 2, ancho_terminal / 2, 0, ancho_terminal / 2);

    // Ventana de menú (inferior izquierda)
    win_menu = newwin(altura_terminal / 2, ancho_terminal / 2, altura_terminal / 2, 0);

    // Ventana de comandos (inferior derecha)
    win_comandos = newwin(altura_terminal / 2, ancho_terminal / 2, altura_terminal / 2, ancho_terminal / 2);

    // Habilitar teclas especiales para todas las ventanas
    keypad(win_bandas, TRUE);
    keypad(win_inventario, TRUE);
    keypad(win_menu, TRUE);
    keypad(win_comandos, TRUE);

    refresh();
}

void conectar_memoria_compartida()
{
    int shm_fd = shm_open("/burger_system", O_RDWR, 0666);
    if (shm_fd == -1)
    {
        endwin();
        printf("Error: No se pudo conectar con el sistema principal.\n");
        printf("Asegúrese de que el sistema esté ejecutándose.\n");
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

void mostrar_interfaz_completa()
{
    // Limpiar todas las ventanas
    werase(win_bandas);
    werase(win_inventario);
    werase(win_menu);
    werase(win_comandos);

    // ═══ VENTANA DE BANDAS ═══
    if (has_colors())
        wattron(win_bandas, COLOR_PAIR(4));
    wborder(win_bandas, '|', '|', '-', '-', '+', '+', '+', '+');
    mvwprintw(win_bandas, 0, 2, " ESTADO DE BANDAS ");
    if (has_colors())
        wattroff(win_bandas, COLOR_PAIR(4));

    mvwprintw(win_bandas, 1, 2, "Total procesadas: %d | En cola: %d",
              datos_compartidos->total_ordenes_procesadas,
              datos_compartidos->cola_espera.tamano);

    for (int i = 0; i < datos_compartidos->num_bandas && i < 8; i++)
    {
        Banda *banda = &datos_compartidos->bandas[i];

        int color_pair = 1; // Verde por defecto
        if (!banda->activa)
            color_pair = 3; // Rojo
        else if (banda->pausada)
            color_pair = 2; // Amarillo
        else if (i == banda_seleccionada)
            color_pair = 6; // Magenta para seleccionada

        if (has_colors())
            wattron(win_bandas, COLOR_PAIR(color_pair));

        mvwprintw(win_bandas, 3 + i * 2, 2, "BANDA %d: %s", i,
                  banda->pausada ? "PAUSADA" : (!banda->activa ? "INACTIVA" : "ACTIVA"));

        // Mostrar estado actual con truncamiento si es muy largo
        char estado_corto[30];
        strncpy(estado_corto, banda->estado_actual, 29);
        estado_corto[29] = '\0';
        mvwprintw(win_bandas, 4 + i * 2, 2, "  Estado: %-25s Procesadas: %d",
                  estado_corto, banda->hamburguesas_procesadas);

        if (has_colors())
            wattroff(win_bandas, COLOR_PAIR(color_pair));
    }

    // ═══ VENTANA DE INVENTARIO (BANDA SELECCIONADA) ═══
    if (has_colors())
        wattron(win_inventario, COLOR_PAIR(4));
    wborder(win_inventario, '|', '|', '-', '-', '+', '+', '+', '+');
    mvwprintw(win_inventario, 0, 2, " INVENTARIO BANDA %d ", banda_seleccionada);
    if (has_colors())
        wattroff(win_inventario, COLOR_PAIR(4));

    if (banda_seleccionada < datos_compartidos->num_bandas)
    {
        Banda *banda = &datos_compartidos->bandas[banda_seleccionada];

        // Mostrar si está procesando una orden
        if (banda->procesando_orden)
        {
            mvwprintw(win_inventario, 2, 2, "Procesando: %s (Orden #%d)",
                      banda->orden_actual.nombre_hamburguesa,
                      banda->orden_actual.id_orden);
            mvwprintw(win_inventario, 3, 2, "Progreso: %d/%d ingredientes",
                      banda->orden_actual.paso_actual,
                      banda->orden_actual.num_ingredientes);
        }
        else
        {
            mvwprintw(win_inventario, 2, 2, "Estado: %s", banda->estado_actual);
        }

        // Mostrar dispensadores
        mvwprintw(win_inventario, 5, 2, "DISPENSADORES:");
        for (int j = 0; j < 10 && j < MAX_INGREDIENTES; j++)
        {
            pthread_mutex_lock(&banda->dispensadores[j].mutex);
            int cantidad = banda->dispensadores[j].cantidad;
            char *nombre = banda->dispensadores[j].nombre;

            int color_pair = 1; // Verde
            if (cantidad <= 2)
                color_pair = 2; // Amarillo
            if (cantidad == 0)
                color_pair = 3; // Rojo

            if (has_colors())
                wattron(win_inventario, COLOR_PAIR(color_pair));
            mvwprintw(win_inventario, 6 + j, 2, "%-15s: %2d [%c]",
                      nombre, cantidad, 'a' + j);
            if (has_colors())
                wattroff(win_inventario, COLOR_PAIR(color_pair));

            pthread_mutex_unlock(&banda->dispensadores[j].mutex);
        }
    }

    // ═══ VENTANA DE MENÚ ═══
    mostrar_menu_hamburguesas_panel();

    // ═══ VENTANA DE COMANDOS ═══
    if (has_colors())
        wattron(win_comandos, COLOR_PAIR(5));
    wborder(win_comandos, '|', '|', '-', '-', '+', '+', '+', '+');
    mvwprintw(win_comandos, 0, 2, " COMANDOS DISPONIBLES ");
    if (has_colors())
        wattroff(win_comandos, COLOR_PAIR(5));

    mvwprintw(win_comandos, 2, 2, "NAVEGACIÓN:");
    mvwprintw(win_comandos, 3, 2, "  ↑/↓: Cambiar banda seleccionada");
    mvwprintw(win_comandos, 4, 2, "  ESPACIO: Pausar/Reanudar banda");

    mvwprintw(win_comandos, 6, 2, "CONTROL BANDAS:");
    mvwprintw(win_comandos, 7, 2, "  1-9: Pausar banda específica");
    mvwprintw(win_comandos, 8, 2, "  R: Reanudar todas las bandas");

    mvwprintw(win_comandos, 10, 2, "INVENTARIO:");
    mvwprintw(win_comandos, 11, 2, "  a-j: Reabastecer ingrediente");
    mvwprintw(win_comandos, 12, 2, "  T: Reabastecer banda completa");

    mvwprintw(win_comandos, 14, 2, "SISTEMA:");
    mvwprintw(win_comandos, 15, 2, "  H: Ayuda | Q: Salir");

    // Refrescar todas las ventanas
    wrefresh(win_bandas);
    wrefresh(win_inventario);
    wrefresh(win_menu);
    wrefresh(win_comandos);
    refresh();
}

void mostrar_menu_hamburguesas_panel()
{
    if (has_colors())
        wattron(win_menu, COLOR_PAIR(4));
    wborder(win_menu, '|', '|', '-', '-', '+', '+', '+', '+');
    mvwprintw(win_menu, 0, 2, " MENÚ DE HAMBURGUESAS ");
    if (has_colors())
        wattroff(win_menu, COLOR_PAIR(4));

    for (int i = 0; i < NUM_TIPOS_HAMBURGUESA; i++)
    {
        mvwprintw(win_menu, 2 + i * 2, 2, "%d. %-15s $%.2f",
                  i + 1, menu_hamburguesas[i].nombre, menu_hamburguesas[i].precio);

        // Mostrar algunos ingredientes principales
        char ingredientes_cortos[60] = "";
        for (int j = 0; j < 3 && j < menu_hamburguesas[i].num_ingredientes; j++)
        {
            if (j > 0)
                strcat(ingredientes_cortos, ", ");
            strncat(ingredientes_cortos, menu_hamburguesas[i].ingredientes[j], 10);
        }
        if (menu_hamburguesas[i].num_ingredientes > 3)
        {
            strcat(ingredientes_cortos, "...");
        }

        mvwprintw(win_menu, 3 + i * 2, 2, "   %s", ingredientes_cortos);
    }
}

void procesar_comando(int ch)
{
    switch (ch)
    {
    case 'q':
    case 'Q':
        return;

    case 'h':
    case 'H':
        mostrar_ayuda_control();
        break;

    case KEY_UP:
        cambiar_banda_seleccionada(-1);
        break;

    case KEY_DOWN:
        cambiar_banda_seleccionada(1);
        break;

    case ' ': // ESPACIO - pausar/reanudar banda seleccionada
        if (datos_compartidos->bandas[banda_seleccionada].pausada)
        {
            reanudar_banda(banda_seleccionada);
        }
        else
        {
            pausar_banda(banda_seleccionada);
        }
        break;

    case 'r':
    case 'R':
        reanudar_todas_bandas();
        break;

    case 't':
    case 'T':
        reabastecer_banda_especifica(banda_seleccionada);
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
        pausar_banda(ch - '1');
        break;

    case 'a':
    case 'b':
    case 'c':
    case 'd':
    case 'e':
    case 'f':
    case 'g':
    case 'i':
    case 'j':
        reabastecer_ingrediente_banda(banda_seleccionada, ch - 'a');
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

void pausar_banda(int banda_id)
{
    if (banda_id >= 0 && banda_id < datos_compartidos->num_bandas)
    {
        datos_compartidos->bandas[banda_id].pausada = 1;

        // Mostrar mensaje temporal en la parte inferior
        mvprintw(LINES - 1, 0, "Banda %d PAUSADA - Presiona cualquier tecla...", banda_id);
        refresh();
        getch();
        mvprintw(LINES - 1, 0, "%*s", COLS, ""); // Limpiar línea
    }
}

void reanudar_banda(int banda_id)
{
    if (banda_id >= 0 && banda_id < datos_compartidos->num_bandas)
    {
        if (datos_compartidos->bandas[banda_id].pausada)
        {
            datos_compartidos->bandas[banda_id].pausada = 0;
            pthread_cond_signal(&datos_compartidos->bandas[banda_id].condicion);

            mvprintw(LINES - 1, 0, "Banda %d REANUDADA - Presiona cualquier tecla...", banda_id);
            refresh();
            getch();
            mvprintw(LINES - 1, 0, "%*s", COLS, ""); // Limpiar línea
        }
    }
}

void reanudar_todas_bandas()
{
    int bandas_reanudadas = 0;
    for (int i = 0; i < datos_compartidos->num_bandas; i++)
    {
        if (datos_compartidos->bandas[i].pausada)
        {
            datos_compartidos->bandas[i].pausada = 0;
            pthread_cond_signal(&datos_compartidos->bandas[i].condicion);
            bandas_reanudadas++;
        }
    }

    mvprintw(LINES - 1, 0, "%d bandas REANUDADAS - Presiona cualquier tecla...", bandas_reanudadas);
    refresh();
    getch();
    mvprintw(LINES - 1, 0, "%*s", COLS, ""); // Limpiar línea
}

void reabastecer_banda_especifica(int banda_id)
{
    if (banda_id >= 0 && banda_id < datos_compartidos->num_bandas)
    {
        for (int i = 0; i < MAX_INGREDIENTES; i++)
        {
            pthread_mutex_lock(&datos_compartidos->bandas[banda_id].dispensadores[i].mutex);
            datos_compartidos->bandas[banda_id].dispensadores[i].cantidad = CAPACIDAD_DISPENSADOR;
            pthread_mutex_unlock(&datos_compartidos->bandas[banda_id].dispensadores[i].mutex);
        }

        mvprintw(LINES - 1, 0, "Banda %d REABASTECIDA completamente - Presiona cualquier tecla...", banda_id);
        refresh();
        getch();
        mvprintw(LINES - 1, 0, "%*s", COLS, ""); // Limpiar línea
    }
}

void reabastecer_ingrediente_banda(int banda_id, int ingrediente_id)
{
    if (banda_id >= 0 && banda_id < datos_compartidos->num_bandas &&
        ingrediente_id >= 0 && ingrediente_id < MAX_INGREDIENTES)
    {

        pthread_mutex_lock(&datos_compartidos->bandas[banda_id].dispensadores[ingrediente_id].mutex);
        datos_compartidos->bandas[banda_id].dispensadores[ingrediente_id].cantidad = CAPACIDAD_DISPENSADOR;
        char *nombre = datos_compartidos->bandas[banda_id].dispensadores[ingrediente_id].nombre;
        pthread_mutex_unlock(&datos_compartidos->bandas[banda_id].dispensadores[ingrediente_id].mutex);

        mvprintw(LINES - 1, 0, "Banda %d: %s REABASTECIDO - Presiona cualquier tecla...",
                 banda_id, nombre);
        refresh();
        getch();
        mvprintw(LINES - 1, 0, "%*s", COLS, ""); // Limpiar línea
    }
}

void mostrar_ayuda_control()
{
    clear();
    mvprintw(2, 5, "╔════════════════════════════════════════════════════════════════╗");
    mvprintw(3, 5, "║                    AYUDA DEL PANEL DE CONTROL                  ║");
    mvprintw(4, 5, "╠════════════════════════════════════════════════════════════════╣");
    mvprintw(5, 5, "║ NAVEGACIÓN:                                                    ║");
    mvprintw(6, 5, "║   ↑/↓           Cambiar banda seleccionada                     ║");
    mvprintw(7, 5, "║   ESPACIO       Pausar/Reanudar banda seleccionada            ║");
    mvprintw(8, 5, "║                                                                ║");
    mvprintw(9, 5, "║ CONTROL DE BANDAS:                                             ║");
    mvprintw(10, 5, "║   1-9           Pausar banda específica                        ║");
    mvprintw(11, 5, "║   R             Reanudar todas las bandas                      ║");
    mvprintw(12, 5, "║                                                                ║");
    mvprintw(13, 5, "║ CONTROL DE INVENTARIO:                                         ║");
    mvprintw(14, 5, "║   a-j           Reabastecer ingrediente específico (banda sel.)║");
    mvprintw(15, 5, "║   T             Reabastecer toda la banda seleccionada        ║");
    mvprintw(16, 5, "║                                                                ║");
    mvprintw(17, 5, "║ SISTEMA:                                                       ║");
    mvprintw(18, 5, "║   H             Mostrar esta ayuda                             ║");
    mvprintw(19, 5, "║   Q             Salir del panel de control                     ║");
    mvprintw(20, 5, "╚════════════════════════════════════════════════════════════════╝");
    mvprintw(22, 5, "Presione cualquier tecla para continuar...");

    refresh();
    getch();
}

void limpiar_ncurses()
{
    if (win_bandas)
        delwin(win_bandas);
    if (win_inventario)
        delwin(win_inventario);
    if (win_menu)
        delwin(win_menu);
    if (win_comandos)
        delwin(win_comandos);
    endwin();
}

int main()
{
    printf("Iniciando Panel de Control Avanzado del Sistema de Hamburguesas...\n");
    printf("Conectando con sistema principal...\n");

    // Conectar con el sistema principal
    conectar_memoria_compartida();

    // Inicializar interfaz
    inicializar_ncurses();

    printf("Panel de control conectado exitosamente\n");
    printf("Cambiando a interfaz gráfica avanzada...\n");
    sleep(2);

    // Bucle principal
    int ch;
    time_t ultimo_refresh = time(NULL);

    while ((ch = getch()) != 'q' && ch != 'Q')
    {
        time_t ahora = time(NULL);

        // Actualizar interfaz cada segundo o con comando
        if (ahora - ultimo_refresh >= 1 || ch != ERR)
        {
            if (!datos_compartidos->sistema_activo)
            {
                break; // Sistema principal terminado
            }

            mostrar_interfaz_completa();
            ultimo_refresh = ahora;
        }

        // Procesar comando si hay uno
        if (ch != ERR)
        {
            procesar_comando(ch);
        }

        usleep(50000); // 50ms de espera
    }

    // Limpiar recursos
    limpiar_ncurses();

    printf("Panel de control terminado\n");
    return 0;
}