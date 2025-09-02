# =============================================================================
# Makefile para el Sistema de Simulación de Hamburguesas
# =============================================================================
# 
# Este Makefile proporciona comandos para compilar y ejecutar el sistema
# completo de simulación de hamburguesas, incluyendo:
# 
# - burger_system: Sistema principal de simulación
# - control_panel: Panel de control interactivo
# 
# =============================================================================
# DEPENDENCIAS REQUERIDAS
# =============================================================================
# 
# El sistema requiere las siguientes bibliotecas del sistema:
# - pthread: Para programación multi-hilo
# - librt: Para memoria compartida POSIX
# - ncurses: Para interfaz gráfica del panel de control
# 
# En sistemas Ubuntu/Debian, instalar con:
#   sudo apt-get install libncurses5-dev
# 
# En sistemas macOS, instalar con:
#   brew install ncurses
# 
# =============================================================================
# VARIABLES DE COMPILACIÓN
# =============================================================================

# Compilador C estándar
CC = gcc

# Flags de compilación optimizados para desarrollo y producción
CFLAGS = -Wall -std=c99 -D_GNU_SOURCE -c

# Bibliotecas del sistema requeridas
LIBS = -lpthread -lrt

# =============================================================================
# REGLAS PRINCIPALES
# =============================================================================

# Meta principal: compilar sistema completo y panel de control
all: burger_system control_panel
	@echo "================================================"
	@echo "SISTEMA COMPILADO EXITOSAMENTE"
	@echo "================================================"
	@echo "Ejecutables creados:"
	@echo "  • burger_system    - Sistema principal de simulación"
	@echo "  • control_panel    - Panel de control interactivo"
	@echo ""
	@echo "Para ejecutar el sistema:"
	@echo "  1. ./burger_system -n 4 &"
	@echo "  2. ./control_panel"
	@echo "================================================"

# =============================================================================
# COMPILACIÓN DEL SISTEMA PRINCIPAL
# =============================================================================

# Sistema principal de simulación de hamburguesas
burger_system: burger_system.o
	@echo "Enlazando burger_system..."
	$(CC) -o burger_system burger_system.o $(LIBS)
	@echo "✓ burger_system compilado exitosamente"

# Objeto del sistema principal
burger_system.o: burger_system.c
	@echo "Compilando burger_system.c..."
	$(CC) $(CFLAGS) burger_system.c

# =============================================================================
# COMPILACIÓN DEL PANEL DE CONTROL
# =============================================================================

# Panel de control interactivo
control_panel: control_panel.o
	@echo "Enlazando control_panel..."
	$(CC) -o control_panel control_panel.o $(LIBS) -lncurses
	@echo "✓ control_panel compilado exitosamente"

# Objeto del panel de control
control_panel.o: control_panel.c
	@echo "Compilando control_panel.c..."
	$(CC) $(CFLAGS) control_panel.c

# =============================================================================
# REGLAS DE UTILIDAD
# =============================================================================

# Limpiar archivos compilados y objetos
clean:
	@echo "Limpiando archivos compilados..."
	rm -f burger_system control_panel *.o
	@echo "✓ Limpieza completada"

# Ejecutar el sistema principal con configuración por defecto
run: burger_system
	@echo "================================================"
	@echo "EJECUTANDO SISTEMA DE HAMBURGUESAS"
	@echo "================================================"
	@echo "Configuración: 4 bandas, tiempos por defecto"
	@echo "Para detener: Ctrl+C"
	@echo "================================================"
	./burger_system -n 4

# Ejecutar con configuración personalizada
run-custom: burger_system
	@echo "================================================"
	@echo "EJECUTANDO CON CONFIGURACIÓN PERSONALIZADA"
	@echo "================================================"
	@echo "Uso: make run-custom BANDAS=N TIEMPO_ING=N TIEMPO_ORD=N"
	@echo "Ejemplo: make run-custom BANDAS=6 TIEMPO_ING=3 TIEMPO_ORD=10"
	@echo "================================================"
	./burger_system -n $(BANDAS) -t $(TIEMPO_ING) -o $(TIEMPO_ORD)

