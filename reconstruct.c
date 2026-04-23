/*
 * reconstruct.c - Download assets, beautify JS, reconstruct project structure
 */
#include "ccdes.h"

/* ── Recursive mkdir ──────────────────────────────────────── */

int mkdirs(const char *path) {
    char tmp[MAX_PATH_LEN];
    snprintf(tmp, sizeof(tmp), "%s", path);

    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
#ifdef _WIN32
            _mkdir(tmp);
#else
            mkdir(tmp, 0755);
#endif
            *p = '/';
        }
    }
#ifdef _WIN32
    return _mkdir(tmp);
#else
    return mkdir(tmp, 0755);
#endif
}

/* ── Ensure parent directory of a file path exists ────────── */

static void ensure_parent_dir(const char *filepath) {
    char dir[MAX_PATH_LEN];
    snprintf(dir, sizeof(dir), "%s", filepath);
    char *last_slash = strrchr(dir, '/');
    if (last_slash) {
        *last_slash = '\0';
        mkdirs(dir);
    }
}

/* ── Download all discovered assets ───────────────────────── */

int download_all_assets(Site *site) {
    printf("\n[*] Downloading %d assets...\n", site->asset_count);

    int ok = 0, fail = 0;

    for (int i = 0; i < site->asset_count; i++) {
        Asset *a = &site->assets[i];
        if (a->downloaded) continue;

        /* Build local file path */
        char filepath[MAX_PATH_LEN];
        snprintf(filepath, sizeof(filepath), "%s/raw/%s",
                 site->output_dir, a->local_path);

        ensure_parent_dir(filepath);

        printf("\r  [%d/%d] Downloading...  ", i + 1, site->asset_count);
        fflush(stdout);

        if (download_to_file(a->url, filepath) == 0) {
            a->downloaded = 1;
            ok++;
        } else {
            fail++;
        }
    }

    printf("\r  [+] Downloaded: %d OK, %d failed (of %d total)       \n",
           ok, fail, site->asset_count);
    return ok;
}

/* ── Try to download source maps for JS assets ────────────── */

int try_download_source_maps(Site *site) {
    printf("\n[*] Checking for source maps...\n");

    int found = 0;

    for (int i = 0; i < site->asset_count; i++) {
        Asset *a = &site->assets[i];
        if (a->type != ASSET_JS || !a->downloaded) continue;

        /* Try URL + ".map" */
        char map_url[MAX_URL];
        snprintf(map_url, sizeof(map_url), "%s.map", a->url);

        char map_path[MAX_PATH_LEN];
        snprintf(map_path, sizeof(map_path), "%s/sourcemaps/%s.map",
                 site->output_dir, a->local_path);

        ensure_parent_dir(map_path);

        Buffer buf;
        buffer_init(&buf);
        int rc = download_to_buffer(map_url, &buf);
        if (rc == 0 && buf.size > 10 && buf.data[0] == '{') {
            /* Looks like a valid source map JSON */
            FILE *f = fopen(map_path, "wb");
            if (f) {
                fwrite(buf.data, 1, buf.size, f);
                fclose(f);
                found++;

                /* Also try to extract original sources from the sourcemap */
                /* Look for "sourcesContent":[ in the JSON */
                if (strstr(buf.data, "\"sourcesContent\"")) {
                    printf("  [+] Source map WITH original code: %s\n",
                           a->local_path);
                }
            }
        }
        buffer_free(&buf);

        /* Only print progress every 10 files */
        if ((i + 1) % 10 == 0) {
            printf("\r  [%d/%d] Checking source maps...  ", i + 1,
                   site->asset_count);
            fflush(stdout);
        }
    }

    printf("\r  [+] Source maps found: %d                              \n",
           found);
    return found;
}

/* ── Try to download _buildManifest.js ────────────────────── */

