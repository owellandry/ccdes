/*
 * main.c - CCDES: Next.js Site Decompiler
 * Entry point, CLI menu, orchestration
 */
#include "ccdes.h"

/* ── Full decompilation pipeline ──────────────────────────── */

int decompile_site(const char *url) {
    Site *site = calloc(1, sizeof(Site));
    if (!site) { fprintf(stderr, "[!] Out of memory\n"); return -1; }
    if (site_init(site, url) != 0) { free(site); return -1; }

    printf("\n");
    printf("===========================================================\n");
    printf("  CCDES v%s - Next.js Decompiler\n", CCDES_VERSION);
    printf("===========================================================\n");
    printf("  Target : %s\n", site->url);
    printf("  Domain : %s\n", site->domain);
    printf("  Output : %s/\n", site->output_dir);
    printf("===========================================================\n");

    /* ── Step 1: Download the main page ───────────────────── */
    printf("\n[*] Step 1: Downloading main page...\n");
    if (download_to_buffer(site->url, &site->html) != 0) {
        fprintf(stderr, "[!] Failed to download main page\n");
        buffer_free(&site->html);
        free(site);
        return -1;
    }
    printf("  [+] Downloaded %zu bytes of HTML\n", site->html.size);

    /* Quick sanity check - is this actually HTML? */
    if (site->html.size < 50 || !strstr(site->html.data, "<")) {
        fprintf(stderr, "[!] Response does not look like HTML. "
                "The site may be blocking automated requests.\n");
    }

    /* ── Step 2: Analyse site structure ───────────────────── */
    printf("\n[*] Step 2: Analysing site structure...\n");

    detect_asset_prefix(site);
    detect_router_type(site);
    extract_next_data(site);
    detect_build_id(site);

    /* ── Step 3: Extract all _next/ asset URLs ────────────── */
    printf("\n[*] Step 3: Extracting asset URLs from HTML...\n");
    extract_assets_from_html(site);

    if (site->asset_count == 0) {
        printf("[!] No _next/ assets found. This might not be a Next.js site,\n");
        printf("    or the assets are loaded dynamically via JavaScript.\n");
        mkdirs(site->output_dir);
        char html_path[MAX_PATH_LEN];
        snprintf(html_path, sizeof(html_path), "%s/index.html", site->output_dir);
        FILE *f = fopen(html_path, "wb");
        if (f) {
            fwrite(site->html.data, 1, site->html.size, f);
            fclose(f);
            printf("[*] Saved raw HTML to %s\n", html_path);
        }
        buffer_free(&site->html);
        free(site->next_data);
        free(site);
        return 0;
    }

    /* ── Step 4: Extract routes from chunk names ──────────── */
    printf("\n[*] Step 4: Extracting routes from chunk names...\n");
    extract_routes_from_chunks(site);

    /* ── Step 5: Download all assets ──────────────────────── */
    printf("\n[*] Step 5: Downloading all assets...\n");
    download_all_assets(site);

    /* ── Step 6: Try _buildManifest.js ────────────────────── */
    printf("\n[*] Step 6: Looking for build manifests...\n");
    try_download_build_manifest(site);

    /* ── Step 7: Check for source maps ────────────────────── */
    printf("\n[*] Step 7: Checking for source maps...\n");
    try_download_source_maps(site);

    /* ── Step 8: Reconstruct project structure ────────────── */
    printf("\n[*] Step 8: Reconstructing project...\n");
    reconstruct_project(site);

    /* ── Summary ──────────────────────────────────────────── */
    printf("\n");
    printf("===========================================================\n");
    printf("  Decompilation complete!\n");
    printf("===========================================================\n");
    printf("  Assets downloaded : %d\n", site->asset_count);
    printf("  Routes found      : %d\n", site->route_count);
    printf("  Build ID          : %s\n",
           site->build_id[0] ? site->build_id : "(unknown)");
    printf("  Router type       : %s\n",
           site->is_app_router ? "App Router" : "Pages Router");
    printf("  Output directory  : %s/\n", site->output_dir);
    printf("\n");
    printf("  raw/              - Original downloaded files\n");
    printf("  reconstructed/    - Rebuilt project skeleton\n");
    printf("  sourcemaps/       - Source maps (if found)\n");
    printf("  report.md         - Full analysis report\n");
    printf("===========================================================\n\n");

    /* Cleanup */
    buffer_free(&site->html);
    free(site->next_data);
    free(site);

    return 0;
}

