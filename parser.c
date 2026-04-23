/*
 * parser.c - HTML parsing, asset extraction, route detection
 * Handles both App Router (RSC) and Pages Router (__NEXT_DATA__) sites
 */
#include "ccdes.h"

/* ── Utility: ends_with ────────────────────────────────────── */

static int ends_with(const char *str, const char *suffix) {
    size_t slen = strlen(str);
    size_t xlen = strlen(suffix);
    if (xlen > slen) return 0;
    return strcmp(str + slen - xlen, suffix) == 0;
}

/* ── URL decode (%XX → char) ──────────────────────────────── */

void url_decode(const char *src, char *dst, size_t dst_size) {
    size_t i = 0, j = 0;
    while (src[i] && j < dst_size - 1) {
        if (src[i] == '%' && isxdigit((unsigned char)src[i+1])
                          && isxdigit((unsigned char)src[i+2])) {
            char hex[3] = { src[i+1], src[i+2], '\0' };
            dst[j++] = (char)strtol(hex, NULL, 16);
            i += 3;
        } else {
            dst[j++] = src[i++];
        }
    }
    dst[j] = '\0';
}

/* ── Parse URL into scheme / host / path ──────────────────── */

int parse_url_parts(const char *url, char *scheme, size_t ss,
                    char *host, size_t hs, char *path, size_t ps)
{
    const char *sep = strstr(url, "://");
    if (!sep) return -1;

    size_t slen = (size_t)(sep - url);
    if (slen >= ss) return -1;
    memcpy(scheme, url, slen);
    scheme[slen] = '\0';

    const char *h = sep + 3;
    const char *slash = strchr(h, '/');
    if (slash) {
        size_t hlen = (size_t)(slash - h);
        if (hlen >= hs) return -1;
        memcpy(host, h, hlen);
        host[hlen] = '\0';
        snprintf(path, ps, "%s", slash);
    } else {
        snprintf(host, hs, "%s", h);
        snprintf(path, ps, "/");
    }
    return 0;
}

/* ── Initialise Site struct from a URL ─────────────────────── */

int site_init(Site *site, const char *url) {
    memset(site, 0, sizeof(*site));
    buffer_init(&site->html);

    snprintf(site->url, MAX_URL, "%s", url);

    char scheme[16], host[256], path[MAX_URL];
    if (parse_url_parts(url, scheme, sizeof(scheme),
                        host, sizeof(host), path, sizeof(path)) != 0) {
        fprintf(stderr, "[!] Invalid URL: %s\n", url);
        return -1;
    }

    snprintf(site->domain, sizeof(site->domain), "%s", host);
    snprintf(site->base_url, MAX_URL, "%s://%s", scheme, host);
    snprintf(site->output_dir, MAX_PATH_LEN, "output/%s", host);

    return 0;
}

/* ── Determine asset type from URL / extension ─────────────── */

static AssetType classify_asset(const char *url) {
    if (ends_with(url, ".js"))    return ASSET_JS;
    if (ends_with(url, ".css"))   return ASSET_CSS;
    if (ends_with(url, ".woff2")) return ASSET_FONT;
    if (ends_with(url, ".woff"))  return ASSET_FONT;
    if (ends_with(url, ".ttf"))   return ASSET_FONT;
    if (ends_with(url, ".eot"))   return ASSET_FONT;
    if (ends_with(url, ".otf"))   return ASSET_FONT;
    if (ends_with(url, ".png"))   return ASSET_IMAGE;
    if (ends_with(url, ".jpg"))   return ASSET_IMAGE;
    if (ends_with(url, ".jpeg"))  return ASSET_IMAGE;
    if (ends_with(url, ".gif"))   return ASSET_IMAGE;
    if (ends_with(url, ".svg"))   return ASSET_IMAGE;
    if (ends_with(url, ".webp"))  return ASSET_IMAGE;
    if (ends_with(url, ".ico"))   return ASSET_IMAGE;
    if (ends_with(url, ".avif"))  return ASSET_IMAGE;
    if (ends_with(url, ".map"))   return ASSET_MAP;
    if (ends_with(url, ".json"))  return ASSET_JSON;
    return ASSET_OTHER;
}

