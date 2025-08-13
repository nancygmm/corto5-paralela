#include <stdio.h>
#include <stdlib.h>
#include <omp.h>
#include <unistd.h>
#include <time.h>

// Constantes de la simulación
#define NUM_CLIENTES 50
#define MAX_INGREDIENTES 5
#define MAX_BEBIDAS 4
#define INVENTARIO_INICIAL 100

// Tipos de bebidas
typedef enum {
    CAFE = 0,
    TE = 1,
    SMOOTHIE = 2,
    FRAPPE = 3
} TipoBebida;

// Tipos de ingredientes
typedef enum {
    GRANOS_CAFE = 0,
    LECHE = 1,
    AZUCAR = 2,
    HIELO = 3,
    FRUTA = 4
} TipoIngrediente;

// Variables globales compartidas
int inventario[MAX_INGREDIENTES];           
float caja_registradora = 0.0;             
int estadisticas_ventas[MAX_BEBIDAS];       
int clientes_procesados = 0;                

// Nombres para imprimir
const char* nombres_bebidas[] = {"Café", "Té", "Smoothie", "Frappé"};
const char* nombres_ingredientes[] = {"Granos de Café", "Leche", "Azúcar", "Hielo", "Fruta"};

// Precios de las bebidas
float precios[] = {25.0, 20.0, 35.0, 40.0};

// Función para inicializar la cafetería
void inicializar_cafeteria() {
    printf("INICIANDO SIMULACIÓN DE CAFETERÍA\n");
    
    // Inicializar inventario 
    for (int i = 0; i < MAX_INGREDIENTES; i++) {
        inventario[i] = INVENTARIO_INICIAL;
    }
    
    // Inicializar estadísticas 
    for (int i = 0; i < MAX_BEBIDAS; i++) {
        estadisticas_ventas[i] = 0;
    }
    
    printf("Inventario inicial: %d unidades de cada ingrediente\n", INVENTARIO_INICIAL);
    printf("Número de clientes a atender: %d\n", NUM_CLIENTES);
    printf("Número de threads disponibles: %d\n", omp_get_max_threads());
    printf("\n");
}

// Función para verificar disponibilidad de ingredientes
int verificar_ingredientes(TipoBebida tipo) {
    switch (tipo) {
        case CAFE:
            return (inventario[GRANOS_CAFE] >= 1 && inventario[LECHE] >= 1);
        case TE:
            return (inventario[AZUCAR] >= 1);
        case SMOOTHIE:
            return (inventario[FRUTA] >= 2 && inventario[HIELO] >= 1);
        case FRAPPE:
            return (inventario[GRANOS_CAFE] >= 1 && inventario[HIELO] >= 2 && inventario[LECHE] >= 1);
        default:
            return 0;
    }
}

// Función para usar ingredientes (critical section necesaria)
void usar_ingredientes(TipoBebida tipo) {
    switch (tipo) {
        case CAFE:
            inventario[GRANOS_CAFE]--;
            inventario[LECHE]--;
            break;
        case TE:
            inventario[AZUCAR]--;
            break;
        case SMOOTHIE:
            inventario[FRUTA] -= 2;
            inventario[HIELO]--;
            break;
        case FRAPPE:
            inventario[GRANOS_CAFE]--;
            inventario[HIELO] -= 2;
            inventario[LECHE]--;
            break;
    }
}

// Función para obtener tiempo de preparación en microsegundos
int obtener_tiempo_preparacion(TipoBebida tipo) {
    switch (tipo) {
        case CAFE: return 100000;    
        case TE: return 80000;       
        case SMOOTHIE: return 200000; 
        case FRAPPE: return 150000;   
        default: return 100000;
    }
}

// Función para procesar un cliente individual
int procesar_cliente(int id_cliente, TipoBebida tipo_bebida, float dinero_cliente) {
    // Variables privadas para este cliente específico
    float precio = precios[tipo_bebida];
    int tiempo_preparacion = obtener_tiempo_preparacion(tipo_bebida);
    
    // Verificar si el cliente tiene suficiente dinero
    if (dinero_cliente < precio) {
        printf("Cliente %d: No tiene suficiente dinero para %s (tiene %.2f, necesita %.2f)\n", 
               id_cliente, nombres_bebidas[tipo_bebida], dinero_cliente, precio);
        return 0;
    }
    
    // Critical section para verificar y usar ingredientes
    int bebida_preparada = 0;
    #pragma omp critical(inventario)
    {
        if (verificar_ingredientes(tipo_bebida)) {
            usar_ingredientes(tipo_bebida);
            bebida_preparada = 1;
            printf("Cliente %d: Preparando %s (Precio: %.2f)\n", 
                   id_cliente, nombres_bebidas[tipo_bebida], precio);
        } else {
            printf("Cliente %d: No hay ingredientes suficientes para %s\n", 
                   id_cliente, nombres_bebidas[tipo_bebida]);
        }
    }
    
    if (bebida_preparada) {
        // Simular tiempo de preparación
        usleep(tiempo_preparacion);
        
        // Critical section para actualizar caja y estadísticas
        #pragma omp critical(ventas)
        {
            caja_registradora += precio;
            estadisticas_ventas[tipo_bebida]++;
            clientes_procesados++;
        }
        
        printf("Cliente %d: %s entregado exitosamente\n", 
               id_cliente, nombres_bebidas[tipo_bebida]);
        return 1;
    }
    
    return 0;
}