int try_download_build_manifest(Site *site) {
    if (!site->build_id[0]) return -1;

    printf("[*] Looking for _buildManifest.js...\n");

    /* Try several URL patterns */
    const char *patterns[] = {
        "%s%s/_next/static/%s/_buildManifest.js",
        "%s/_next/static/%s/_buildManifest.js",
        "%s%s/_next/static/%s/_ssgManifest.js",
        NULL
    };

    for (int i = 0; patterns[i]; i++) {
        char url[MAX_URL];
        if (i == 1) {
            snprintf(url, sizeof(url), patterns[i],
                     site->base_url, site->build_id);
        } else {
            snprintf(url, sizeof(url), patterns[i],
                     site->base_url, site->asset_prefix, site->build_id);
        }

        char filepath[MAX_PATH_LEN];
        const char *fname = (i < 2) ? "_buildManifest.js" : "_ssgManifest.js";
        snprintf(filepath, sizeof(filepath), "%s/raw/_next/static/%s/%s",
                 site->output_dir, site->build_id, fname);
        ensure_parent_dir(filepath);

        Buffer buf;
        buffer_init(&buf);
        if (download_to_buffer(url, &buf) == 0 && buf.size > 10) {
            FILE *f = fopen(filepath, "wb");
            if (f) {
                fwrite(buf.data, 1, buf.size, f);
                fclose(f);
                printf("  [+] Downloaded: %s (%zu bytes)\n", fname, buf.size);
            }
        }
        buffer_free(&buf);
    }

    return 0;
}

/* ── JavaScript beautifier ────────────────────────────────── */

int is_minified(const char *code, size_t len) {
    int newlines = 0;
    size_t check = len < 2000 ? len : 2000;
    for (size_t i = 0; i < check; i++) {
        if (code[i] == '\n') newlines++;
    }
    return (newlines < 10);
}

char *beautify_js(const char *code, size_t len) {
    /* Allocate generous output buffer (beautified code is ~2-3x larger) */
    size_t cap = len * 3 + 1024;
    char *out = malloc(cap);
    if (!out) return NULL;

    size_t pos = 0;
    int indent = 0;
    int in_string = 0;
    char str_char = 0;
    int in_template = 0;
    int in_comment = 0;       /* 1 = line comment, 2 = block comment */
    int prev_was_newline = 0;

    #define EMIT(c) do { \
        if (pos + 10 >= cap) { \
            cap *= 2; \
            char *tmp = realloc(out, cap); \
            if (!tmp) { free(out); return NULL; } \
            out = tmp; \
        } \
        out[pos++] = (c); \
    } while(0)

    #define EMIT_INDENT() do { \
        EMIT('\n'); \
        for (int _j = 0; _j < indent * 2; _j++) EMIT(' '); \
        prev_was_newline = 1; \
    } while(0)

    for (size_t i = 0; i < len; i++) {
        char c = code[i];
        char next = (i + 1 < len) ? code[i + 1] : '\0';

        /* Handle comments */
        if (!in_string && !in_template) {
            if (in_comment == 1) {
                EMIT(c);
                if (c == '\n') in_comment = 0;
                continue;
            }
            if (in_comment == 2) {
                EMIT(c);
                if (c == '*' && next == '/') {
                    EMIT('/');
                    i++;
                    in_comment = 0;
                }
                continue;
            }
            if (c == '/' && next == '/') {
                in_comment = 1;
                EMIT(c);
                continue;
            }
            if (c == '/' && next == '*') {
                in_comment = 2;
                EMIT(c);
                continue;
            }
        }

        /* Handle strings */
        if (!in_comment) {
            if (c == '`' && !in_string) {
                in_template = !in_template;
                EMIT(c);
                continue;
            }
            if (in_template) {
                EMIT(c);
                continue;
            }
            if ((c == '"' || c == '\'') && !in_string) {
                in_string = 1;
                str_char = c;
                EMIT(c);
                continue;
            }
            if (in_string) {
                EMIT(c);
                if (c == str_char && (i == 0 || code[i-1] != '\\'))
                    in_string = 0;
                continue;
            }
        }

        /* Structure characters */
        if (c == '{' || c == '[') {
            EMIT(c);
            indent++;
            EMIT_INDENT();
            prev_was_newline = 1;
            continue;
        }
        if (c == '}' || c == ']') {
            indent--;
            if (indent < 0) indent = 0;
            EMIT_INDENT();
            EMIT(c);
            prev_was_newline = 0;
            continue;
        }
        if (c == ';') {
            EMIT(c);
            EMIT_INDENT();
            prev_was_newline = 1;
            continue;
        }
        if (c == ',') {
            EMIT(c);
            /* Newline after comma at top-level or in objects */
            if (indent <= 3) {
                EMIT_INDENT();
                prev_was_newline = 1;
            } else {
                EMIT(' ');
                prev_was_newline = 0;
            }
            continue;
        }

        /* Skip redundant whitespace */
        if (c == '\n' || c == '\r') {
            if (!prev_was_newline) {
                EMIT('\n');
                prev_was_newline = 1;
            }
            continue;
        }

        prev_was_newline = 0;
        EMIT(c);
    }

    out[pos] = '\0';

    #undef EMIT
    #undef EMIT_INDENT

    return out;
}