/* ── Add an asset to the site (deduplicating) ─────────────── */

static int add_asset(Site *site, const char *raw_url) {
    if (site->asset_count >= MAX_ASSETS) return -1;

    /* Build full URL */
    char full_url[MAX_URL];
    if (raw_url[0] == '/' && raw_url[1] != '/') {
        snprintf(full_url, MAX_URL, "%s%s", site->base_url, raw_url);
    } else if (strncmp(raw_url, "//", 2) == 0) {
        snprintf(full_url, MAX_URL, "https:%s", raw_url);
    } else if (strncmp(raw_url, "http", 4) == 0) {
        snprintf(full_url, MAX_URL, "%s", raw_url);
    } else {
        snprintf(full_url, MAX_URL, "%s/%s", site->base_url, raw_url);
    }

    /* Deduplicate */
    for (int i = 0; i < site->asset_count; i++) {
        if (strcmp(site->assets[i].url, full_url) == 0) return 0;
    }

    Asset *a = &site->assets[site->asset_count];
    snprintf(a->url, MAX_URL, "%s", full_url);
    a->type = classify_asset(full_url);
    a->downloaded = 0;

    /* Derive local path: strip everything before _next/ */
    const char *next = strstr(raw_url, "_next/");
    if (next) {
        snprintf(a->local_path, MAX_PATH_LEN, "_next/%s", next + 6);
    } else {
        /* fallback: use the raw URL path */
        const char *last_slash = strrchr(raw_url, '/');
        if (last_slash)
            snprintf(a->local_path, MAX_PATH_LEN, "assets/%s", last_slash + 1);
        else
            snprintf(a->local_path, MAX_PATH_LEN, "assets/%s", raw_url);
    }

    site->asset_count++;
    return 1;
}

/* ── Extract all _next/ URLs from HTML ─────────────────────── */

static int extract_attr(Site *site, const char *attr_name) {
    if (!site->html.data) return 0;

    char pattern_dq[32], pattern_sq[32];
    snprintf(pattern_dq, sizeof(pattern_dq), "%s=\"", attr_name);
    snprintf(pattern_sq, sizeof(pattern_sq), "%s='", attr_name);

    int count = 0;
    const char *html = site->html.data;
    const char *pos = html;

    while (pos && *pos) {
        /* Try double quotes first, then single */
        const char *match = strstr(pos, pattern_dq);
        char quote = '"';
        if (!match) {
            match = strstr(pos, pattern_sq);
            quote = '\'';
        }
        if (!match) break;

        const char *val_start = match + strlen(quote == '"' ? pattern_dq : pattern_sq);
        const char *val_end = strchr(val_start, quote);
        if (!val_end) break;

        size_t vlen = (size_t)(val_end - val_start);
        if (vlen > 0 && vlen < MAX_URL) {
            char url[MAX_URL];
            memcpy(url, val_start, vlen);
            url[vlen] = '\0';

            if (strstr(url, "_next/")) {
                if (add_asset(site, url) > 0) count++;
            }
        }
        pos = val_end + 1;
    }
    return count;
}

int extract_assets_from_html(Site *site) {
    int count = 0;
    count += extract_attr(site, "src");
    count += extract_attr(site, "href");

    /* Also extract from <link> preload headers that might be in the HTML */
    count += extract_attr(site, "as");   /* catches some edge cases */

    printf("  [+] Found %d unique _next/ assets in HTML\n", site->asset_count);

    /* Count by type */
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
    printf("      JS: %d | CSS: %d | Fonts: %d | Images: %d | Other: %d\n",
           js, css, font, img, other);

    return count;
}

/* ── Detect CDN / asset prefix ─────────────────────────────── */

int detect_asset_prefix(Site *site) {
    if (!site->html.data) return -1;

    /* Look for patterns like /cdn/assets/XXXX/_next/ or similar prefixes */
    const char *p = strstr(site->html.data, "_next/static/");
    if (!p) return -1;

    /* Walk backwards to find the start of the path (after a quote char) */
    const char *start = p;
    while (start > site->html.data && *(start - 1) != '"' && *(start - 1) != '\''
           && *(start - 1) != '(' && *(start - 1) != ' ')
        start--;

    /* The prefix is everything between start and the "_next/" part */
    size_t prefix_len = (size_t)(p - start);
    if (prefix_len > 0 && prefix_len < MAX_URL) {
        memcpy(site->asset_prefix, start, prefix_len);
        site->asset_prefix[prefix_len] = '\0';

        /* Remove trailing slash if present */
        size_t len = strlen(site->asset_prefix);
        if (len > 0 && site->asset_prefix[len - 1] == '/')
            site->asset_prefix[len - 1] = '\0';

        printf("  [+] Asset prefix: %s\n", site->asset_prefix);
    }

    return 0;
}

