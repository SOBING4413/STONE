#include "json.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ alloc */

static JsonValue *json_new(JsonType t)
{
    JsonValue *v = (JsonValue *)calloc(1, sizeof(JsonValue));
    if (v) v->type = t;
    return v;
}

JsonValue *json_new_object(void) { return json_new(JSON_OBJECT); }
JsonValue *json_new_array(void)  { return json_new(JSON_ARRAY); }

JsonValue *json_new_number(double n)
{
    JsonValue *v = json_new(JSON_NUMBER);
    if (v) v->number = n;
    return v;
}

JsonValue *json_new_bool(int b)
{
    JsonValue *v = json_new(JSON_BOOL);
    if (v) v->number = b ? 1.0 : 0.0;
    return v;
}

JsonValue *json_new_string(const char *s)
{
    JsonValue *v = json_new(JSON_STRING);
    if (!v) return NULL;
    if (!s) s = "";
    v->string = (char *)malloc(strlen(s) + 1);
    if (!v->string) { free(v); return NULL; }
    memcpy(v->string, s, strlen(s) + 1);
    return v;
}

void json_free(JsonValue *v)
{
    int i;
    if (!v) return;
    for (i = 0; i < v->count; ++i) {
        if (v->items) json_free(v->items[i]);
        if (v->keys) free(v->keys[i]);
    }
    free(v->items);
    free(v->keys);
    free(v->string);
    free(v);
}

static int json_grow(JsonValue *v, int want_keys)
{
    int ncap;
    JsonValue **ni;
    char **nk;

    if (v->count < v->cap) return 1;
    ncap = v->cap ? v->cap * 2 : 8;
    if (ncap > 1 << 20) return 0;

    ni = (JsonValue **)realloc(v->items, (size_t)ncap * sizeof(JsonValue *));
    if (!ni) return 0;
    v->items = ni;

    if (want_keys) {
        nk = (char **)realloc(v->keys, (size_t)ncap * sizeof(char *));
        if (!nk) return 0;
        v->keys = nk;
    }
    v->cap = ncap;
    return 1;
}

int json_array_push(JsonValue *arr, JsonValue *val)
{
    if (!arr || arr->type != JSON_ARRAY || !val) { json_free(val); return 0; }
    if (!json_grow(arr, 0)) { json_free(val); return 0; }
    arr->items[arr->count++] = val;
    return 1;
}

int json_object_set(JsonValue *obj, const char *key, JsonValue *val)
{
    char *k;
    size_t n;

    if (!obj || obj->type != JSON_OBJECT || !key || !val) { json_free(val); return 0; }
    if (!json_grow(obj, 1)) { json_free(val); return 0; }

    n = strlen(key) + 1;
    k = (char *)malloc(n);
    if (!k) { json_free(val); return 0; }
    memcpy(k, key, n);

    obj->keys[obj->count] = k;
    obj->items[obj->count] = val;
    obj->count++;
    return 1;
}

void json_set_str(JsonValue *o, const char *k, const char *s) { json_object_set(o, k, json_new_string(s)); }
void json_set_num(JsonValue *o, const char *k, double n)      { json_object_set(o, k, json_new_number(n)); }
void json_set_int(JsonValue *o, const char *k, int n)         { json_object_set(o, k, json_new_number((double)n)); }
void json_set_bool(JsonValue *o, const char *k, int b)        { json_object_set(o, k, json_new_bool(b)); }

/* ----------------------------------------------------------------- reading */

const JsonValue *json_get(const JsonValue *obj, const char *key)
{
    int i;
    if (!obj || obj->type != JSON_OBJECT || !key || !obj->keys) return NULL;
    for (i = 0; i < obj->count; ++i)
        if (obj->keys[i] && strcmp(obj->keys[i], key) == 0) return obj->items[i];
    return NULL;
}

const JsonValue *json_at(const JsonValue *arr, int index)
{
    if (!arr || (arr->type != JSON_ARRAY && arr->type != JSON_OBJECT)) return NULL;
    if (index < 0 || index >= arr->count) return NULL;
    return arr->items[index];
}

int json_count(const JsonValue *v) { return v ? v->count : 0; }