/* ── Beautify a JS file and save to output ────────────────── */

static int beautify_and_save(const char *input_path, const char *output_path) {
    FILE *f = fopen(input_path, "rb");
    if (!f) return -1;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size <= 0 || size > 50 * 1024 * 1024) { /* skip files > 50MB */
        fclose(f);
        return -1;
    }

    char *code = malloc((size_t)size + 1);
    if (!code) { fclose(f); return -1; }
    size_t nread = fread(code, 1, (size_t)size, f);
    code[nread] = '\0';
    fclose(f);

    char *beautified = NULL;
    if (is_minified(code, (size_t)size)) {
        beautified = beautify_js(code, (size_t)size);
    }

    ensure_parent_dir(output_path);

    FILE *out = fopen(output_path, "wb");
    if (!out) {
        free(code);
        free(beautified);
        return -1;
    }

    if (beautified) {
        fwrite(beautified, 1, strlen(beautified), out);
        free(beautified);
    } else {
        fwrite(code, 1, (size_t)size, out);
    }

    fclose(out);
    free(code);
    return 0;
}

/* ── Generate package.json ────────────────────────────────── */

int generate_package_json(Site *site) {
    char filepath[MAX_PATH_LEN];
    snprintf(filepath, sizeof(filepath), "%s/reconstructed/package.json",
             site->output_dir);
    ensure_parent_dir(filepath);

    FILE *f = fopen(filepath, "w");
    if (!f) return -1;

    fprintf(f,
        "{\n"
        "  \"name\": \"%s-reconstructed\",\n"
        "  \"version\": \"1.0.0\",\n"
        "  \"private\": true,\n"
        "  \"description\": \"Reconstructed from %s (build: %s)\",\n"
        "  \"scripts\": {\n"
        "    \"dev\": \"next dev\",\n"
        "    \"build\": \"next build\",\n"
        "    \"start\": \"next start\"\n"
        "  },\n"
        "  \"dependencies\": {\n"
        "    \"next\": \"latest\",\n"
        "    \"react\": \"latest\",\n"
        "    \"react-dom\": \"latest\"\n"
        "  }\n"
        "}\n",
        site->domain, site->domain, site->build_id);

    fclose(f);
    printf("  [+] Generated package.json\n");
    return 0;
}

/* ── Generate next.config.js ──────────────────────────────── */

int generate_next_config(Site *site) {
    char filepath[MAX_PATH_LEN];
    snprintf(filepath, sizeof(filepath), "%s/reconstructed/next.config.js",
             site->output_dir);
    ensure_parent_dir(filepath);

    FILE *f = fopen(filepath, "w");
    if (!f) return -1;

    fprintf(f,
        "/** @type {import('next').NextConfig} */\n"
        "const nextConfig = {\n"
        "  // Reconstructed from: %s\n"
        "  // Build ID: %s\n"
        "  // Router: %s\n",
        site->url, site->build_id,
        site->is_app_router ? "App Router" : "Pages Router");

    if (site->asset_prefix[0]) {
        fprintf(f,
        "  // Original asset prefix: %s\n"
        "  assetPrefix: '%s',\n",
        site->asset_prefix, site->asset_prefix);
    }

    fprintf(f,
        "}\n\n"
        "module.exports = nextConfig\n");

    fclose(f);
    printf("  [+] Generated next.config.js\n");
    return 0;
}

/* ── Generate analysis report ─────────────────────────────── */