/* ── Detect build ID ──────────────────────────────────────── */

int detect_build_id(Site *site) {
    if (!site->html.data) return -1;

    /* Method 1: From __NEXT_DATA__ JSON (Pages Router) */
    if (site->next_data) {
        const char *bid = strstr(site->next_data, "\"buildId\":\"");
        if (bid) {
            bid += 11; /* skip "buildId":" */
            const char *end = strchr(bid, '"');
            if (end) {
                size_t len = (size_t)(end - bid);
                if (len < sizeof(site->build_id)) {
                    memcpy(site->build_id, bid, len);
                    site->build_id[len] = '\0';
                    printf("  [+] Build ID (from __NEXT_DATA__): %s\n",
                           site->build_id);
                    return 0;
                }
            }
        }
    }

    /* Method 2: From asset prefix path pattern
     * e.g. /cdn/assets/30b026743bd8945a/_next/ → build_id = 30b026743bd8945a
     */
    if (site->asset_prefix[0]) {
        const char *last_slash = strrchr(site->asset_prefix, '/');
        const char *id_start = last_slash ? last_slash + 1 : site->asset_prefix;
        if (strlen(id_start) > 4) {
            snprintf(site->build_id, sizeof(site->build_id), "%s", id_start);
            printf("  [+] Build ID (from asset prefix): %s\n", site->build_id);
            return 0;
        }
    }

    /* Method 3: From _buildManifest.js URL pattern
     * /_next/static/BUILD_ID/_buildManifest.js
     */
    const char *bm = strstr(site->html.data, "_next/static/");
    while (bm) {
        bm += 13; /* skip "_next/static/" */
        const char *slash = strchr(bm, '/');
        if (slash && strstr(slash, "_buildManifest")) {
            size_t len = (size_t)(slash - bm);
            if (len > 4 && len < sizeof(site->build_id)) {
                memcpy(site->build_id, bm, len);
                site->build_id[len] = '\0';
                printf("  [+] Build ID (from manifest URL): %s\n",
                       site->build_id);
                return 0;
            }
        }
        bm = strstr(bm, "_next/static/");
    }

    printf("  [!] Could not detect build ID\n");
    return -1;
}

/* ── Detect router type (App Router vs Pages Router) ──────── */

int detect_router_type(Site *site) {
    if (!site->html.data) return -1;

    /* App Router indicators */
    int app_signals = 0;
    if (strstr(site->html.data, "chunks/app/"))    app_signals++;
    if (strstr(site->html.data, "main-app-"))      app_signals++;
    if (strstr(site->html.data, "\"RSC\""))        app_signals++;
    if (strstr(site->html.data, "app-pages-internals")) app_signals++;

    /* Pages Router indicators */
    int pages_signals = 0;
    if (strstr(site->html.data, "__NEXT_DATA__"))  pages_signals++;
    if (strstr(site->html.data, "chunks/pages/"))  pages_signals++;
    if (strstr(site->html.data, "getServerSideProps")) pages_signals++;
    if (strstr(site->html.data, "getStaticProps"))     pages_signals++;

    site->is_app_router = (app_signals >= pages_signals) ? 1 : 0;

    printf("  [+] Router type: %s (app=%d, pages=%d signals)\n",
           site->is_app_router ? "App Router" : "Pages Router",
           app_signals, pages_signals);

    return 0;
}

/* ── Extract __NEXT_DATA__ JSON (Pages Router) ────────────── */