// Función de trabajo para estación de limpieza (sections)
void estacion_limpieza() {
    printf("Estación de limpieza: Iniciando limpieza de equipos\n");
    
    // Simular tiempo de limpieza
    for (int i = 0; i < 5; i++) {
        usleep(50000); 
        printf("Estación de limpieza: Limpiando equipo %d/5\n", i + 1);
    }
    
    printf("Estación de limpieza: Limpieza completada\n");
}

// Función de trabajo para estación de reabastecimiento (sections)
void estacion_reabastecimiento() {
    printf("Estación de reabastecimiento: Verificando inventario\n");
    
    #pragma omp critical(inventario)
    {
        for (int i = 0; i < MAX_INGREDIENTES; i++) {
            if (inventario[i] < 10) {
                printf("Estación de reabastecimiento: Reabasteciendo %s (cantidad baja: %d)\n", 
                       nombres_ingredientes[i], inventario[i]);
                inventario[i] += 20;
                printf("Estación de reabastecimiento: %s reabastecido a %d unidades\n", 
                       nombres_ingredientes[i], inventario[i]);
            }
        }
    }
    
    usleep(100000); // 0.1 segundos
    printf("Estación de reabastecimiento: Verificación completada\n");
}

// Función principal de la simulación
void simular_cafeteria() {
    // Variables para reduction
    float ventas_totales = 0.0;
    int total_clientes_atendidos = 0;
    int max_tiempo_servicio = 0;
    int min_tiempo_servicio = 1000000;
    
    double tiempo_inicio_simulacion = omp_get_wtime();
    
    printf("INICIANDO ATENCIÓN DE CLIENTES\n\n");
    
    // Parallel for con múltiples cláusulas OpenMP
    #pragma omp parallel for \
        firstprivate(tiempo_inicio_simulacion) \
        shared(inventario, caja_registradora, estadisticas_ventas) \
        reduction(+:ventas_totales, total_clientes_atendidos) \
        reduction(max:max_tiempo_servicio) \
        reduction(min:min_tiempo_servicio) \
        schedule(dynamic, 2)
    for (int i = 0; i < NUM_CLIENTES; i++) {
        // Variables firstprivate para cada cliente
        int id_cliente = i + 1;
        float dinero_cliente = 30.0 + (rand() % 50); // Entre 30 y 80
        TipoBebida tipo_bebida = rand() % MAX_BEBIDAS;
        
        double tiempo_inicio_cliente = omp_get_wtime();
        
        // Procesar cliente
        int cliente_atendido = procesar_cliente(id_cliente, tipo_bebida, dinero_cliente);
        
        if (cliente_atendido) {
            double tiempo_servicio = (omp_get_wtime() - tiempo_inicio_cliente) * 1000000; 
            
            // Actualizar variables de reduction
            ventas_totales += precios[tipo_bebida];
            total_clientes_atendidos++;
            
            int tiempo_int = (int)tiempo_servicio;
            if (tiempo_int > max_tiempo_servicio) max_tiempo_servicio = tiempo_int;
            if (tiempo_int < min_tiempo_servicio) min_tiempo_servicio = tiempo_int;
        }
    }
    
    printf("\nOPERACIONES PARALELAS DE ESTACIONES\n");
    
    // Parallel sections para diferentes estaciones
    #pragma omp parallel sections shared(inventario)
    {
        #pragma omp section
        {
            printf("Thread %d ejecutando estación de limpieza\n", omp_get_thread_num());
            estacion_limpieza();
        }
        
        #pragma omp section
        {
            printf("Thread %d ejecutando estación de reabastecimiento\n", omp_get_thread_num());
            estacion_reabastecimiento();
        }
        
        #pragma omp section
        {
            printf("Thread %d ejecutando control de calidad\n", omp_get_thread_num());
            usleep(200000); 
            printf("Control de calidad: Verificación de equipos completada\n");
        }
    }
    
    double tiempo_total_simulacion = omp_get_wtime() - tiempo_inicio_simulacion;
    
    // Mostrar estadísticas finales
    printf("\nESTADÍSTICAS FINALES\n");
    printf("Tiempo total de simulación: %.3f segundos\n", tiempo_total_simulacion);
    printf("Total en caja registradora: $%.2f\n", caja_registradora);
    printf("Ventas calculadas por reduction: $%.2f\n", ventas_totales);
    printf("Clientes procesados: %d/%d\n", clientes_procesados, NUM_CLIENTES);
    printf("Clientes atendidos (reduction): %d\n", total_clientes_atendidos);
    printf("Tiempo máximo de servicio: %d microsegundos\n", max_tiempo_servicio);
    printf("Tiempo mínimo de servicio: %d microsegundos\n", min_tiempo_servicio);
    
    printf("\nVENTAS POR TIPO DE BEBIDA\n");
    for (int i = 0; i < MAX_BEBIDAS; i++) {
        printf("%s: %d vendidos (%.2f%% del total)\n", 
               nombres_bebidas[i], 
               estadisticas_ventas[i],
               (float)estadisticas_ventas[i] / clientes_procesados * 100);
    }
    
    printf("\nINVENTARIO FINAL\n");
    for (int i = 0; i < MAX_INGREDIENTES; i++) {
        printf("%s: %d unidades restantes\n", nombres_ingredientes[i], inventario[i]);
    }
}

int main() {
    // Inicializar generador de números aleatorios
    srand(time(NULL));
    
    // Configurar número de threads
    omp_set_num_threads(4);
    
    printf("Threads configurados: %d\n", omp_get_max_threads());
    
    // Inicializar y ejecutar simulación
    inicializar_cafeteria();
    simular_cafeteria();
        
    return 0;
}