# Sistema de Simulación de Hamburguesas 🍔

[![C](https://img.shields.io/badge/C-99-blue.svg)](https://gcc.gnu.org/)
[![License](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/Platform-POSIX-lightgrey.svg)](https://en.wikipedia.org/wiki/POSIX)

## 📋 Descripción del Proyecto

Este proyecto implementa un **sistema de simulación de restaurante de hamburguesas** completamente automatizado con múltiples bandas de preparación. El sistema demuestra conceptos avanzados de programación concurrente, sincronización de hilos, gestión de memoria compartida y desarrollo de interfaces gráficas en terminal.

### 🎯 Características Principales

- **🔄 Simulación Multi-Hilo**: Múltiples bandas de preparación operando simultáneamente
- **📊 Gestión Inteligente de Inventario**: Sistema de alertas automáticas y reabastecimiento
- **🚦 Cola FIFO Avanzada**: Gestión eficiente de órdenes pendientes sin pérdidas
- **🎮 Panel de Control Interactivo**: Interfaz gráfica en tiempo real con ncurses
- **⚡ Configuración Flexible**: Parámetros ajustables para diferentes escenarios
- **📈 Monitoreo en Tiempo Real**: Estadísticas y logs detallados del sistema

## 🏗️ Arquitectura del Sistema

### Componentes Principales

```
┌─────────────────────────────────────────────────────────────┐
│                    SISTEMA PRINCIPAL                        │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────────────┐ │
│  │ Generador   │  │ Asignador   │  │ Monitor de          │ │
│  │ de Órdenes  │  │ de Órdenes  │  │ Inventario          │ │
│  └─────────────┘  └─────────────┘  └─────────────────────┘ │
│                                                             │
│  ┌─────────────────────────────────────────────────────────┐ │
│  │              BANDAS DE PREPARACIÓN                      │ │
│  │  ┌─────────┐  ┌─────────┐  ┌─────────┐  ┌─────────┐   │ │
│  │  │ Banda 1 │  │ Banda 2 │  │ Banda 3 │  │ Banda N │   │ │
│  │  └─────────┘  └─────────┘  └─────────┘  └─────────┘   │ │
│  └─────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────┘
                              │
                              │ Memoria Compartida
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                  PANEL DE CONTROL                           │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────────────┐ │
│  │ Vista       │  │ Control de  │  │ Gestión de          │ │
│  │ General     │  │ Bandas      │  │ Inventario          │ │
│  └─────────────┘  └─────────────┘  └─────────────────────┘ │
└─────────────────────────────────────────────────────────────┘
```

### Flujo de Datos

1. **Generación**: El sistema crea órdenes automáticamente
2. **Asignación**: Las órdenes se distribuyen a bandas disponibles
3. **Procesamiento**: Cada banda prepara hamburguesas paso a paso
4. **Monitoreo**: El sistema supervisa inventario y genera alertas
5. **Control**: El panel permite intervención manual en tiempo real

## 🚀 Instalación y Configuración

### Requisitos del Sistema

- **Sistema Operativo**: Linux, macOS o cualquier sistema compatible con POSIX
- **Compilador**: GCC 4.8+ o Clang 3.5+
- **Bibliotecas**: pthread, librt, ncurses

### Instalación de Dependencias

#### Ubuntu/Debian

```bash
sudo apt-get update
sudo apt-get install build-essential libncurses5-dev
```

#### CentOS/RHEL/Fedora

```bash
sudo yum groupinstall "Development Tools"
sudo yum install ncurses-devel
# o para Fedora:
sudo dnf groupinstall "Development Tools"
sudo dnf install ncurses-devel
```

#### macOS

```bash
# Instalar Xcode Command Line Tools
xcode-select --install

# Instalar ncurses con Homebrew
brew install ncurses
```

### Compilación del Proyecto

```bash
# Clonar el repositorio
git clone [URL_DEL_REPOSITORIO]
cd ProyectoFinal

# Compilar el sistema completo
make all

# Verificar la compilación
ls -la burger_system control_panel
```

## 🎮 Uso del Sistema

### Ejecución Básica

```bash
# 1. Ejecutar el sistema principal (en segundo plano)
./burger_system -n 4 &

# 2. Ejecutar el panel de control
./control_panel
```

### Configuración Avanzada

```bash
# Sistema con 6 bandas, 3 segundos por ingrediente, 15 segundos entre órdenes
./burger_system -n 6 -t 3 -o 15 &

# Sistema rápido: 2 bandas, 1 segundo por ingrediente, 5 segundos entre órdenes
./burger_system -n 2 -t 1 -o 5 &

# Solo mostrar menú de hamburguesas disponibles
./burger_system -m
```

### Comandos del Makefile

```bash
# Compilar sistema completo
make all

# Ejecutar con configuración por defecto
make run

# Ejecutar con configuración personalizada
make run-custom BANDAS=6 TIEMPO_ING=3 TIEMPO_ORD=10

# Ejecutar solo el panel de control
make panel

# Compilar con información de depuración
make debug

# Compilar con optimizaciones
make release

# Limpiar archivos compilados
make clean

# Mostrar información del proyecto
make info
```

## 🎯 Controles del Panel de Control

### Navegación

- **↑/↓**: Cambiar banda o ingrediente seleccionado
- **TAB**: Cambiar entre diferentes vistas
- **1-9**: Seleccionar banda directamente

### Control de Bandas

- **ESPACIO**: Pausar/Reanudar banda seleccionada
- **R**: Reabastecer banda completamente
- **I**: Ver inventario detallado de la banda

### Gestión de Inventario

- **+/-**: Añadir/quitar unidades del ingrediente seleccionado
- **F**: Llenar completamente el ingrediente seleccionado

### Modo Abastecimiento

- **1**: Reabastecer banda seleccionada
- **2 o A**: Reabastecer TODAS las bandas
- **3 o C**: Reabastecer solo ingredientes críticos
- **4 o E**: Reabastecer solo ingredientes agotados
- **5**: Modo personalizado (ingrediente por ingrediente)

### Sistema

- **H**: Mostrar ayuda detallada
- **Q**: Salir del panel de control

## 📊 Tipos de Hamburguesas Disponibles

| Hamburguesa       | Ingredientes                         | Precio | Tiempo Estimado |
| ----------------- | ------------------------------------ | ------ | --------------- |
| **Clásica**       | Pan, carne, lechuga, tomate          | $8.50  | 10s             |
| **Cheeseburger**  | Clásica + queso                      | $9.25  | 12s             |
| **BBQ Bacon**     | Con bacon, queso, cebolla, salsa BBQ | $11.75 | 14s             |
| **Vegetariana**   | Sin carne, con vegetal y aguacate    | $10.25 | 14s             |
| **Deluxe**        | Premium con todos los ingredientes   | $13.50 | 18s             |
| **Spicy Mexican** | Con jalapeños y salsa picante        | $12.00 | 16s             |

## 🔧 Configuración del Sistema

### Parámetros de Línea de Comandos

| Parámetro                  | Descripción                  | Rango | Valor por Defecto |
| -------------------------- | ---------------------------- | ----- | ----------------- |
| `-n, --bandas`             | Número de bandas             | 1-10  | 3                 |
| `-t, --tiempo-ingrediente` | Segundos por ingrediente     | 1-60  | 2                 |
| `-o, --tiempo-orden`       | Segundos entre órdenes       | 1-300 | 7                 |
| `-m, --menu`               | Mostrar menú de hamburguesas | -     | -                 |
| `-h, --help`               | Mostrar ayuda completa       | -     | -                 |

### Señales del Sistema

- **SIGINT/SIGTERM**: Terminación limpia del sistema
- **SIGUSR1**: Pausar una banda aleatoria
- **SIGUSR2**: Reanudar todas las bandas pausadas
- **SIGCONT**: Reabastecimiento automático de bandas críticas

## 📈 Rendimiento y Escalabilidad

### Estimaciones de Rendimiento

El sistema calcula automáticamente:

- **Tiempo promedio por hamburguesa**: Basado en ingredientes y configuración
- **Órdenes por minuto**: Según el tiempo entre órdenes configurado
- **Capacidad teórica**: Máximo rendimiento del sistema
- **Advertencias de saturación**: Si la cola podría crecer indefinidamente

### Ejemplo de Configuración

```bash
# Configuración de alto rendimiento
./burger_system -n 8 -t 1 -o 3

# Resultado esperado:
# • Tiempo promedio por hamburguesa: 7.5 segundos
# • Órdenes generadas por minuto: 20.0
# • Capacidad teórica del sistema: 64.0 hamburguesas/minuto
# • CONFIGURACIÓN: Sistema balanceado para esta carga
```

## 🐛 Solución de Problemas

### Problemas Comunes

#### Error de Compilación

```bash
# Si ncurses no se encuentra:
sudo apt-get install libncurses5-dev  # Ubuntu/Debian
brew install ncurses                   # macOS
```

#### Error de Conexión del Panel

```bash
# El panel no puede conectar con el sistema principal
# Solución: Asegurarse de que burger_system esté ejecutándose
ps aux | grep burger_system
./burger_system -n 4 &
```

#### Sistema No Responde

```bash
# Verificar si el sistema está activo
ps aux | grep burger_system

# Reiniciar si es necesario
pkill burger_system
./burger_system -n 4 &
```

### Logs y Debugging

```bash
# Compilar con información de depuración
make debug

# Verificar sintaxis sin compilar
make check

# Limpiar completamente (incluye memoria compartida)
make clean-all
```

## 🧪 Casos de Uso y Ejemplos

### Simulación de Restaurante Pequeño

```bash
./burger_system -n 2 -t 3 -o 10
# 2 bandas, preparación moderada, 10s entre órdenes
```

### Simulación de Restaurante de Alto Volumen

```bash
./burger_system -n 6 -t 1 -o 5
# 6 bandas, preparación rápida, 5s entre órdenes
```

### Pruebas de Estrés

```bash
./burger_system -n 10 -t 1 -o 2
# 10 bandas, máxima velocidad, 2s entre órdenes
```

## 📚 Documentación Técnica

### Estructuras de Datos

- **`DatosCompartidos`**: Contiene toda la información del sistema
- **`Banda`**: Representa una banda de preparación con su estado
- **`Orden`**: Define una orden de hamburguesa con metadatos
- **`ColaFIFO`**: Implementa la cola de órdenes pendientes
- **`Ingrediente`**: Gestiona el inventario de cada ingrediente

### Algoritmos Implementados

- **Asignación Inteligente**: Busca la banda más adecuada para cada orden
- **Gestión de Cola FIFO**: Cola circular thread-safe sin pérdidas
- **Sistema de Inventario**: Control de consumo y reabastecimiento
- **Procesamiento Paralelo**: Hilos POSIX para operaciones concurrentes

### Sincronización

- **Mutexes**: Acceso exclusivo a recursos compartidos
- **Variables de Condición**: Sincronización entre hilos
- **Memoria Compartida**: Comunicación entre procesos
- **Señales del Sistema**: Control externo del sistema

## 🤝 Contribución

### Cómo Contribuir

1. **Fork** el repositorio
2. **Crea** una rama para tu feature (`git checkout -b feature/AmazingFeature`)
3. **Commit** tus cambios (`git commit -m 'Add some AmazingFeature'`)
4. **Push** a la rama (`git push origin feature/AmazingFeature`)
5. **Abre** un Pull Request

### Estándares de Código

- **Estilo**: Seguir el estilo de código existente
- **Documentación**: Comentar todas las funciones y estructuras
- **Testing**: Probar cambios antes de enviar
- **Commits**: Mensajes descriptivos y en español

## 📄 Licencia

Este proyecto está bajo la Licencia MIT. Ver el archivo `LICENSE` para más detalles.

## 👨‍💻 Autor

**Tu Nombre** - [tu-email@dominio.com](mailto:tu-email@dominio.com)

## 🙏 Agradecimientos

- **Profesores**: Por la guía y supervisión del proyecto
- **Compañeros**: Por el apoyo y colaboración durante el desarrollo
- **Comunidad Open Source**: Por las bibliotecas y herramientas utilizadas

## 📞 Soporte

Para soporte técnico o consultas:

- **Email**: [tu-email@dominio.com](mailto:tu-email@dominio.com)
- **Issues**: Crear un issue en GitHub para reportar bugs
- **Documentación**: Revisar la documentación inline del código

---

**⭐ Si este proyecto te ha sido útil, considera darle una estrella en GitHub!**