int extract_next_data(Site *site) {
    if (!site->html.data) return -1;

    const char *tag = strstr(site->html.data, "__NEXT_DATA__");
    if (!tag) return 0; /* not present = App Router, not an error */

    /* Find the > that closes the script tag */
    const char *start = strchr(tag, '>');
    if (!start) return -1;
    start++; /* skip '>' */

    /* Find closing </script> */
    const char *end = strstr(start, "</script>");
    if (!end) return -1;

    size_t len = (size_t)(end - start);
    site->next_data = malloc(len + 1);
    if (!site->next_data) return -1;

    memcpy(site->next_data, start, len);
    site->next_data[len] = '\0';
    site->next_data_len = len;

    printf("  [+] Extracted __NEXT_DATA__: %zu bytes\n", len);
    return 1;
}

/* ── Strip hash from chunk filename ───────────────────────── */

static void strip_chunk_hash(const char *filename, char *clean, size_t csize) {
    const char *ext = strrchr(filename, '.');
    if (!ext) { snprintf(clean, csize, "%s", filename); return; }

    /* Find the last '-' before the extension */
    const char *dash = ext;
    while (dash > filename && *(dash - 1) != '-') dash--;

    if (dash > filename) {
        /* Check if everything between dash and ext looks like a hex hash */
        int looks_like_hash = 1;
        for (const char *c = dash; c < ext; c++) {
            if (!isxdigit((unsigned char)*c)) { looks_like_hash = 0; break; }
        }

        if (looks_like_hash && (ext - dash) >= 8) {
            size_t prefix_len = (size_t)(dash - filename - 1);
            snprintf(clean, csize, "%.*s%s", (int)prefix_len, filename, ext);
            return;
        }
    }
    snprintf(clean, csize, "%s", filename);
}

/* ── Extract routes from app/ chunk names ─────────────────── */

int extract_routes_from_chunks(Site *site) {
    for (int i = 0; i < site->asset_count; i++) {
        if (site->assets[i].type != ASSET_JS) continue;

        const char *url = site->assets[i].url;

        /* Look for app/ or pages/ in chunk paths */
        const char *app_chunk = strstr(url, "chunks/app/");
        const char *page_chunk = strstr(url, "chunks/pages/");
        const char *chunk_path = NULL;
        int is_app = 0;

        if (app_chunk) {
            chunk_path = app_chunk + 11; /* skip "chunks/app/" */
            is_app = 1;
        } else if (page_chunk) {
            chunk_path = page_chunk + 13; /* skip "chunks/pages/" */
            is_app = 0;
        }

        if (!chunk_path) continue;
        if (site->route_count >= MAX_ROUTES) break;

        /* URL-decode the chunk path */
        char decoded[512];
        url_decode(chunk_path, decoded, sizeof(decoded));

        /* The chunk path is like: [locale]/(home-layout)/page-HASH.js
         * Split into route directory and filename */
        char *last_slash = strrchr(decoded, '/');
        char route_dir[256] = "";
        char filename[256];

        if (last_slash) {
            size_t dir_len = (size_t)(last_slash - decoded);
            if (dir_len < sizeof(route_dir)) {
                memcpy(route_dir, decoded, dir_len);
                route_dir[dir_len] = '\0';
            }
            snprintf(filename, sizeof(filename), "%s", last_slash + 1);
        } else {
            snprintf(filename, sizeof(filename), "%s", decoded);
        }

        /* Strip hash from filename */
        char clean_name[256];
        strip_chunk_hash(filename, clean_name, sizeof(clean_name));

        Route *r = &site->routes[site->route_count];

        if (route_dir[0]) {
            snprintf(r->route, sizeof(r->route), "%s/%s",
                     is_app ? "app" : "pages", route_dir);
        } else {
            snprintf(r->route, sizeof(r->route), "%s",
                     is_app ? "app" : "pages");
        }
        snprintf(r->file, sizeof(r->file), "%s", clean_name);
        snprintf(r->chunk, MAX_PATH_LEN, "%s", site->assets[i].local_path);

        site->route_count++;
    }

    printf("  [+] Extracted %d routes from chunk names\n", site->route_count);
    for (int i = 0; i < site->route_count && i < 20; i++) {
        printf("      %s/%s\n", site->routes[i].route, site->routes[i].file);
    }
    if (site->route_count > 20)
        printf("      ... and %d more\n", site->route_count - 20);

    return site->route_count;
}