const char *json_str(const JsonValue *obj, const char *key, const char *fallback)
{
    const JsonValue *v = json_get(obj, key);
    return (v && v->type == JSON_STRING && v->string) ? v->string : fallback;
}

double json_num(const JsonValue *obj, const char *key, double fallback)
{
    const JsonValue *v = json_get(obj, key);
    if (!v) return fallback;
    if (v->type == JSON_NUMBER || v->type == JSON_BOOL) {
        if (v->number != v->number) return fallback; /* NaN guard */
        return v->number;
    }
    return fallback;
}

int json_int(const JsonValue *obj, const char *key, int fallback)
{
    double d = json_num(obj, key, (double)fallback);
    if (d > 2147483000.0) return 2147483000;
    if (d < -2147483000.0) return -2147483000;
    return (int)(d < 0 ? d - 0.5 : d + 0.5);
}

int json_bool(const JsonValue *obj, const char *key, int fallback)
{
    const JsonValue *v = json_get(obj, key);
    if (!v) return fallback;
    if (v->type == JSON_BOOL || v->type == JSON_NUMBER) return v->number != 0.0;
    return fallback;
}

/* ----------------------------------------------------------------- parsing */

typedef struct {
    const char *p;
    int         depth;
    int         error;
} JParser;

static void jp_ws(JParser *j)
{
    while (*j->p == ' ' || *j->p == '\t' || *j->p == '\n' || *j->p == '\r') j->p++;
}

static JsonValue *jp_value(JParser *j);

