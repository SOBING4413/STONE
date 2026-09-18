/* platform/assets.c - see assets.h. */
#include "assets.h"

#include <string.h>

#include <android/asset_manager.h>
#include <android/log.h>

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  "STONE", __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN,  "STONE", __VA_ARGS__)

#define NAME_LEN     32
#define ENTRY_BYTES  (NAME_LEN + 4 * 4 + 8 + 8)   /* 64 */
#define HEADER_BYTES 16

static AAsset             *g_asset;
static const unsigned char *g_base;
static size_t              g_size;
static int                 g_count;
static const unsigned char *g_table;
static int                 g_ready;

/* The pack is generated on a little-endian host and every ABI STONE ships for
   is little-endian too, but reading byte by byte costs nothing and removes
   the assumption entirely. */
static unsigned int rd_u32(const unsigned char *p)
{
    return (unsigned int)p[0]
         | ((unsigned int)p[1] << 8)
         | ((unsigned int)p[2] << 16)
         | ((unsigned int)p[3] << 24);
}

static unsigned long long rd_u64(const unsigned char *p)
{
    return (unsigned long long)rd_u32(p)
         | ((unsigned long long)rd_u32(p + 4) << 32);
}

int stone_assets_init(struct AAssetManager *mgr)
{
    const unsigned char *h;
    unsigned int version, count, data_offset;
    off_t len;

    if (g_ready) return 1;
    if (!mgr) return 0;

    g_asset = AAssetManager_open((AAssetManager *)mgr, STONE_PACK_FILE,
                                 AASSET_MODE_BUFFER);
    if (!g_asset) {
        LOGW("assets: %s not found, running without artwork", STONE_PACK_FILE);
        return 0;
    }

    len   = AAsset_getLength(g_asset);
    g_base = (const unsigned char *)AAsset_getBuffer(g_asset);
    if (!g_base || len < (off_t)HEADER_BYTES) {
        LOGW("assets: %s is unreadable", STONE_PACK_FILE);
        AAsset_close(g_asset);
        g_asset = NULL;
        return 0;
    }
    g_size = (size_t)len;

    h = g_base;
    if (memcmp(h, "STPK", 4) != 0) {
        LOGW("assets: bad magic in %s", STONE_PACK_FILE);
        AAsset_close(g_asset);
        g_asset = NULL;
        return 0;
    }
    version     = rd_u32(h + 4);
    count       = rd_u32(h + 8);
    data_offset = rd_u32(h + 12);

    if (version != 1 || count == 0 || count > 256 ||
        (size_t)data_offset > g_size ||
        (size_t)data_offset < HEADER_BYTES + (size_t)count * ENTRY_BYTES) {
        LOGW("assets: %s header is out of range", STONE_PACK_FILE);
        AAsset_close(g_asset);
        g_asset = NULL;
        return 0;
    }

    g_table = g_base + HEADER_BYTES;
    g_count = (int)count;
    g_ready = 1;
    LOGI("assets: %s mapped, %d textures, %.2f MB",
         STONE_PACK_FILE, g_count, (double)g_size / (1024.0 * 1024.0));
    return 1;
}

void stone_assets_shutdown(void)
{
    if (g_asset) AAsset_close(g_asset);
    g_asset = NULL;
    g_base  = NULL;
    g_table = NULL;
    g_size  = 0;
    g_count = 0;
    g_ready = 0;
}

int stone_assets_ready(void) { return g_ready; }

int stone_assets_image(const char *name, StoneAssetImage *out)
{
    int i;

    if (!g_ready || !name || !out) return 0;

    for (i = 0; i < g_count; ++i) {
        const unsigned char *e = g_table + (size_t)i * ENTRY_BYTES;
        unsigned int w, h, mips, fmt;
        unsigned long long off, size;

        if (strncmp((const char *)e, name, NAME_LEN) != 0) continue;

        w    = rd_u32(e + NAME_LEN);
        h    = rd_u32(e + NAME_LEN + 4);
        mips = rd_u32(e + NAME_LEN + 8);
        fmt  = rd_u32(e + NAME_LEN + 12);
        off  = rd_u64(e + NAME_LEN + 16);
        size = rd_u64(e + NAME_LEN + 24);

        /* Everything below is a bounds check against the mapped file: a
           truncated or tampered APK must fail the lookup, never read past
           the mapping. */
        if (w == 0 || h == 0 || w > 8192 || h > 8192) return 0;
        if (mips == 0 || mips > 16) return 0;
        if (fmt != 0) return 0;
        if (off > g_size || size > g_size || off + size > g_size) return 0;
        if (size < (unsigned long long)w * h * 4u) return 0;

        out->width     = (int)w;
        out->height    = (int)h;
        out->mip_count = (int)mips;
        out->format    = (int)fmt;
        out->data      = g_base + off;
        out->size      = (size_t)size;
        return 1;
    }
    return 0;
}

const unsigned char *stone_assets_mip(const StoneAssetImage *img, int level,
                                      int *out_w, int *out_h)
{
    size_t skip = 0;
    int w, h, l;

    if (!img || !img->data) return NULL;
    if (level < 0) level = 0;
    if (level >= img->mip_count) level = img->mip_count - 1;

    w = img->width;
    h = img->height;
    for (l = 0; l < level; ++l) {
        skip += (size_t)w * (size_t)h * 4u;
        w = w > 1 ? w / 2 : 1;
        h = h > 1 ? h / 2 : 1;
    }
    if (skip + (size_t)w * (size_t)h * 4u > img->size) return NULL;

    if (out_w) *out_w = w;
    if (out_h) *out_h = h;
    return img->data + skip;
}