int generate_report(Site *site) {
    char filepath[MAX_PATH_LEN];
    snprintf(filepath, sizeof(filepath), "%s/report.md", site->output_dir);
    ensure_parent_dir(filepath);

    FILE *f = fopen(filepath, "w");
    if (!f) return -1;

    fprintf(f, "# CCDES Analysis Report\n\n");
    fprintf(f, "## Site Information\n\n");
    fprintf(f, "- **URL**: %s\n", site->url);
    fprintf(f, "- **Domain**: %s\n", site->domain);
    fprintf(f, "- **Build ID**: %s\n", site->build_id);
    fprintf(f, "- **Router**: %s\n",
            site->is_app_router ? "App Router (RSC)" : "Pages Router");
    if (site->asset_prefix[0])
        fprintf(f, "- **Asset Prefix**: %s\n", site->asset_prefix);

    fprintf(f, "\n## Assets Found\n\n");
    fprintf(f, "| Type   | Count |\n");
    fprintf(f, "|--------|-------|\n");
    int js = 0, css = 0, font = 0, img = 0, other = 0;
    for (int i = 0; i < site->asset_count; i++) {
        switch (site->assets[i].type) {
            case ASSET_JS:    js++;    break;
            case ASSET_CSS:   css++;   break;
            case ASSET_FONT:  font++;  break;
            case ASSET_IMAGE: img++;   break;
            default:          other++; break;
        }
    }
    fprintf(f, "| JS     | %d    |\n", js);
    fprintf(f, "| CSS    | %d    |\n", css);
    fprintf(f, "| Fonts  | %d    |\n", font);
    fprintf(f, "| Images | %d    |\n", img);
    fprintf(f, "| Other  | %d    |\n", other);
    fprintf(f, "| **Total** | **%d** |\n", site->asset_count);

    fprintf(f, "\n## Detected Routes\n\n");
    if (site->route_count > 0) {
        fprintf(f, "| Route | File |\n");
        fprintf(f, "|-------|------|\n");
        for (int i = 0; i < site->route_count; i++) {
            fprintf(f, "| `%s` | `%s` |\n",
                    site->routes[i].route, site->routes[i].file);
        }
    } else {
        fprintf(f, "No routes could be extracted from chunk names.\n");
    }

    fprintf(f, "\n## Asset List\n\n");
    fprintf(f, "### JavaScript\n\n");
    for (int i = 0; i < site->asset_count; i++) {
        if (site->assets[i].type == ASSET_JS)
            fprintf(f, "- `%s`\n", site->assets[i].local_path);
    }
    fprintf(f, "\n### CSS\n\n");
    for (int i = 0; i < site->asset_count; i++) {
        if (site->assets[i].type == ASSET_CSS)
            fprintf(f, "- `%s`\n", site->assets[i].local_path);
    }
    fprintf(f, "\n### Fonts\n\n");
    for (int i = 0; i < site->asset_count; i++) {
        if (site->assets[i].type == ASSET_FONT)
            fprintf(f, "- `%s`\n", site->assets[i].local_path);
    }

    fprintf(f, "\n## Output Structure\n\n");
    fprintf(f, "```\n");
    fprintf(f, "%s/\n", site->output_dir);
    fprintf(f, "├── raw/                  # Original downloaded files\n");
    fprintf(f, "│   ├── index.html\n");
    fprintf(f, "│   └── _next/static/     # JS, CSS, fonts, media\n");
    fprintf(f, "├── reconstructed/        # Rebuilt project skeleton\n");
    fprintf(f, "│   ├── app/ or pages/    # Route components (beautified)\n");
    fprintf(f, "│   ├── styles/           # CSS files\n");
    fprintf(f, "│   ├── package.json\n");
    fprintf(f, "│   └── next.config.js\n");
    fprintf(f, "├── sourcemaps/           # Source maps (if found)\n");
    fprintf(f, "└── report.md             # This file\n");
    fprintf(f, "```\n");

    fprintf(f, "\n---\n");
    fprintf(f, "*Generated by CCDES v%s*\n", CCDES_VERSION);

    fclose(f);
    printf("  [+] Generated report.md\n");
    return 0;
}

/* ── Reconstruct the project structure ────────────────────── */

