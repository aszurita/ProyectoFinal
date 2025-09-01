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

#define MAX_BANDAS 10
#define MAX_INGREDIENTES 20
#define MAX_ORDENES 100
#define MAX_NOMBRE_INGREDIENTE 50
#define CAPACIDAD_DISPENSADOR 20

// Mismas estructuras que el sistema principal
typedef struct
{
    char nombre[MAX_NOMBRE_INGREDIENTE];
    int cantidad;
    pthread_mutex_t mutex;
} Ingrediente;

typedef struct
{
    int id_orden;
    char ingredientes_solicitados[MAX_INGREDIENTES][MAX_NOMBRE_INGREDIENTE];
    int num_ingredientes;
    time_t tiempo_creacion;
} Orden;

typedef struct
{
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

typedef struct
{
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
WINDOW *win_control;
WINDOW *win_estado;
WINDOW *win_comandos;
int pid_sistema = -1;

// Prototipos
void inicializar_ncurses();
void conectar_memoria_compartida();
void mostrar_interfaz_control_avanzada();
void procesar_comando(int ch);
void pausar_banda(int banda_id);
void reanudar_banda(int banda_id);
void reanudar_todas_bandas();
void reabastecer_ingrediente(int ingrediente_id);
void reabastecer_todos_ingredientes();
void encontrar_pid_sistema();
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
        init_pair(1, COLOR_GREEN, COLOR_BLACK);  // Verde para estado activo
        init_pair(2, COLOR_YELLOW, COLOR_BLACK); // Amarillo para pausado
        init_pair(3, COLOR_RED, COLOR_BLACK);    // Rojo para alertas
        init_pair(4, COLOR_CYAN, COLOR_BLACK);   // Cian para encabezados
        init_pair(5, COLOR_WHITE, COLOR_BLUE);   // Blanco sobre azul para selección
    }

    // Crear ventanas
    int altura_terminal, ancho_terminal;
    getmaxyx(stdscr, altura_terminal, ancho_terminal);

    // Ventana de control (izquierda)
    win_control = newwin(altura_terminal - 5, ancho_terminal / 2, 0, 0);

    // Ventana de estado (derecha)
    win_estado = newwin(altura_terminal - 5, ancho_terminal / 2, 0, ancho_terminal / 2);

    // Ventana de comandos (abajo)
    win_comandos = newwin(5, ancho_terminal, altura_terminal - 5, 0);

    // Habilitar teclas especiales para todas las ventanas
    keypad(win_control, TRUE);
    keypad(win_estado, TRUE);
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

void mostrar_interfaz_control_avanzada()
{
    // Limpiar ventanas
    werase(win_control);
    werase(win_estado);
    werase(win_comandos);

    // Ventana de control de bandas
    if (has_colors())
        wattron(win_control, COLOR_PAIR(4));
    wborder(win_control, '|', '|', '-', '-', '+', '+', '+', '+');
    mvwprintw(win_control, 0, 2, " CONTROL DE BANDAS ");
    if (has_colors())
        wattroff(win_control, COLOR_PAIR(4));

    for (int i = 0; i < datos_compartidos->num_bandas; i++)
    {
        Banda *banda = &datos_compartidos->bandas[i];

        int color_pair = 1; // Verde por defecto (activa)
        char estado[20];

        if (!banda->activa)
        {
            strcpy(estado, "INACTIVA");
            color_pair = 3; // Rojo
        }
        else if (banda->pausada)
        {
            strcpy(estado, "PAUSADA");
            color_pair = 2; // Amarillo
        }
        else if (banda->procesando_orden)
        {
            strcpy(estado, "PROCESANDO");
            color_pair = 1; // Verde
        }
        else
        {
            strcpy(estado, "ESPERANDO");
            color_pair = 1; // Verde
        }

        if (has_colors())
            wattron(win_control, COLOR_PAIR(color_pair));
        mvwprintw(win_control, 2 + i * 2, 2, "Banda %d: %-12s", i, estado);
        mvwprintw(win_control, 3 + i * 2, 2, "  Procesadas: %-6d [%c] Pausar [%c] Reanudar",
                  banda->hamburguesas_procesadas, '1' + i, 'A' + i);
        if (has_colors())
            wattroff(win_control, COLOR_PAIR(color_pair));
    }

    // Ventana de estado del sistema
    if (has_colors())
        wattron(win_estado, COLOR_PAIR(4));
    wborder(win_estado, '|', '|', '-', '-', '+', '+', '+', '+');
    mvwprintw(win_estado, 0, 2, " ESTADO DEL SISTEMA ");
    if (has_colors())
        wattroff(win_estado, COLOR_PAIR(4));

    mvwprintw(win_estado, 2, 2, "Total procesadas: %d", datos_compartidos->total_ordenes_procesadas);
    mvwprintw(win_estado, 3, 2, "En cola de espera: %d", datos_compartidos->cola_espera.tamano);

    mvwprintw(win_estado, 5, 2, "INVENTARIO:");
    for (int i = 0; i < datos_compartidos->num_ingredientes && i < 15; i++)
    {
        int cantidad = datos_compartidos->ingredientes[i].cantidad;
        char *nombre = datos_compartidos->ingredientes[i].nombre;

        int color_pair = 1; // Verde
        if (cantidad <= 2)
            color_pair = 2; // Amarillo
        if (cantidad == 0)
            color_pair = 3; // Rojo

        if (has_colors())
            wattron(win_estado, COLOR_PAIR(color_pair));
        mvwprintw(win_estado, 6 + i, 2, "%-15s: %2d [%c]",
                  nombre, cantidad, 'a' + i);
        if (has_colors())
            wattroff(win_estado, COLOR_PAIR(color_pair));
    }

    // Ventana de comandos
    if (has_colors())
        wattron(win_comandos, COLOR_PAIR(5));
    wborder(win_comandos, '|', '|', '-', '-', '+', '+', '+', '+');
    mvwprintw(win_comandos, 0, 2, " COMANDOS DISPONIBLES ");
    if (has_colors())
        wattroff(win_comandos, COLOR_PAIR(5));

    mvwprintw(win_comandos, 1, 2, "Bandas: [1-9] Pausar | [A-I] Reanudar | [R] Reanudar todas");
    mvwprintw(win_comandos, 2, 2, "Inventario: [a-o] Reabastecer ingrediente | [T] Todos | [S] Estado");
    mvwprintw(win_comandos, 3, 2, "Sistema: [H] Ayuda | [Q] Salir | [ENTER] Actualizar");

    // Refrescar todas las ventanas
    wrefresh(win_control);
    wrefresh(win_estado);
    wrefresh(win_comandos);
    refresh();
}

void procesar_comando(int ch)
{
    switch (ch)
    {
    case 'q':
    case 'Q':
        // Salir del panel de control
        return;

    case 'h':
    case 'H':
        mostrar_ayuda_control();
        break;

    case 'r':
    case 'R':
        reanudar_todas_bandas();
        break;

    case 't':
    case 'T':
        reabastecer_todos_ingredientes();
        break;

    case 's':
    case 'S':
        // Forzar actualización de estado
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

    case 'A':
    case 'B':
    case 'C':
    case 'D':
    case 'E':
    case 'F':
    case 'G':
    case 'I':
        reanudar_banda(ch - 'A');
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
    case 'k':
    case 'l':
    case 'm':
    case 'n':
    case 'o':
        reabastecer_ingrediente(ch - 'a');
        break;
    }
}

void pausar_banda(int banda_id)
{
    if (banda_id >= 0 && banda_id < datos_compartidos->num_bandas)
    {
        datos_compartidos->bandas[banda_id].pausada = 1;

        // Mostrar mensaje de confirmación
        mvwprintw(win_comandos, 4, 2, "Banda %d PAUSADA                    ", banda_id);
        wrefresh(win_comandos);
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

            mvwprintw(win_comandos, 4, 2, "Banda %d REANUDADA                 ", banda_id);
            wrefresh(win_comandos);
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

    mvwprintw(win_comandos, 4, 2, "%d bandas REANUDADAS               ", bandas_reanudadas);
    wrefresh(win_comandos);
}

void reabastecer_ingrediente(int ingrediente_id)
{
    if (ingrediente_id >= 0 && ingrediente_id < datos_compartidos->num_ingredientes)
    {
        pthread_mutex_lock(&datos_compartidos->ingredientes[ingrediente_id].mutex);
        datos_compartidos->ingredientes[ingrediente_id].cantidad = CAPACIDAD_DISPENSADOR;
        pthread_mutex_unlock(&datos_compartidos->ingredientes[ingrediente_id].mutex);

        mvwprintw(win_comandos, 4, 2, "Ingrediente %s REABASTECIDO      ",
                  datos_compartidos->ingredientes[ingrediente_id].nombre);
        wrefresh(win_comandos);
    }
}

void reabastecer_todos_ingredientes()
{
    for (int i = 0; i < datos_compartidos->num_ingredientes; i++)
    {
        pthread_mutex_lock(&datos_compartidos->ingredientes[i].mutex);
        datos_compartidos->ingredientes[i].cantidad = CAPACIDAD_DISPENSADOR;
        pthread_mutex_unlock(&datos_compartidos->ingredientes[i].mutex);
    }

    mvwprintw(win_comandos, 4, 2, "TODOS los ingredientes REABASTECIDOS");
    wrefresh(win_comandos);
}

void encontrar_pid_sistema()
{
    FILE *fp = popen("pgrep burger_system", "r");
    if (fp != NULL)
    {
        if (fscanf(fp, "%d", &pid_sistema) != 1)
        {
            pid_sistema = -1;
        }
        pclose(fp);
    }
}

void mostrar_ayuda_control()
{
    WINDOW *win_ayuda = newwin(20, 60, 5, 10);
    if (has_colors())
        wattron(win_ayuda, COLOR_PAIR(4));
    wborder(win_ayuda, '|', '|', '-', '-', '+', '+', '+', '+');
    mvwprintw(win_ayuda, 0, 2, " AYUDA DEL PANEL DE CONTROL ");
    if (has_colors())
        wattroff(win_ayuda, COLOR_PAIR(4));

    mvwprintw(win_ayuda, 2, 2, "CONTROL DE BANDAS:");
    mvwprintw(win_ayuda, 3, 2, "  1-9: Pausar banda correspondiente");
    mvwprintw(win_ayuda, 4, 2, "  A-I: Reanudar banda correspondiente");
    mvwprintw(win_ayuda, 5, 2, "  R:   Reanudar todas las bandas");

    mvwprintw(win_ayuda, 7, 2, "CONTROL DE INVENTARIO:");
    mvwprintw(win_ayuda, 8, 2, "  a-o: Reabastecer ingrediente específico");
    mvwprintw(win_ayuda, 9, 2, "  T:   Reabastecer todos los ingredientes");

    mvwprintw(win_ayuda, 11, 2, "COMANDOS DEL SISTEMA:");
    mvwprintw(win_ayuda, 12, 2, "  S:     Actualizar estado");
    mvwprintw(win_ayuda, 13, 2, "  H:     Mostrar esta ayuda");
    mvwprintw(win_ayuda, 14, 2, "  Q:     Salir del panel de control");
    mvwprintw(win_ayuda, 15, 2, "  ENTER: Refrescar pantalla");

    mvwprintw(win_ayuda, 17, 2, "Presione cualquier tecla para continuar...");

    wrefresh(win_ayuda);
    wgetch(win_ayuda);
    delwin(win_ayuda);
}

void limpiar_ncurses()
{
    if (win_control)
        delwin(win_control);
    if (win_estado)
        delwin(win_estado);
    if (win_comandos)
        delwin(win_comandos);
    endwin();
}

int main()
{
    printf("Iniciando Panel de Control del Sistema de Hamburguesas...\n");

    // Conectar con el sistema principal
    conectar_memoria_compartida();

    // Encontrar PID del sistema principal
    encontrar_pid_sistema();

    // Inicializar interfaz
    inicializar_ncurses();

    printf("Panel de control conectado exitosamente\n");
    printf("Cambiando a interfaz gráfica...\n");
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

            mostrar_interfaz_control_avanzada();
            ultimo_refresh = ahora;
        }

        // Procesar comando si hay uno
        if (ch != ERR)
        {
            procesar_comando(ch);

            // Limpiar línea de estado después de 2 segundos
            if (ch != 'h' && ch != 'H')
            {
                sleep(2);
                mvwprintw(win_comandos, 4, 2, "                                        ");
                wrefresh(win_comandos);
            }
        }

        usleep(50000); // 50ms de espera
    }

    // Limpiar recursos
    limpiar_ncurses();

    printf("Panel de control terminado\n");
    return 0;
}