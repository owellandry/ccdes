#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>

#ifdef _WIN32
#include <windows.h>
#endif
#include <locale.h>

// Estructura para almacenar datos descargados
struct MemoryStruct {
    char *memory;
    size_t size;
};

// Callback para escribir datos descargados
static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    struct MemoryStruct *mem = (struct MemoryStruct *)userp;
    
    char *ptr = realloc(mem->memory, mem->size + realsize + 1);
    if (!ptr) {
        printf("Error: no hay suficiente memoria\n");
        return 0;
    }
    
    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;
    
    return realsize;
}

// Descargar contenido desde URL (versión mejorada)
int download_url(const char *url, struct MemoryStruct *chunk) {
    CURL *curl;
    CURLcode res;
    char errbuf[CURL_ERROR_SIZE] = {0};

    curl = curl_easy_init();
    if (!curl) {
        printf("[!] No se pudo inicializar libcurl\n");
        return -1;
    }

    // === CONFIGURACIÓN ROBUSTA ===
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)chunk);
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);      // seguir redirecciones
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);            // 30 segundos máximo
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);

    // User-Agent real (evita bloqueos)
    curl_easy_setopt(curl, CURLOPT_USERAGENT,
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
        "(KHTML, like Gecko) Chrome/134.0.0.0 Safari/537.36");

    // Cabeceras comunes
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8");
    headers = curl_slist_append(headers, "Accept-Language: es-ES,es;q=0.9,en;q=0.8");
    headers = curl_slist_append(headers, "Accept-Encoding: gzip, deflate");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    // SSL (importante en Windows)
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);

    printf("[*] Conectando a %s ...\n", url);
    res = curl_easy_perform(curl);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        printf("[!] Error al descargar la URL\n");
        printf("[!] Código de error: %s\n", curl_easy_strerror(res));
        if (errbuf[0] != '\0')
            printf("[!] Detalle: %s\n", errbuf);
        return -1;
    }

    return 0;
}

// Función principal de descompilación
void decompile_nextjs(const char *url) {
    printf("\n=== NextJS Decompiler - Herramienta de Seguridad ===\n");
    printf("Analizando: %s\n\n", url);
    
    struct MemoryStruct chunk;
    chunk.memory = malloc(1);
    chunk.size = 0;
    
    if (download_url(url, &chunk) != 0) {
        free(chunk.memory);
        return;
    }
    
    printf("[+] Descargado correctamente: %zu bytes\n", chunk.size);
    printf("[*] Analizando estructura de Next.js...\n");
    
    // Guardar resultado
    FILE *output = fopen("output_decompiled.js", "wb");
    if (output) {
        fwrite(chunk.memory, 1, chunk.size, output);
        fclose(output);
        printf("[+] Contenido guardado en: output_decompiled.js\n");
    } else {
        printf("[!] Error al guardar el archivo\n");
    }
    
    free(chunk.memory);
}

// Modo de prueba (sin cambios)
void test_mode() {
    printf("\n=== Modo de Prueba ===\n");
    
    const char *sample_code = 
        "!function(){var e={};function t(n){if(e[n])return e[n].exports;"
        "var r=e[n]={i:n,l:!1,exports:{}};return o[n].call(r.exports,r,r.exports,t),"
        "r.l=!0,r.exports}t.m=o,t.c=e,t.d=function(e,n,r){"
        "t.o(e,n)||Object.defineProperty(e,n,{enumerable:!0,get:r})},"
        "t.r=function(e){\"undefined\"!=typeof Symbol&&Symbol.toStringTag&&"
        "Object.defineProperty(e,Symbol.toStringTag,{value:\"Module\"}),"
        "Object.defineProperty(e,\"__esModule\",{value:!0})}}();";
    
    printf("[*] Analizando código de ejemplo minificado...\n");
    printf("[*] Tamaño: %zu bytes\n\n", strlen(sample_code));
    
    FILE *f = fopen("sample_minified.js", "wb");
    if (f) {
        fwrite(sample_code, 1, strlen(sample_code), f);
        fclose(f);
        printf("[+] Código de ejemplo guardado en: sample_minified.js\n");
    }
    
    extern void analyze_nextjs_structure(const char *, size_t);
    analyze_nextjs_structure(sample_code, strlen(sample_code));
}

int main() {
    // ==================== SOPORTE UTF-8 ====================
    setlocale(LC_ALL, "");
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
    // ======================================================

    char url[512];
    int opcion;

    printf("\n");
    printf("==================================================\n");
    printf("   NextJS Decompiler - Security Testing Tool     \n");
    printf("   Uso exclusivo para pruebas de seguridad       \n");
    printf("==================================================\n");
    
    while (1) {
        printf("\n[1] Descompilar build de Next.js desde URL\n");
        printf("[2] Modo de prueba (sin conexion)\n");
        printf("[3] Analizar archivo local\n");
        printf("[4] Salir\n");
        printf("\nSelecciona una opcion: ");
        
        if (scanf("%d", &opcion) != 1) {
            while (getchar() != '\n');
            continue;
        }
        while (getchar() != '\n');
        
        switch (opcion) {
            case 1:
                printf("\nIngresa la URL del build de Next.js: ");
                if (fgets(url, sizeof(url), stdin)) {
                    url[strcspn(url, "\n")] = 0;
                    if (strlen(url) > 0)
                        decompile_nextjs(url);
                }
                break;
            case 2:
                test_mode();
                break;
            case 3:
                printf("\nIngresa la ruta del archivo: ");
                if (fgets(url, sizeof(url), stdin)) {
                    url[strcspn(url, "\n")] = 0;
                    FILE *f = fopen(url, "rb");
                    if (f) {
                        fseek(f, 0, SEEK_END);
                        long size = ftell(f);
                        fseek(f, 0, SEEK_SET);
                        char *content = malloc(size + 1);
                        if (content) {
                            fread(content, 1, size, f);
                            content[size] = 0;
                            extern void analyze_nextjs_structure(const char *, size_t);
                            analyze_nextjs_structure(content, size);
                            free(content);
                        }
                        fclose(f);
                    } else {
                        printf("[!] Error al abrir el archivo\n");
                    }
                }
                break;
            case 4:
                printf("\nSaliendo...\n");
                return 0;
            default:
                printf("Opción inválida\n");
        }
    }
    return 0;
}