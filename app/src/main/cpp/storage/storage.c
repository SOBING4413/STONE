/* localtime_r(), fsync(), fileno() */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "storage.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../core/model.h"

#ifdef __ANDROID__
#include <android/log.h>
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, "STONE", __VA_ARGS__)
#else
#define LOGW(...) ((void)0)
#endif

static char g_base[512];
static char g_external[512];

void storage_init(const char *base_dir, const char *external_dir)
{
    g_base[0] = '\0';
    g_external[0] = '\0';
    if (base_dir && base_dir[0]) {
        stone_strlcpy(g_base, base_dir, sizeof(g_base));
        /* Best effort: the directory normally already exists. */
        mkdir(g_base, 0770);
    }
    if (external_dir && external_dir[0]) {
        stone_strlcpy(g_external, external_dir, sizeof(g_external));
        mkdir(g_external, 0770);
    }
}

const char *storage_base_dir(void)     { return g_base; }
const char *storage_external_dir(void) { return g_external[0] ? g_external : g_base; }
int         storage_ready(void)        { return g_base[0] != '\0'; }

const char *storage_error_text(StorageResult r)
{
    switch (r) {
    case STORAGE_OK:            return "OK";
    case STORAGE_ERR_NO_PATH:   return "Storage not available";
    case STORAGE_ERR_NOT_FOUND: return "File not found";
    case STORAGE_ERR_TOO_BIG:   return "File too large";
    case STORAGE_ERR_IO:        return "Read/write error";
    case STORAGE_ERR_PARSE:     return "Invalid JSON";
    case STORAGE_ERR_MEMORY:    return "Out of memory";
    default:                    return "Unknown error";
    }
}

/* Builds an absolute path. A name containing '/' is treated as already
   absolute (used by backup files). '..' is rejected. */
static int build_path(char *out, size_t n, const char *name)
{
    if (!name || !name[0]) return 0;
    if (strstr(name, "..")) return 0;
    if (name[0] == '/') { stone_strlcpy(out, name, n); return 1; }
    if (!g_base[0]) return 0;
    snprintf(out, n, "%s/%s", g_base, name);
    return 1;
}

char *storage_read_text(const char *filename, StorageResult *err)
{
    char path[640];
    FILE *f;
    long size;
    char *buf;
    size_t got;

    if (err) *err = STORAGE_OK;
    if (!build_path(path, sizeof(path), filename)) {
        if (err) *err = STORAGE_ERR_NO_PATH;
        return NULL;
    }

    f = fopen(path, "rb");
    if (!f) { if (err) *err = STORAGE_ERR_NOT_FOUND; return NULL; }

    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); if (err) *err = STORAGE_ERR_IO; return NULL; }
    size = ftell(f);
    if (size < 0) { fclose(f); if (err) *err = STORAGE_ERR_IO; return NULL; }
    if (size > STONE_MAX_FILE_BYTES) { fclose(f); if (err) *err = STORAGE_ERR_TOO_BIG; return NULL; }
    rewind(f);

    buf = (char *)malloc((size_t)size + 1);
    if (!buf) { fclose(f); if (err) *err = STORAGE_ERR_MEMORY; return NULL; }

    got = fread(buf, 1, (size_t)size, f);
    fclose(f);
    buf[got] = '\0';
    return buf;
}

StorageResult storage_write_text(const char *filename, const char *text)
{
    char path[640], tmp[704];
    FILE *f;
    size_t len;

    if (!text) return STORAGE_ERR_IO;
    if (!build_path(path, sizeof(path), filename)) return STORAGE_ERR_NO_PATH;
    snprintf(tmp, sizeof(tmp), "%s.tmp", path);

    f = fopen(tmp, "wb");
    if (!f) { LOGW("open failed: %s (%s)", tmp, strerror(errno)); return STORAGE_ERR_IO; }

    len = strlen(text);
    if (len && fwrite(text, 1, len, f) != len) { fclose(f); remove(tmp); return STORAGE_ERR_IO; }
    if (fflush(f) != 0) { fclose(f); remove(tmp); return STORAGE_ERR_IO; }
    fsync(fileno(f));            /* durability before the rename */
    if (fclose(f) != 0) { remove(tmp); return STORAGE_ERR_IO; }

    if (rename(tmp, path) != 0) { LOGW("rename failed: %s", strerror(errno)); remove(tmp); return STORAGE_ERR_IO; }
    return STORAGE_OK;
}

int storage_file_exists(const char *filename)
{
    char path[640];
    struct stat st;
    if (!build_path(path, sizeof(path), filename)) return 0;
    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

StorageResult storage_delete(const char *filename)
{
    char path[640];
    if (!build_path(path, sizeof(path), filename)) return STORAGE_ERR_NO_PATH;
    return remove(path) == 0 ? STORAGE_OK : STORAGE_ERR_IO;
}

static void quarantine(const char *filename)
{
    char path[640], bad[704];
    if (!build_path(path, sizeof(path), filename)) return;
    snprintf(bad, sizeof(bad), "%s.corrupt", path);
    remove(bad);
    if (rename(path, bad) == 0) LOGW("quarantined corrupt file: %s", filename);
}

JsonValue *storage_read_json(const char *filename, StorageResult *err)
{
    StorageResult e = STORAGE_OK;
    char *text;
    JsonValue *root;

    text = storage_read_text(filename, &e);
    if (!text) { if (err) *err = e; return NULL; }

    root = json_parse(text);
    free(text);

    if (!root) {
        quarantine(filename);
        if (err) *err = STORAGE_ERR_PARSE;
        return NULL;
    }
    if (err) *err = STORAGE_OK;
    return root;
}

StorageResult storage_write_json(const char *filename, const JsonValue *root)
{
    char *text;
    StorageResult r;

    text = json_to_string(root, 1);
    if (!text) return STORAGE_ERR_MEMORY;
    r = storage_write_text(filename, text);
    free(text);
    return r;
}

StorageResult storage_export_bundle(const char *name, const JsonValue *bundle)
{
    char path[640];
    if (name && name[0] == '/') return storage_write_json(name, bundle);
    snprintf(path, sizeof(path), "%s/%s", storage_external_dir(),
             (name && name[0]) ? name : "stone_backup.json");
    return storage_write_json(path, bundle);
}

JsonValue *storage_import_bundle(const char *name, StorageResult *err)
{
    char path[640];
    if (name && name[0] == '/') return storage_read_json(name, err);
    snprintf(path, sizeof(path), "%s/%s", storage_external_dir(),
             (name && name[0]) ? name : "stone_backup.json");
    return storage_read_json(path, err);
}