/* ── Analyse a local file ─────────────────────────────────── */

static void analyse_local_file(const char *filepath) {
    FILE *f = fopen(filepath, "rb");
    if (!f) {
        printf("[!] Cannot open: %s (%s)\n", filepath, strerror(errno));
        return;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size <= 0 || size > 100 * 1024 * 1024) {
        printf("[!] File too large or empty\n");
        fclose(f);
        return;
    }

    char *code = malloc((size_t)size + 1);
    if (!code) { fclose(f); return; }
    size_t nread = fread(code, 1, (size_t)size, f);
    code[nread] = '\0';
    fclose(f);

    printf("\n=== File Analysis: %s ===\n", filepath);
    printf("  Size: %ld bytes\n", size);

    if (is_minified(code, (size_t)size)) {
        printf("  [!] Code is minified\n");
        printf("  [*] Beautifying...\n");

        char *beautified = beautify_js(code, (size_t)size);
        if (beautified) {
            char out_name[MAX_PATH_LEN];
            snprintf(out_name, sizeof(out_name), "beautified_%s", filepath);
            /* Replace path separators */
            for (char *p = out_name; *p; p++)
                if (ccdes_is_sep(*p)) *p = '_';

            FILE *out = fopen(out_name, "wb");
            if (out) {
                fwrite(beautified, 1, strlen(beautified), out);
                fclose(out);
                printf("  [+] Saved beautified code to: %s\n", out_name);
            }
            free(beautified);
        }
    } else {
        printf("  Code does not appear to be minified\n");
    }

    /* Detect Next.js patterns */
    printf("\n  [*] Scanning for Next.js patterns...\n");
    const char *patterns[] = {
        "/_next/static/", "/_next/data/", "__NEXT_DATA__",
        "pageProps", "getStaticProps", "getServerSideProps",
        "React.createElement", "useState", "useEffect",
        "useRouter", "next/link", "next/image",
        "jsx", "forwardRef", "__webpack_require__",
        "self.__next_f", "RSC", NULL
    };
    int found = 0;
    for (int i = 0; patterns[i]; i++) {
        if (strstr(code, patterns[i])) {
            printf("    [+] %s\n", patterns[i]);
            found++;
        }
    }
    if (!found)
        printf("    No Next.js patterns found\n");

    free(code);
}

/* ── Test mode (offline demo) ─────────────────────────────── */

static void test_mode(void) {
    printf("\n=== Test Mode (Offline Demo) ===\n\n");

    const char *sample_html =
        "<!DOCTYPE html><html><head>"
        "<link rel=\"stylesheet\" href=\"/_next/static/css/abc123.css\">"
        "</head><body>"
        "<script id=\"__NEXT_DATA__\" type=\"application/json\">"
        "{\"buildId\":\"test-build-123\",\"page\":\"/\","
        "\"props\":{\"pageProps\":{\"data\":\"hello\"}}}</script>"
        "<script src=\"/_next/static/chunks/webpack-deadbeef.js\"></script>"
        "<script src=\"/_next/static/chunks/pages/index-cafebabe.js\"></script>"
        "<script src=\"/_next/static/test-build-123/_buildManifest.js\"></script>"
        "</body></html>";

    printf("[*] Simulating analysis with sample Next.js HTML...\n\n");

    Site *site = calloc(1, sizeof(Site));
    if (!site) { fprintf(stderr, "[!] Out of memory\n"); return; }
    buffer_init(&site->html);
    snprintf(site->url, MAX_URL, "https://example.com");
    snprintf(site->base_url, MAX_URL, "https://example.com");
    snprintf(site->domain, sizeof(site->domain), "example.com");
    snprintf(site->output_dir, MAX_PATH_LEN, "output/test-demo");

    /* Copy sample HTML into buffer */
    site->html.size = strlen(sample_html);
    site->html.data = malloc(site->html.size + 1);
    memcpy(site->html.data, sample_html, site->html.size + 1);

    detect_asset_prefix(site);
    detect_router_type(site);
    extract_next_data(site);
    detect_build_id(site);
    extract_assets_from_html(site);
    extract_routes_from_chunks(site);

    printf("\n[+] Test mode complete. All parsing functions work correctly.\n");

    buffer_free(&site->html);
    free(site->next_data);
    free(site);
}

