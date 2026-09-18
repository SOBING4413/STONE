#include "chart.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHART_GRID_LINES 4

static void series_range(const float *v, int n, float *lo, float *hi)
{
    int i;
    float mn = v[0], mx = v[0];
    for (i = 1; i < n; ++i) {
        if (v[i] < mn) mn = v[i];
        if (v[i] > mx) mx = v[i];
    }
    if (mx - mn < 0.0001f) { mx = mn + 1.0f; mn -= 1.0f; }
    /* 8 % headroom on both ends so the line never touches the frame */
    {
        float pad = (mx - mn) * 0.08f;
        *lo = mn - pad;
        *hi = mx + pad;
    }
}

static void draw_grid(Rect plot, float lo, float hi, const ChartStyle *s)
{
    int i;
    for (i = 0; i <= CHART_GRID_LINES; ++i) {
        float t = (float)i / (float)CHART_GRID_LINES;
        float y = plot.y + plot.h * t;
        char label[24];
        float value = hi - (hi - lo) * t;

        render_rect(rect_make(plot.x, y, plot.w, 1.0f), s->grid, 0.0f);
        if (fabsf(value) >= 100.0f) snprintf(label, sizeof(label), "%.0f", value);
        else snprintf(label, sizeof(label), "%.1f", value);
        render_text(label, plot.x + plot.w + 6.0f, y - s->label_size * 0.6f,
                    s->label_size, s->label, 0);
    }
}

static void draw_x_labels(Rect plot, int n, const char *const *labels,
                          const ChartStyle *s)
{
    int i, step;
    if (!labels || n <= 0) return;

    step = n > 7 ? (n + 6) / 7 : 1;
    for (i = 0; i < n; i += step) {
        float x = (n == 1) ? plot.x + plot.w * 0.5f
                           : plot.x + plot.w * ((float)i / (float)(n - 1));
        float w = render_text_width(labels[i], s->label_size, 0);
        render_text(labels[i], x - w * 0.5f, plot.y + plot.h + 8.0f,
                    s->label_size, s->label, 0);
    }
}

void chart_line(Rect area, const float *values, int n,
                const char *const *x_labels, const ChartStyle *s)
{
    Rect plot;
    float lo, hi, px = 0.0f, py = 0.0f;
    int i;

    if (!values || n <= 0 || !s) return;

    plot = area;
    plot.w -= 44.0f;                 /* room for the value axis */
    plot.h -= 22.0f;                 /* room for the date axis  */
    if (plot.w < 40.0f || plot.h < 30.0f) return;

    series_range(values, n, &lo, &hi);
    draw_grid(plot, lo, hi, s);

    if (s->target > 0.0f && s->target >= lo && s->target <= hi) {
        float ty = plot.y + plot.h * (1.0f - (s->target - lo) / (hi - lo));
        int seg;
        for (seg = 0; seg * 14.0f < plot.w; ++seg) {
            float x0 = plot.x + (float)seg * 14.0f;
            float x1 = x0 + 8.0f;
            if (x1 > plot.x + plot.w) x1 = plot.x + plot.w;
            render_rect(rect_make(x0, ty, x1 - x0, 1.5f), s->label, 0.0f);
        }
    }

    /* Area fill: one thin column per sample keeps this allocation free. */
    if (s->fill_top.a > 0.0f && n > 1) {
        int steps = (int)plot.w;
        if (steps > 400) steps = 400;
        for (i = 0; i < steps; ++i) {
            float t = (float)i / (float)(steps - 1);
            float fi = t * (float)(n - 1);
            int i0 = (int)fi;
            int i1 = i0 + 1 < n ? i0 + 1 : i0;
            float f = fi - (float)i0;
            float v = values[i0] + (values[i1] - values[i0]) * f;
            float y = plot.y + plot.h * (1.0f - (v - lo) / (hi - lo));
            float x = plot.x + plot.w * t;
            render_rect_gradient(rect_make(x, y, plot.w / (float)steps + 1.0f,
                                           plot.y + plot.h - y),
                                 s->fill_top, color_alpha(s->fill_top, 0.0f), 0.0f);
        }
    }

    for (i = 0; i < n; ++i) {
        float x = (n == 1) ? plot.x + plot.w * 0.5f
                           : plot.x + plot.w * ((float)i / (float)(n - 1));
        float y = plot.y + plot.h * (1.0f - (values[i] - lo) / (hi - lo));
        if (i > 0) render_line(px, py, x, y, 2.5f, s->line);
        px = x;
        py = y;
    }

    if (s->show_points) {
        for (i = 0; i < n; ++i) {
            float x = (n == 1) ? plot.x + plot.w * 0.5f
                               : plot.x + plot.w * ((float)i / (float)(n - 1));
            float y = plot.y + plot.h * (1.0f - (values[i] - lo) / (hi - lo));
            render_circle(x, y, 3.5f, s->line);
        }
    }

    draw_x_labels(plot, n, x_labels, s);
}

void chart_bars(Rect area, const float *values, int n,
                const char *const *x_labels, const ChartStyle *s)
{
    Rect plot;
    float hi = 0.0f, gap, bw;
    int i;

    if (!values || n <= 0 || !s) return;

    plot = area;
    plot.w -= 44.0f;
    plot.h -= 22.0f;
    if (plot.w < 40.0f || plot.h < 30.0f) return;

    for (i = 0; i < n; ++i) if (values[i] > hi) hi = values[i];
    if (hi <= 0.0f) hi = 1.0f;
    hi *= 1.15f;

    draw_grid(plot, 0.0f, hi, s);

    gap = plot.w / (float)n * 0.28f;
    bw = plot.w / (float)n - gap;
    if (bw < 4.0f) bw = 4.0f;

    for (i = 0; i < n; ++i) {
        float h = plot.h * (values[i] / hi);
        float x = plot.x + (bw + gap) * (float)i + gap * 0.5f;
        Rect bar = rect_make(x, plot.y + plot.h - h, bw, h);
        if (h < 2.0f) {
            render_rect(rect_make(x, plot.y + plot.h - 2.0f, bw, 2.0f),
                        color_alpha(s->line, 0.35f), 1.0f);
            continue;
        }
        render_rect_gradient(bar, s->line, color_alpha(s->line, 0.55f), 6.0f);
    }

    if (x_labels) {
        for (i = 0; i < n; ++i) {
            float x = plot.x + (bw + gap) * (float)i + gap * 0.5f + bw * 0.5f;
            float w = render_text_width(x_labels[i], s->label_size, 0);
            render_text(x_labels[i], x - w * 0.5f, plot.y + plot.h + 8.0f,
                        s->label_size, s->label, 0);
        }
    }
}