static int jp_hex4(const char *p, unsigned *out)
{
    int i;
    unsigned v = 0;
    for (i = 0; i < 4; ++i) {
        char c = p[i];
        v <<= 4;
        if (c >= '0' && c <= '9') v |= (unsigned)(c - '0');
        else if (c >= 'a' && c <= 'f') v |= (unsigned)(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') v |= (unsigned)(c - 'A' + 10);
        else return 0;
    }
    *out = v;
    return 1;
}

static void sb_put_utf8(StrBuf *sb, unsigned cp)
{
    if (cp < 0x80) {
        sb_putc(sb, (char)cp);
    } else if (cp < 0x800) {
        sb_putc(sb, (char)(0xC0 | (cp >> 6)));
        sb_putc(sb, (char)(0x80 | (cp & 0x3F)));
    } else if (cp < 0x10000) {
        sb_putc(sb, (char)(0xE0 | (cp >> 12)));
        sb_putc(sb, (char)(0x80 | ((cp >> 6) & 0x3F)));
        sb_putc(sb, (char)(0x80 | (cp & 0x3F)));
    } else {
        sb_putc(sb, (char)(0xF0 | (cp >> 18)));
        sb_putc(sb, (char)(0x80 | ((cp >> 12) & 0x3F)));
        sb_putc(sb, (char)(0x80 | ((cp >> 6) & 0x3F)));
        sb_putc(sb, (char)(0x80 | (cp & 0x3F)));
    }
}

/* Parses a quoted string; returns a malloc'd C string or NULL on error. */
static char *jp_string_raw(JParser *j)
{
    StrBuf sb;

    if (*j->p != '"') { j->error = 1; return NULL; }
    j->p++;
    sb_init(&sb);

    while (*j->p && *j->p != '"') {
        unsigned char c = (unsigned char)*j->p;
        if (c == '\\') {
            j->p++;
            switch (*j->p) {
            case '"':  sb_putc(&sb, '"');  j->p++; break;
            case '\\': sb_putc(&sb, '\\'); j->p++; break;
            case '/':  sb_putc(&sb, '/');  j->p++; break;
            case 'b':  sb_putc(&sb, '\b'); j->p++; break;
            case 'f':  sb_putc(&sb, '\f'); j->p++; break;
            case 'n':  sb_putc(&sb, '\n'); j->p++; break;
            case 'r':  sb_putc(&sb, '\r'); j->p++; break;
            case 't':  sb_putc(&sb, '\t'); j->p++; break;
            case 'u': {
                unsigned cp = 0, lo = 0;
                if (!jp_hex4(j->p + 1, &cp)) { j->error = 1; sb_free(&sb); return NULL; }
                j->p += 5;
                if (cp >= 0xD800 && cp <= 0xDBFF && j->p[0] == '\\' && j->p[1] == 'u' &&
                    jp_hex4(j->p + 2, &lo) && lo >= 0xDC00 && lo <= 0xDFFF) {
                    cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                    j->p += 6;
                }
                sb_put_utf8(&sb, cp);
                break;
            }
            default: j->error = 1; sb_free(&sb); return NULL;
            }
        } else if (c < 0x20) {
            j->error = 1; sb_free(&sb); return NULL;
        } else {
            sb_putc(&sb, (char)c);
            j->p++;
        }
        if (sb.oom) { j->error = 1; sb_free(&sb); return NULL; }
    }

    if (*j->p != '"') { j->error = 1; sb_free(&sb); return NULL; }
    j->p++;
    return sb_detach(&sb);
}

static JsonValue *jp_object(JParser *j)
{
    JsonValue *obj = json_new_object();
    if (!obj) { j->error = 1; return NULL; }

    j->p++; /* '{' */
    jp_ws(j);
    if (*j->p == '}') { j->p++; return obj; }

    for (;;) {
        char *key;
        JsonValue *val;

        jp_ws(j);
        key = jp_string_raw(j);
        if (!key) { j->error = 1; json_free(obj); return NULL; }
        jp_ws(j);
        if (*j->p != ':') { free(key); j->error = 1; json_free(obj); return NULL; }
        j->p++;
        jp_ws(j);
        val = jp_value(j);
        if (!val) { free(key); j->error = 1; json_free(obj); return NULL; }

        if (!json_grow(obj, 1)) { free(key); json_free(val); j->error = 1; json_free(obj); return NULL; }
        obj->keys[obj->count] = key;
        obj->items[obj->count] = val;
        obj->count++;

        jp_ws(j);
        if (*j->p == ',') { j->p++; continue; }
        if (*j->p == '}') { j->p++; return obj; }
        j->error = 1;
        json_free(obj);
        return NULL;
    }
}

static JsonValue *jp_array(JParser *j)
{
    JsonValue *arr = json_new_array();
    if (!arr) { j->error = 1; return NULL; }

    j->p++; /* '[' */
    jp_ws(j);
    if (*j->p == ']') { j->p++; return arr; }

    for (;;) {
        JsonValue *val;
        jp_ws(j);
        val = jp_value(j);
        if (!val) { j->error = 1; json_free(arr); return NULL; }
        if (!json_array_push(arr, val)) { j->error = 1; json_free(arr); return NULL; }
        jp_ws(j);
        if (*j->p == ',') { j->p++; continue; }
        if (*j->p == ']') { j->p++; return arr; }
        j->error = 1;
        json_free(arr);
        return NULL;
    }
}

static JsonValue *jp_value(JParser *j)
{
    JsonValue *v = NULL;

    if (j->error) return NULL;
    if (++j->depth > JSON_MAX_DEPTH) { j->error = 1; j->depth--; return NULL; }

    jp_ws(j);
    switch (*j->p) {
    case '{': v = jp_object(j); break;
    case '[': v = jp_array(j); break;
    case '"': {
        char *s = jp_string_raw(j);
        if (s) {
            v = json_new(JSON_STRING);
            if (v) v->string = s; else free(s);
        }
        break;
    }
    case 't':
        if (strncmp(j->p, "true", 4) == 0)  { j->p += 4; v = json_new_bool(1); }
        break;
    case 'f':
        if (strncmp(j->p, "false", 5) == 0) { j->p += 5; v = json_new_bool(0); }
        break;
    case 'n':
        if (strncmp(j->p, "null", 4) == 0)  { j->p += 4; v = json_new(JSON_NULL); }
        break;
    default: {
        char *end = NULL;
        double d;
        if (*j->p != '-' && (*j->p < '0' || *j->p > '9')) break;
        d = strtod(j->p, &end);
        if (end && end != j->p && d == d) { j->p = end; v = json_new_number(d); }
        break;
    }
    }

    j->depth--;
    if (!v) j->error = 1;
    return v;
}

JsonValue *json_parse(const char *text)
{
    JParser j;
    JsonValue *root;

    if (!text) return NULL;
    j.p = text;
    j.depth = 0;
    j.error = 0;

    /* Skip a UTF-8 BOM if a hand-edited backup file carries one. */
    if ((unsigned char)j.p[0] == 0xEF && (unsigned char)j.p[1] == 0xBB &&
        (unsigned char)j.p[2] == 0xBF) j.p += 3;

    root = jp_value(&j);
    if (!root) return NULL;
    jp_ws(&j);
    if (*j.p != '\0') { json_free(root); return NULL; } /* trailing garbage */
    return root;
}

/* ----------------------------------------------------------------- writing */

static void json_write_string(const char *s, StrBuf *out)
{
    sb_putc(out, '"');
    for (; s && *s; ++s) {
        unsigned char c = (unsigned char)*s;
        switch (c) {
        case '"':  sb_puts(out, "\\\""); break;
        case '\\': sb_puts(out, "\\\\"); break;
        case '\n': sb_puts(out, "\\n");  break;
        case '\r': sb_puts(out, "\\r");  break;
        case '\t': sb_puts(out, "\\t");  break;
        case '\b': sb_puts(out, "\\b");  break;
        case '\f': sb_puts(out, "\\f");  break;
        default:
            if (c < 0x20) sb_printf(out, "\\u%04x", c);
            else sb_putc(out, (char)c);
        }
    }
    sb_putc(out, '"');
}

static void json_write_number(double n, StrBuf *out)
{
    char buf[40];
    if (n != n || n > 1e308 || n < -1e308) { sb_puts(out, "0"); return; }
    if (n == (double)(long long)n && n < 1e15 && n > -1e15) {
        snprintf(buf, sizeof(buf), "%lld", (long long)n);
    } else {
        snprintf(buf, sizeof(buf), "%.4f", n);
        /* trim trailing zeros for a tidy file */
        {
            char *dot = strchr(buf, '.');
            if (dot) {
                char *e = buf + strlen(buf) - 1;
                while (e > dot && *e == '0') *e-- = '\0';
                if (e == dot) *e = '\0';
            }
        }
    }
    sb_puts(out, buf);
}

static void json_indent(StrBuf *out, int depth)
{
    int i;
    sb_putc(out, '\n');
    for (i = 0; i < depth; ++i) sb_puts(out, "  ");
}

static void json_write_rec(const JsonValue *v, StrBuf *out, int pretty, int depth)
{
    int i;
    if (!v) { sb_puts(out, "null"); return; }

    switch (v->type) {
    case JSON_NULL:   sb_puts(out, "null"); break;
    case JSON_BOOL:   sb_puts(out, v->number != 0.0 ? "true" : "false"); break;
    case JSON_NUMBER: json_write_number(v->number, out); break;
    case JSON_STRING: json_write_string(v->string, out); break;
    case JSON_ARRAY:
        if (v->count == 0) { sb_puts(out, "[]"); break; }
        sb_putc(out, '[');
        for (i = 0; i < v->count; ++i) {
            if (i) sb_putc(out, ',');
            if (pretty) json_indent(out, depth + 1);
            json_write_rec(v->items[i], out, pretty, depth + 1);
        }
        if (pretty) json_indent(out, depth);
        sb_putc(out, ']');
        break;
    case JSON_OBJECT:
        if (v->count == 0) { sb_puts(out, "{}"); break; }
        sb_putc(out, '{');
        for (i = 0; i < v->count; ++i) {
            if (i) sb_putc(out, ',');
            if (pretty) json_indent(out, depth + 1);
            json_write_string(v->keys ? v->keys[i] : "", out);
            sb_putc(out, ':');
            if (pretty) sb_putc(out, ' ');
            json_write_rec(v->items[i], out, pretty, depth + 1);
        }
        if (pretty) json_indent(out, depth);
        sb_putc(out, '}');
        break;
    }
}

void json_write(const JsonValue *v, StrBuf *out, int pretty)
{
    json_write_rec(v, out, pretty, 0);
}

char *json_to_string(const JsonValue *v, int pretty)
{
    StrBuf sb;
    sb_init(&sb);
    json_write_rec(v, &sb, pretty, 0);
    if (sb.oom) { sb_free(&sb); return NULL; }
    return sb_detach(&sb);
}