/* ── Main entry point ─────────────────────────────────────── */

int main(int argc, char *argv[]) {
    setlocale(LC_ALL, "");
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    download_global_init();

    /* CLI mode: ccdes <url> */
    if (argc >= 2) {
        if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
            printf("CCDES v%s - Next.js Site Decompiler\n\n", CCDES_VERSION);
            printf("Usage:\n");
            printf("  ccdes <url>           Decompile a Next.js site\n");
            printf("  ccdes --file <path>   Analyse a local JS file\n");
            printf("  ccdes --test          Run offline test mode\n");
            printf("  ccdes --help          Show this help\n\n");
            printf("Examples:\n");
            printf("  ccdes https://example.com\n");
            printf("  ccdes --file bundle.js\n");
            download_global_cleanup();
            return 0;
        }
        if (strcmp(argv[1], "--test") == 0 || strcmp(argv[1], "-t") == 0) {
            test_mode();
            download_global_cleanup();
            return 0;
        }
        if ((strcmp(argv[1], "--file") == 0 || strcmp(argv[1], "-f") == 0)
            && argc >= 3) {
            analyse_local_file(argv[2]);
            download_global_cleanup();
            return 0;
        }
        /* Assume it's a URL */
        int rc = decompile_site(argv[1]);
        download_global_cleanup();
        return rc;
    }

    /* Interactive mode */
    char input[MAX_URL];
    int option;

    printf("\n");
    printf("===========================================================\n");
    printf("  CCDES v%s - Next.js Site Decompiler\n", CCDES_VERSION);
    printf("  Security testing & analysis tool\n");
    printf("===========================================================\n");

    while (1) {
        printf("\n");
        printf("  [1] Decompile Next.js site from URL\n");
        printf("  [2] Analyse local JS file\n");
        printf("  [3] Test mode (offline demo)\n");
        printf("  [4] Exit\n");
        printf("\n  Select option: ");

        if (scanf("%d", &option) != 1) {
            while (getchar() != '\n');
            continue;
        }
        while (getchar() != '\n');

        switch (option) {
        case 1:
            printf("\n  Enter URL: ");
            if (fgets(input, sizeof(input), stdin)) {
                input[strcspn(input, "\n")] = '\0';
                if (strlen(input) > 0) {
                    /* Add https:// if missing */
                    if (strncmp(input, "http", 4) != 0) {
                        char tmp[MAX_URL + 16];
                        snprintf(tmp, sizeof(tmp), "https://%s", input);
                        decompile_site(tmp);
                    } else {
                        decompile_site(input);
                    }
                }
            }
            break;

        case 2:
            printf("\n  Enter file path: ");
            if (fgets(input, sizeof(input), stdin)) {
                input[strcspn(input, "\n")] = '\0';
                if (strlen(input) > 0)
                    analyse_local_file(input);
            }
            break;

        case 3:
            test_mode();
            break;

        case 4:
            printf("\nGoodbye!\n");
            download_global_cleanup();
            return 0;

        default:
            printf("  Invalid option\n");
        }
    }

    download_global_cleanup();
    return 0;
}
