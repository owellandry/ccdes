#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// Detectar si el código está minificado
int is_minified(const char *code, size_t len) {
    int newlines = 0;
    for (size_t i = 0; i < len && i < 1000; i++) {
        if (code[i] == '\n') newlines++;
    }
    return (newlines < 5);
}

// Beautify básico de JavaScript
char* beautify_js(const char *code, size_t len) {
    size_t output_size = len * 2;
    char *output = malloc(output_size);
    if (!output) return NULL;
    
    size_t out_pos = 0;
    int indent_level = 0;
    int in_string = 0;
    char string_char = 0;
    
    for (size_t i = 0; i < len; i++) {
        char c = code[i];
        
        // Manejo simple de strings
        if ((c == '"' || c == '\'') && (i == 0 || code[i-1] != '\\')) {
            if (!in_string) {
                in_string = 1;
                string_char = c;
            } else if (c == string_char) {
                in_string = 0;
            }
        }
        
        if (!in_string) {
            if (c == '{') {
                output[out_pos++] = c;
                output[out_pos++] = '\n';
                indent_level++;
                for (int j = 0; j < indent_level * 2; j++)
                    output[out_pos++] = ' ';
                continue;
            }
            else if (c == '}') {
                indent_level--;
                output[out_pos++] = '\n';
                for (int j = 0; j < indent_level * 2; j++)
                    output[out_pos++] = ' ';
                output[out_pos++] = c;
                output[out_pos++] = '\n';
                for (int j = 0; j < indent_level * 2; j++)
                    output[out_pos++] = ' ';
                continue;
            }
            else if (c == ';') {
                output[out_pos++] = c;
                output[out_pos++] = '\n';
                for (int j = 0; j < indent_level * 2; j++)
                    output[out_pos++] = ' ';
                continue;
            }
        }
        
        output[out_pos++] = c;
        
        // Redimensionar buffer si es necesario
        if (out_pos >= output_size - 100) {
            output_size *= 2;
            char *new_output = realloc(output, output_size);
            if (!new_output) {
                free(output);
                return NULL;
            }
            output = new_output;
        }
    }
    
    output[out_pos] = '\0';
    return output;
}

// Extraer rutas de Next.js
void extract_nextjs_routes(const char *code, size_t len) {
    (void)len;  // eliminar warning
    
    printf("\n[*] Extrayendo rutas de Next.js...\n");
    
    const char *patterns[] = {
        "/_next/static/", "/_next/data/", "__NEXT_DATA__",
        "pageProps", "getStaticProps", "getServerSideProps", NULL
    };
    
    for (int i = 0; patterns[i] != NULL; i++) {
        if (strstr(code, patterns[i])) {
            printf("  [+] Encontrado: %s\n", patterns[i]);
        }
    }
}

// Identificar componentes React
void identify_react_components(const char *code, size_t len) {
    (void)len;  // eliminar warning
    
    printf("\n[*] Identificando componentes React...\n");
    
    const char *patterns[] = {
        "React.createElement", "useState", "useEffect",
        "Component", "jsx", "forwardRef", NULL
    };
    
    for (int i = 0; patterns[i] != NULL; i++) {
        if (strstr(code, patterns[i])) {
            printf("  [+] Patrón React encontrado: %s\n", patterns[i]);
        }
    }
}

// Analizar estructura de Next.js
void analyze_nextjs_structure(const char *code, size_t len) {
    printf("\n=== Análisis de Estructura Next.js ===\n");
    
    if (is_minified(code, len)) {
        printf("[!] Código minificado detectado\n");
        printf("[*] Aplicando beautifier...\n");
        
        char *beautified = beautify_js(code, len);
        if (beautified) {
            FILE *f = fopen("beautified_output.js", "wb");
            if (f) {
                fwrite(beautified, 1, strlen(beautified), f);
                fclose(f);
                printf("[+] Código beautificado guardado en: beautified_output.js\n");
            }
            free(beautified);
        }
    }
    
    extract_nextjs_routes(code, len);
    identify_react_components(code, len);
}