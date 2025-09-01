// burger_types.h - Definiciones de tipos y estructuras
#ifndef BURGER_TYPES_H
#define BURGER_TYPES_H

#include <pthread.h>
#include <time.h>

#define MAX_BANDAS 10
#define MAX_INGREDIENTES 15
#define MAX_ORDENES 100
#define MAX_NOMBRE_INGREDIENTE 30
#define MAX_LOGS_POR_BANDA 10
#define CAPACIDAD_DISPENSADOR 20
#define NUM_TIPOS_HAMBURGUESA 6

// Estructura para ingredientes por banda
typedef struct {
    char nombre[MAX_NOMBRE_INGREDIENTE];
    int cantidad;
    pthread_mutex_t mutex;
} Ingrediente;

// Definición de tipos de hamburguesas
typedef struct {
    char nombre[50];
    char ingredientes[10][MAX_NOMBRE_INGREDIENTE];
    int num_ingredientes;
    float precio;
} TipoHamburguesa;

// Estructura para logs de cada banda
typedef struct {
    char mensaje[100];
    time_t timestamp;
} LogEntry;

// Estructura para una orden específica
typedef struct {
    int id_orden;
    int tipo_hamburguesa;
    char nombre_hamburguesa[50];
    char ingredientes_solicitados[MAX_INGREDIENTES][MAX_NOMBRE_INGREDIENTE];
    int num_ingredientes;
    time_t tiempo_creacion;
    int paso_actual;
    int completada;
} Orden;

// Estructura para una banda de preparación
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
    ColaFIFO cola_espera;
    int num_bandas;
    int sistema_activo;
    int total_ordenes_procesadas;
    pthread_mutex_t mutex_global;
    pthread_cond_t nueva_orden;
} DatosCompartidos;

// Menú de hamburguesas disponibles
extern TipoHamburguesa menu_hamburguesas[NUM_TIPOS_HAMBURGUESA];
extern char ingredientes_base[MAX_INGREDIENTES][MAX_NOMBRE_INGREDIENTE];

#endif // BURGER_TYPES_H