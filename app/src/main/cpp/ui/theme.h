/* theme.h - colour palette. Dark is the default look; light is a toggle. */
#ifndef STONE_THEME_H
#define STONE_THEME_H

#include "../renderer/render.h"

typedef struct {
    Color bg;            /* window background            */
    Color surface;       /* cards                        */
    Color surface_alt;   /* nested panels, inputs        */
    Color border;
    Color primary;       /* brand / progress             */
    Color primary_soft;
    Color accent;        /* calories, highlights         */
    Color warn;
    Color danger;
    Color text;
    Color text_dim;
    Color text_faint;
    Color nav_bg;
    Color shadow;
    Color overlay;
} StoneTheme;

/* Resolved once per frame from settings.dark_theme. */
const StoneTheme *theme(void);
void theme_refresh(int dark);

#endif /* STONE_THEME_H */
