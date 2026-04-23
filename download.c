/*
 * download.c - HTTP downloading with libcurl
 * Handles buffer management, gzip, cookies, connection reuse
 */
#include "ccdes.h"

/* ── Buffer helpers ────────────────────────────────────────── */

void buffer_init(Buffer *b) {
    b->data = NULL;
    b->size = 0;
}

void buffer_free(Buffer *b) {
    free(b->data);
    b->data = NULL;
    b->size = 0;
}

/* ── curl write callback ──────────────────────────────────── */

static size_t write_buffer_cb(void *contents, size_t size, size_t nmemb,
                              void *userp)
{
    size_t realsize = size * nmemb;
    Buffer *buf = (Buffer *)userp;

    char *ptr = realloc(buf->data, buf->size + realsize + 1);
    if (!ptr) {
        fprintf(stderr, "[!] Out of memory (need %zu bytes)\n",
                buf->size + realsize + 1);
        return 0;
    }
    buf->data = ptr;
    memcpy(buf->data + buf->size, contents, realsize);
    buf->size += realsize;
    buf->data[buf->size] = '\0';
    return realsize;
}

/* ── curl write-to-file callback ──────────────────────────── */

static size_t write_file_cb(void *contents, size_t size, size_t nmemb,
                            void *userp)
{
    return fwrite(contents, size, nmemb, (FILE *)userp);
}

/* ── Global init / cleanup ────────────────────────────────── */

void download_global_init(void) {
    curl_global_init(CURL_GLOBAL_ALL);
}

void download_global_cleanup(void) {
    curl_global_cleanup();
}

/* ── Configure a curl handle with common options ──────────── */

static void configure_curl(CURL *curl, const char *url, char *errbuf) {
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 10L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, DL_TIMEOUT);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, DL_CONNECT_TO);

    /* Let libcurl handle gzip / brotli / deflate automatically */
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");

    /* Realistic browser headers */
    curl_easy_setopt(curl, CURLOPT_USERAGENT,
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) "
        "AppleWebKit/537.36 (KHTML, like Gecko) "
        "Chrome/134.0.0.0 Safari/537.36");

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers,
        "Accept: text/html,application/xhtml+xml,application/xml;"
        "q=0.9,image/webp,*/*;q=0.8");
    headers = curl_slist_append(headers,
        "Accept-Language: en-US,en;q=0.9,es;q=0.8");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    /* Note: headers leaked – acceptable for a CLI tool */

    /* Accept cookies from server (in-memory jar) */
    curl_easy_setopt(curl, CURLOPT_COOKIEFILE, "");

    /* TLS verification */
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
}

/* ── Download URL contents into a Buffer ──────────────────── */

int download_to_buffer(const char *url, Buffer *buf) {
    CURL *curl = curl_easy_init();
    if (!curl) {
        fprintf(stderr, "[!] curl_easy_init failed\n");
        return -1;
    }

    char errbuf[CURL_ERROR_SIZE] = {0};
    configure_curl(curl, url, errbuf);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_buffer_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, buf);

    CURLcode res = curl_easy_perform(curl);

    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        fprintf(stderr, "[!] Download failed: %s\n", curl_easy_strerror(res));
        if (errbuf[0]) fprintf(stderr, "    Detail: %s\n", errbuf);
        return -1;
    }

    if (http_code >= 400) {
        fprintf(stderr, "[!] HTTP %ld for %s\n", http_code, url);
        return (int)http_code;
    }

    return 0;
}

/* ── Download URL and save directly to a file ─────────────── */

int download_to_file(const char *url, const char *filepath) {
    FILE *fp = fopen(filepath, "wb");
    if (!fp) {
        fprintf(stderr, "[!] Cannot create %s: %s\n", filepath, strerror(errno));
        return -1;
    }

    CURL *curl = curl_easy_init();
    if (!curl) {
        fclose(fp);
        return -1;
    }

    char errbuf[CURL_ERROR_SIZE] = {0};
    configure_curl(curl, url, errbuf);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_file_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);

    CURLcode res = curl_easy_perform(curl);

    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    curl_easy_cleanup(curl);
    fclose(fp);

    if (res != CURLE_OK || http_code >= 400) {
        remove(filepath);   /* clean up partial file */
        return -1;
    }

    return 0;
}
