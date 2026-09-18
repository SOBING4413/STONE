/* json.h - dependency-free JSON parser/writer for STONE.
 *
 * Design notes:
 *  - Strict enough to reject malformed files, tolerant enough never to crash.
 *  - Hard limits on nesting depth and document size so a corrupted file cannot
 *    exhaust the stack or the heap.
 *  - All getters accept NULL and return safe defaults, so page code can read
 *    optional fields without defensive branches everywhere.
 */
#ifndef STONE_JSON_H
#define STONE_JSON_H

#include "../utils/strbuf.h"

#define JSON_MAX_DEPTH 32

typedef enum {
    JSON_NULL = 0,
    JSON_BOOL,
    JSON_NUMBER,
    JSON_STRING,
    JSON_ARRAY,
    JSON_OBJECT
} JsonType;

typedef struct JsonValue JsonValue;

struct JsonValue {
    JsonType    type;
    double      number;     /* JSON_NUMBER, JSON_BOOL (0/1)            */
    char       *string;     /* JSON_STRING (owned)                     */
    JsonValue **items;      /* JSON_ARRAY / JSON_OBJECT children       */
    char      **keys;       /* JSON_OBJECT keys (owned), parallel array */
    int         count;
    int         cap;
};

/* --- parsing / lifetime --------------------------------------------------- */
JsonValue *json_parse(const char *text);   /* NULL on any syntax error */
void       json_free(JsonValue *v);

/* --- construction --------------------------------------------------------- */
JsonValue *json_new_object(void);
JsonValue *json_new_array(void);
JsonValue *json_new_string(const char *s);
JsonValue *json_new_number(double n);
JsonValue *json_new_bool(int b);
int        json_object_set(JsonValue *obj, const char *key, JsonValue *val); /* takes ownership */
int        json_array_push(JsonValue *arr, JsonValue *val);                  /* takes ownership */

/* Convenience setters (silently no-op when obj is NULL or not an object). */
void json_set_str(JsonValue *obj, const char *key, const char *s);
void json_set_num(JsonValue *obj, const char *key, double n);
void json_set_int(JsonValue *obj, const char *key, int n);
void json_set_bool(JsonValue *obj, const char *key, int b);

/* --- reading -------------------------------------------------------------- */
const JsonValue *json_get(const JsonValue *obj, const char *key);
const JsonValue *json_at(const JsonValue *arr, int index);
int          json_count(const JsonValue *v);
const char  *json_str(const JsonValue *obj, const char *key, const char *fallback);
double       json_num(const JsonValue *obj, const char *key, double fallback);
int          json_int(const JsonValue *obj, const char *key, int fallback);
int          json_bool(const JsonValue *obj, const char *key, int fallback);

/* --- writing -------------------------------------------------------------- */
void  json_write(const JsonValue *v, StrBuf *out, int pretty);
char *json_to_string(const JsonValue *v, int pretty); /* caller frees */

#endif /* STONE_JSON_H */
