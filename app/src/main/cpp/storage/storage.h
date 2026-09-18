/* storage.h - local-only file storage (no network, no server, no cloud).
 *
 * Files live in the app's private internal directory, which Android hands to
 * the native activity as `internalDataPath`. Writes are atomic: data goes to
 * "<name>.tmp", is flushed with fsync(), and only then renamed over the real
 * file, so a kill during save can never leave a half-written JSON behind.
 */
#ifndef STONE_STORAGE_H
#define STONE_STORAGE_H

#include "json.h"

#define STONE_MAX_FILE_BYTES (8 * 1024 * 1024) /* refuse absurd/corrupt files */

typedef enum {
    STORAGE_OK = 0,
    STORAGE_ERR_NO_PATH,
    STORAGE_ERR_NOT_FOUND,
    STORAGE_ERR_TOO_BIG,
    STORAGE_ERR_IO,
    STORAGE_ERR_PARSE,
    STORAGE_ERR_MEMORY
} StorageResult;

/* base_dir is usually app->activity->internalDataPath.
   external_dir (may be NULL) is used for user-visible backup files. */
void        storage_init(const char *base_dir, const char *external_dir);
const char *storage_base_dir(void);
const char *storage_external_dir(void);
int         storage_ready(void);
const char *storage_error_text(StorageResult r);

/* Raw text helpers. Returned buffer is malloc'd and NUL-terminated. */
char         *storage_read_text(const char *filename, StorageResult *out_err);
StorageResult storage_write_text(const char *filename, const char *text);
int           storage_file_exists(const char *filename);
StorageResult storage_delete(const char *filename);

/* JSON helpers. A corrupted file is moved aside to "<name>.corrupt" so the
   user never silently loses data and the app can fall back to defaults. */
JsonValue    *storage_read_json(const char *filename, StorageResult *out_err);
StorageResult storage_write_json(const char *filename, const JsonValue *root);

/* Backup / restore: a single JSON bundle holding every data file. */
StorageResult storage_export_bundle(const char *absolute_or_name, const JsonValue *bundle);
JsonValue    *storage_import_bundle(const char *absolute_or_name, StorageResult *out_err);

#endif /* STONE_STORAGE_H */
