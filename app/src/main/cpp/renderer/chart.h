/* chart.h - data plots drawn straight through the GL renderer.
 * No chart library, no images: axes, grid, area fill, line and bars are all
 * rounded quads produced by render.c.
 */
#ifndef STONE_CHART_H
#define STONE_CHART_H

#include "render.h"

typedef struct {
    Color       line;
    Color       grid;
    Color       label;
    Color       fill_top;      /* area under the line (alpha may be 0) */
    const char *unit;
    float       target;        /* horizontal marker; <= 0 disables it  */
    int         show_points;
    float       label_size;
} ChartStyle;

/* values[0..n-1] plotted left to right. x_labels may be NULL. */
void chart_line(Rect area, const float *values, int n,
                const char *const *x_labels, const ChartStyle *style);

void chart_bars(Rect area, const float *values, int n,
                const char *const *x_labels, const ChartStyle *style);

#endif /* STONE_CHART_H */