# Ejecutar solo el panel de control (requiere sistema activo)
panel: control_panel
	@echo "================================================"
	@echo "EJECUTANDO PANEL DE CONTROL"
	@echo "================================================"
	@echo "IMPORTANTE: El sistema principal debe estar ejecutándose"
	@echo "Para iniciar el sistema: ./burger_system -n 4 &"
	@echo "================================================"
	./control_panel

# =============================================================================
# REGLAS DE DESARROLLO
# =============================================================================

# Compilar con información de depuración
debug: CFLAGS += -g -DDEBUG
debug: clean all

# Compilar con optimizaciones
release: CFLAGS += -O2 -DNDEBUG
release: clean all

# Verificar sintaxis sin compilar
check:
	@echo "Verificando sintaxis de los archivos fuente..."
	$(CC) $(CFLAGS) -fsyntax-only burger_system.c
	$(CC) $(CFLAGS) -fsyntax-only control_panel.c
	@echo "✓ Verificación de sintaxis completada"

# =============================================================================
# REGLAS DE INSTALACIÓN
# =============================================================================

# Instalar en el sistema (requiere permisos de administrador)
install: all
	@echo "Instalando sistema de hamburguesas..."
	sudo cp burger_system /usr/local/bin/
	sudo cp control_panel /usr/local/bin/
	@echo "✓ Sistema instalado en /usr/local/bin/"

# Desinstalar del sistema
uninstall:
	@echo "Desinstalando sistema de hamburguesas..."
	sudo rm -f /usr/local/bin/burger_system
	sudo rm -f /usr/local/bin/control_panel
	@echo "✓ Sistema desinstalado"

# =============================================================================
# REGLAS DE DOCUMENTACIÓN
# =============================================================================

# Generar documentación con Doxygen (si está disponible)
docs:
	@if command -v doxygen >/dev/null 2>&1; then \
		echo "Generando documentación con Doxygen..."; \
		doxygen Doxyfile 2>/dev/null || echo "Doxyfile no encontrado"; \
	else \
		echo "Doxygen no está instalado. Instalar con:"; \
		echo "  Ubuntu/Debian: sudo apt-get install doxygen"; \
		echo "  macOS: brew install doxygen"; \
	fi

# =============================================================================
# REGLAS DE LIMPIEZA AVANZADA
# =============================================================================

# Limpieza completa (incluye archivos temporales del sistema)
clean-all: clean
	@echo "Limpiando archivos temporales del sistema..."
	sudo shm_unlink /burger_system 2>/dev/null || true
	@echo "✓ Limpieza completa completada"

# =============================================================================
# INFORMACIÓN DEL PROYECTO
# =============================================================================

# Mostrar información del proyecto
info:
	@echo "================================================"
	@echo "SISTEMA DE SIMULACIÓN DE HAMBURGUESAS"
	@echo "================================================"
	@echo "Versión: 1.0"
	@echo "Autor: [Tu Nombre]"
	@echo "Fecha: [Fecha de Creación]"
	@echo ""
	@echo "DESCRIPCIÓN:"
	@echo "Sistema de simulación de restaurante de hamburguesas"
	@echo "con múltiples bandas de preparación y panel de control"
	@echo "interactivo en tiempo real."
	@echo ""
	@echo "CARACTERÍSTICAS:"
	@echo "• Simulación multi-hilo con bandas de preparación"
	@echo "• Sistema de inventario con alertas automáticas"
	@echo "• Cola FIFO para gestión de órdenes"
	@echo "• Panel de control gráfico con ncurses"
	@echo "• Memoria compartida para comunicación entre procesos"
	@echo ""
	@echo "COMANDOS DISPONIBLES:"
	@echo "  make all          - Compilar sistema completo"
	@echo "  make run          - Ejecutar sistema (4 bandas)"
	@echo "  make panel        - Ejecutar solo panel de control"
	@echo "  make clean        - Limpiar archivos compilados"
	@echo "  make info         - Mostrar esta información"
	@echo "================================================"

# =============================================================================
# REGLAS ESPECIALES
# =============================================================================

# Meta para evitar conflictos con archivos del mismo nombre
.PHONY: all clean run run-custom panel debug release check install uninstall docs clean-all info

# =============================================================================
# CONFIGURACIÓN POR DEFECTO
# =============================================================================

# Valores por defecto para configuración personalizada
BANDAS ?= 4
TIEMPO_ING ?= 2
TIEMPO_ORD ?= 7