int reconstruct_project(Site *site) {
    printf("\n[*] Reconstructing project structure...\n");

    /* 1. Save original HTML */
    {
        char filepath[MAX_PATH_LEN];
        snprintf(filepath, sizeof(filepath), "%s/raw/index.html",
                 site->output_dir);
        ensure_parent_dir(filepath);
        FILE *f = fopen(filepath, "wb");
        if (f) {
            fwrite(site->html.data, 1, site->html.size, f);
            fclose(f);
            printf("  [+] Saved index.html (%zu bytes)\n", site->html.size);
        }
    }

    /* 2. Save __NEXT_DATA__ if present */
    if (site->next_data) {
        char filepath[MAX_PATH_LEN];
        snprintf(filepath, sizeof(filepath), "%s/raw/__NEXT_DATA__.json",
                 site->output_dir);
        ensure_parent_dir(filepath);
        FILE *f = fopen(filepath, "wb");
        if (f) {
            fwrite(site->next_data, 1, site->next_data_len, f);
            fclose(f);
            printf("  [+] Saved __NEXT_DATA__.json\n");
        }
    }

    /* 3. Create reconstructed route files (beautified JS) */
    printf("  [*] Beautifying and mapping route components...\n");

    int beautified_count = 0;
    for (int i = 0; i < site->route_count; i++) {
        Route *r = &site->routes[i];

        /* Find the raw downloaded chunk */
        char raw_path[MAX_PATH_LEN];
        snprintf(raw_path, sizeof(raw_path), "%s/raw/%s",
                 site->output_dir, r->chunk);

        /* Output path in reconstructed/ */
        char out_path[MAX_PATH_LEN];
        snprintf(out_path, sizeof(out_path), "%s/reconstructed/%s/%s",
                 site->output_dir, r->route, r->file);

        if (beautify_and_save(raw_path, out_path) == 0)
            beautified_count++;
    }
    printf("  [+] Beautified %d route components\n", beautified_count);

    /* 4. Beautify remaining JS chunks into reconstructed/chunks/ */
    printf("  [*] Beautifying shared chunks...\n");
    int shared_count = 0;
    for (int i = 0; i < site->asset_count; i++) {
        Asset *a = &site->assets[i];
        if (a->type != ASSET_JS || !a->downloaded) continue;

        /* Skip route chunks (already handled) */
        int is_route = 0;
        for (int j = 0; j < site->route_count; j++) {
            if (strcmp(site->routes[j].chunk, a->local_path) == 0) {
                is_route = 1;
                break;
            }
        }
        if (is_route) continue;

        char raw_path[MAX_PATH_LEN];
        snprintf(raw_path, sizeof(raw_path), "%s/raw/%s",
                 site->output_dir, a->local_path);

        /* Extract just the filename for the output */
        const char *basename = strrchr(a->local_path, '/');
        basename = basename ? basename + 1 : a->local_path;

        char out_path[MAX_PATH_LEN];
        snprintf(out_path, sizeof(out_path),
                 "%s/reconstructed/chunks/%s", site->output_dir, basename);

        if (beautify_and_save(raw_path, out_path) == 0)
            shared_count++;
    }
    printf("  [+] Beautified %d shared chunks\n", shared_count);

    /* 5. Copy CSS files to reconstructed/styles/ */
    printf("  [*] Organizing CSS files...\n");
    int css_count = 0;
    for (int i = 0; i < site->asset_count; i++) {
        Asset *a = &site->assets[i];
        if (a->type != ASSET_CSS || !a->downloaded) continue;

        char raw_path[MAX_PATH_LEN];
        snprintf(raw_path, sizeof(raw_path), "%s/raw/%s",
                 site->output_dir, a->local_path);

        const char *basename = strrchr(a->local_path, '/');
        basename = basename ? basename + 1 : a->local_path;

        char out_path[MAX_PATH_LEN];
        snprintf(out_path, sizeof(out_path),
                 "%s/reconstructed/styles/%s", site->output_dir, basename);
        ensure_parent_dir(out_path);

        /* Copy CSS file */
        FILE *in = fopen(raw_path, "rb");
        if (!in) continue;
        FILE *out = fopen(out_path, "wb");
        if (!out) { fclose(in); continue; }

        char cbuf[8192];
        size_t n;
        while ((n = fread(cbuf, 1, sizeof(cbuf), in)) > 0)
            fwrite(cbuf, 1, n, out);

        fclose(in);
        fclose(out);
        css_count++;
    }
    printf("  [+] Organized %d CSS files\n", css_count);

    /* 6. Generate project files */
    generate_package_json(site);
    generate_next_config(site);
    generate_report(site);

    return 0;
}
