
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

  

Ejecuta un comando y muestra las mediciones directamente en la terminal. El subcomando ejec es opcional, ya que es la acción por defecto.


```bash
miprof [ejec] <comando> [argumentos...]
```

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
#### b) Medición y Guardado en Archivo (ejecsave)

Ejecuta un comando, muestra las mediciones y las agrega al final de un archivo de texto. Si el archivo no existe, lo crea.
```bash
miprof ejecsave <archivo> <comando> [argumentos...]
```

**Input:**
```bash
mishell> miprof ejecsave resultados.txt ls -l
```

**Output:**
```bash
--- Mediciones de miprof ---
Comando: ls -l
Tiempo real: 0.009871 s
Tiempo de usuario: 0.001245 s
Tiempo de sistema: 0.003451 s
Pico de memoria residente: 3150 KB
---------------------------
```
#### c) Medición con Límite de Tiempo (ejecutar)

Ejecuta un comando con un tiempo máximo de ejecución en segundos. Si el comando excede este límite, la shell lo terminará automáticamente.

```bash
miprof ejecutar <segundos> <comando> [argumentos...]
```

**Input:**
```bash
mishell> miprof ejecutar 3 sleep 5
```

**Output:**
```bash
[miprof] El proceso excedió el tiempo máximo (3 s) y fue terminado.

--- Mediciones de miprof ---
Comando: sleep 5
Tiempo real: 3.001845 s
Tiempo de usuario: 0.000000 s
Tiempo de sistema: 0.000987 s
Pico de memoria residente: 2028 KB
---------------------------
```

### 4. Salir de la Shell

Para finalizar la sesión de `mishell`, usa el comando interno `exit`.

  

```bash

mishell> exit

```
