/* platform/assets.h - zero-copy access to stone_pack.stpk.
 *
 * The pack holds every raster asset STONE draws: the launch emblem, the three
 * onboarding illustrations, the per-tab header band atlas and the achievement
 * badge sheet. Each texture carries a full mip chain so the renderer can
 * upload the level that matches the device instead of a 1024 px master.
 *
 * It is declared noCompress in app/build.gradle, so it sits in the APK
 * uncompressed and page aligned: AAsset_getBuffer() hands back a pointer
 * straight into the mapped APK. Nothing is inflated, nothing is copied, and
 * the pages are evictable by the kernel under memory pressure.
 *
 * Layout, all little endian:
 *
 *   header   char magic[4] = "STPK"
 *            u32  version  = 1
 *            u32  entry_count
 *            u32  data_offset
 *   table    entry_count x {
 *                char name[32];          NUL padded
 *                u32  width, height;     level 0
 *                u32  mip_count;
 *                u32  format;            0 = RGBA8
 *                u64  offset, size;      absolute into the pack
 *            }
 *   data     mip levels back to back, level 0 first, tightly packed RGBA8
 *
 * Created by sobing4413 - Exter Interactive.
 */
#ifndef STONE_PLATFORM_ASSETS_H
#define STONE_PLATFORM_ASSETS_H

#include <stddef.h>

#define STONE_PACK_FILE "stone_pack.stpk"

typedef struct {
    int width, height;     /* level 0 */
    int mip_count;
    int format;            /* 0 = RGBA8 */
    const unsigned char *data;
    size_t size;
} StoneAssetImage;

struct AAssetManager;

/* Maps the pack. Safe to call more than once; returns 1 when it is usable. */
int  stone_assets_init(struct AAssetManager *mgr);
void stone_assets_shutdown(void);
int  stone_assets_ready(void);

/* Looks a texture up by name. Returns 0 when the pack is missing or the name
   is not in it - every caller is expected to degrade gracefully, because the
   app has to keep working if the asset pack was stripped from the APK. */
int  stone_assets_image(const char *name, StoneAssetImage *out);

/* Pixels and dimensions of one mip level of an image already looked up. */
const unsigned char *stone_assets_mip(const StoneAssetImage *img, int level,
                                      int *out_w, int *out_h);

#endif /* STONE_PLATFORM_ASSETS_H */
