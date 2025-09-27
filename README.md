# Tarea1-SO

## Compilación

  

Para compilar el proyecto ejecuta el siguiente comando en la raíz del proyecto:

  

```bash

gcc -o run shell.c -Wall

```

  

Este comando creará un archivo ejecutable llamado `run`. La bandera `-Wall` se utiliza para mostrar todas las advertencias del compilador.

  

---

  

## Ejecución y Uso

  

Para iniciar la shell, ejecuta el archivo compilado desde tu terminal:

  

```bash

./run

```

  

Verás el prompt `mishell> `, indicando que está lista para recibir comandos.

  

### 1. Comandos Simples

  

Ejecuta cualquier comando disponible.

  

```bash

mishell> ls -la

mishell> pwd

mishell> echo "Sistemas Operativos"

```

  

### 2. Pipes

  

Encadena dos o más comandos. El siguiente ejemplo lista los procesos, los ordena por uso de CPU y muestra los 5 primeros.

  

```bash

mishell> ps -aux | sort -nr -k 3 | head -n 5

```

  

### 3. Comando Personalizado `miprof`

  

`miprof` es una herramienta para medir el rendimiento de un comando. Captura el tiempo de ejecución (real, usuario y sistema) y el pico de memoria máxima.

  

**Sintaxis:**

```bash

miprof [opciones] <comando> [argumentos...]

```

  

#### a) Medición y Salida por Pantalla

  

Ejecuta un comando y muestra las mediciones directamente en la terminal.

  

**Input:**

```bash

mishell> miprof sleep 2

```

  

**Output:**

```

--- Mediciones de miprof ---

Comando: sleep

Tiempo real: 2.014077 s

Tiempo de usuario: 0.000000 s

Tiempo de sistema: 0.001264 s

Máximo de memoria: 2028 KB

---------------------------

```

### 4. Salir de la Shell

  

Para finalizar la sesión de `mishell`, usa el comando interno `exit`.

  

```bash

mishell> exit

```
