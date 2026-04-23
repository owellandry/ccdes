/*
 * ccdes.h - Next.js Site Decompiler
 * Common types and function declarations
 */
#ifndef CCDES_H
#define CCDES_H

/* Suppress MSVC warnings about fopen/sprintf etc. */
#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#pragma warning(disable:4996)
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <curl/curl.h>

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#include <io.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#include <locale.h>

/* ── Platform helpers ──────────────────────────────────────── */

#ifdef _WIN32
#define CCDES_MKDIR(p) _mkdir(p)
#define CCDES_PATH_SEP '\\'
#else
#define CCDES_MKDIR(p) mkdir((p), 0755)
#define CCDES_PATH_SEP '/'
#endif

static inline int ccdes_is_sep(char c) {
    return c == '/' || c == '\\';
}

static inline char *ccdes_last_sep(char *s) {
    char *a = strrchr(s, '/');
    char *b = strrchr(s, '\\');
    if (!a) return b;
    if (!b) return a;
    return (a > b) ? a : b;
}

#define CCDES_VERSION "2.0.0"
#define MAX_URL       2048
#define MAX_PATH_LEN  1024
#define MAX_ASSETS    4096
#define MAX_ROUTES    512
#define DL_TIMEOUT    30L
#define DL_CONNECT_TO 15L

/* ── Buffer for downloaded content ─────────────────────────── */

typedef struct {
    char  *data;
    size_t size;
} Buffer;

void   buffer_init(Buffer *b);
void   buffer_free(Buffer *b);

/* ── Asset types ───────────────────────────────────────────── */

typedef enum {
    ASSET_JS,
    ASSET_CSS,
    ASSET_FONT,
    ASSET_IMAGE,
    ASSET_MAP,
    ASSET_JSON,
    ASSET_OTHER
} AssetType;

typedef struct {
    char      url[MAX_URL];         /* full or relative URL            */
    char      local_path[MAX_PATH_LEN]; /* path under output dir       */
    AssetType type;
    int       downloaded;           /* 1 = already fetched             */
} Asset;

/* ── Route extracted from chunk names ──────────────────────── */

typedef struct {
    char route[256];                /* e.g. /[locale]/(home-layout)    */
    char file[256];                 /* e.g. page.js                    */
    char chunk[MAX_PATH_LEN];      /* original chunk filename          */
} Route;

/* ── Site — main context for a decompilation run ───────────── */

typedef struct {
    /* input */
    char   url[MAX_URL];            /* user-supplied URL                */
    char   base_url[MAX_URL];       /* scheme + host (no trailing /)    */
    char   domain[256];             /* host only                        */

    /* downloaded HTML */
    Buffer html;

    /* detected configuration */
    char   build_id[256];
    char   asset_prefix[MAX_URL];   /* e.g. /cdn/assets/30b0…          */
    int    is_app_router;           /* 1 = App Router, 0 = Pages       */

    /* __NEXT_DATA__ (Pages Router) */
    char  *next_data;
    size_t next_data_len;

    /* discovered assets */
    Asset  assets[MAX_ASSETS];
    int    asset_count;

    /* discovered routes */
    Route  routes[MAX_ROUTES];
    int    route_count;

    /* output */
    char   output_dir[MAX_PATH_LEN];
} Site;

/* ── download.c ────────────────────────────────────────────── */

void download_global_init(void);
void download_global_cleanup(void);
int  download_to_buffer(const char *url, Buffer *buf);
int  download_to_file(const char *url, const char *filepath);

/* ── parser.c ──────────────────────────────────────────────── */

int  parse_url_parts(const char *url, char *scheme, size_t ss,
                     char *host, size_t hs, char *path, size_t ps);
int  site_init(Site *site, const char *url);
int  extract_assets_from_html(Site *site);
int  detect_asset_prefix(Site *site);
int  detect_build_id(Site *site);
int  detect_router_type(Site *site);
int  extract_next_data(Site *site);
int  extract_routes_from_chunks(Site *site);
void url_decode(const char *src, char *dst, size_t dst_size);

/* ── reconstruct.c ─────────────────────────────────────────── */

int  mkdirs(const char *path);
int  download_all_assets(Site *site);
int  try_download_source_maps(Site *site);
int  try_download_build_manifest(Site *site);
int  reconstruct_project(Site *site);
int  generate_package_json(Site *site);
int  generate_next_config(Site *site);
int  generate_report(Site *site);

/* ── beautify.c (now in reconstruct.c) ─────────────────────── */

int   is_minified(const char *code, size_t len);
char *beautify_js(const char *code, size_t len);

/* ── main.c ────────────────────────────────────────────────── */

int  decompile_site(const char *url);

#endif /* CCDES_H */
