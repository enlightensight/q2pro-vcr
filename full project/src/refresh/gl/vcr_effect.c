/*
 * VCR/CCTV/Found Footage Screen Effect Implementation
 * 
 * Quake II Engine Client-Side Screen Effect
 * Simulates old VCR/CCTV/recorded war footage overlay.
 * 
 * Features:
 *   - Near black-and-white desaturation with sepia tint
 *   - Constant animated white noise dots
 *   - Subtle film grain overlay
 *   - Light scanlines/interlacing
 *   - Timed distortion spikes (~20 second cycle)
 *   - CCTV high-contrast moments with flicker
 *   - VCR tracking lines
 *   - Quality presets (Low/Medium/High)
 *   - Debug commands for QA testing
 *   
 * Found Footage Effects:
 *   - REC blinking indicator
 *   - VHS timestamp overlay
 *   - Random static bursts
 *   - Tape damage / corruption lines
 *   - Night vision mode (green tint)
 *   - Battery indicator
 *   - Chromatic aberration
 * 
 * Technical:
 *   - Uses OpenGL fixed-function pipeline (no shaders required)
 *   - Procedural noise generation (no texture dependencies)
 *   - Minimal memory footprint (stack allocations only)
 *   - Clean loop without memory leaks
 *   - Graceful fallback for very low-end GPUs
 */

#include "vcr_effect.h"

/* Standard includes */
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <time.h>

/*
 * =============================================================================
 *  QUAKE II ENGINE COMPATIBILITY LAYER
 * =============================================================================
 */

/* Uncomment the appropriate include for your engine: */

/* ========== Q2PRO ENGINE ========== */
/* For Q2Pro, uncomment ONE of these based on your version: */
#include "gl.h"      /* Q2Pro unified renderer - newer versions */
/* #include "refresh/gl.h" */   /* Q2Pro - some versions */
/* #include "client/client.h" */ /* For cl.time access */

/* ========== STANDARD QUAKE II / QUAKE2XP ========== */
/* #include "gl_local.h"  */   /* Standard Quake II ref_gl */
/* #include "r_local.h"   */   /* Quake2XP and some forks */

/* Q2Pro gl.h provides qboolean, qtrue, qfalse */

/* OpenGL - include the appropriate header for your platform */
#ifdef _WIN32
#include <windows.h>
#endif
#include <GL/gl.h>

/*
 * Cvar system compatibility
 */
/* Real Cvar pointers */
cvar_t *vcr_enabled;
cvar_t *vcr_quality;
cvar_t *vcr_mode;
cvar_t *vcr_desaturation;
cvar_t *vcr_noise_dots;
cvar_t *vcr_grain_intensity;
cvar_t *vcr_scanline_alpha;
cvar_t *vcr_distortion_interval;
cvar_t *vcr_distortion_duration;
cvar_t *vcr_cctv_chance;
cvar_t *vcr_tracking_lines;
cvar_t *vcr_rec_indicator;
cvar_t *vcr_timestamp;
cvar_t *vcr_static_bursts;
cvar_t *vcr_debug;

/* Macros to access cvar values safely */
#define CVAR_VALUE(cv) ((cv) ? (cv)->value : 0.0f)
#define CVAR_INT(cv)   ((cv) ? (cv)->integer : 0)
#define CVAR_SET_INT(cv, val) if (cv) { Cvar_SetValue(cv, (float)(val), 0); }


/*
 * =============================================================================
 *  INTERNAL STATE
 * =============================================================================
 */

typedef struct {
    /* Initialization flag */
    qboolean    initialized;
    
    /* Timing */
    float       effect_start_time;
    float       last_distort_time;
    float       cctv_start_time;
    float       current_time;
    
    /* Found footage state */
    float       static_start_time;      /* When static burst started */
    float       tape_damage_start;      /* When tape damage started */
    float       frame_drop_start;       /* When frame drop started */
    float       battery_level;          /* 0.0 to 1.0 */
    
    /* Forced triggers (for debug) */
    qboolean    force_distortion;
    qboolean    force_cctv;
    qboolean    force_static;
    qboolean    force_tape_damage;
    
    /* Subtle mode: whether this spike includes B&W (30% chance) */
    qboolean    do_bw_this_spike;
    
    /* Animation */
    int         frame_count;
    unsigned    rng_state;
    
    /* Tracking line position */
    float       tracking_line_y;
    
    /* Tape damage positions */
    float       damage_line_y[10];
    
    /* Cached dimensions */
    int         width;
    int         height;
    
    /* Quality multipliers */
    float       quality_mult;
    
    /* Screen capture texture for desaturation */
    unsigned int screen_tex;
    
    /* Generation counter - incremented on VCR_Init to detect context resets */
    int         tex_generation;
} vcr_state_t;

static vcr_state_t vcr = {0};


/*
 * =============================================================================
 *  QUALITY PRESETS
 * =============================================================================
 */

typedef struct {
    float noise_mult;
    float grain_mult;
    float scanline_skip;
    float vignette_step;
    qboolean tracking;
    qboolean color_shift;
    qboolean flicker;
    qboolean rec_indicator;
    qboolean timestamp;
    qboolean static_bursts;
} vcr_quality_preset_t;

static const vcr_quality_preset_t quality_presets[3] = {
    /* LOW */
    { 0.25f, 0.0f, 4, 40, qfalse, qfalse, qfalse, qtrue, qfalse, qfalse },
    /* MEDIUM */
    { 0.6f, 0.5f, 2, 30, qtrue, qtrue, qfalse, qtrue, qtrue, qtrue },
    /* HIGH */
    { 1.0f, 1.0f, 2, 20, qtrue, qtrue, qtrue, qtrue, qtrue, qtrue }
};


/*
 * =============================================================================
 *  FAST RANDOM NUMBER GENERATOR
 * =============================================================================
 */

static unsigned vcr_rand_next(void)
{
    vcr.rng_state ^= vcr.rng_state << 13;
    vcr.rng_state ^= vcr.rng_state >> 17;
    vcr.rng_state ^= vcr.rng_state << 5;
    return vcr.rng_state;
}

static float vcr_rand_float(void)
{
    return (float)(vcr_rand_next() & 0xFFFF) / 65535.0f;
}

static int vcr_rand_int(int max)
{
    if (max <= 0) return 0;
    return (int)(vcr_rand_next() % (unsigned)max);
}

static void vcr_rand_seed(unsigned seed)
{
    vcr.rng_state = seed ? seed : 0xDEADBEEF;
    vcr_rand_next();
    vcr_rand_next();
    vcr_rand_next();
}


/*
 * =============================================================================
 *  OPENGL HELPERS
 * =============================================================================
 */

static void vcr_gl_begin_2d(int width, int height)
{
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, width, height, 0, -1, 1);
    
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
}

static void vcr_gl_end_2d(void)
{
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    
    glPopAttrib();
}

static void vcr_draw_rect(float x, float y, float w, float h, 
                          float r, float g, float b, float a)
{
    glColor4f(r, g, b, a);
    glBegin(GL_QUADS);
    glVertex2f(x, y);
    glVertex2f(x + w, y);
    glVertex2f(x + w, y + h);
    glVertex2f(x, y + h);
    glEnd();
}

/* Simple bitmap font rendering for REC and timestamp */
static void vcr_draw_char(float x, float y, char c, float size, float r, float g, float b, float a)
{
    /* Simple 5x7 bitmap font patterns for digits and letters */
    static const unsigned char font_data[128][7] = {
        /* ... simplified - just key characters ... */
    };
    
    /* For now, draw a simple rectangle per character */
    glColor4f(r, g, b, a);
    glBegin(GL_QUADS);
    glVertex2f(x, y);
    glVertex2f(x + size * 0.6f, y);
    glVertex2f(x + size * 0.6f, y + size);
    glVertex2f(x, y + size);
    glEnd();
}

static void vcr_draw_text(float x, float y, const char *text, float size, 
                         float r, float g, float b, float a)
{
    float cx = x;
    while (*text) {
        if (*text != ' ') {
            vcr_draw_char(cx, y, *text, size, r, g, b, a);
        }
        cx += size * 0.7f;
        text++;
    }
}


/*
 * =============================================================================
 *  EFFECT RENDERING - BASE COMPONENTS
 * =============================================================================
 */

static const vcr_quality_preset_t* vcr_get_preset(void)
{
    int q = CVAR_INT(vcr_quality);
    if (q < 0) q = 0;
    if (q > 2) q = 2;
    return &quality_presets[q];
}

static void vcr_draw_desaturation(float intensity, float sepia_tint)
{
    /* BLEND-ONLY DESATURATION - No texture capture needed!
       This approach is compatible with all drivers and won't corrupt console text.
       
       Method: 
       1. Darken image with MULTIPLY blend (reduces brightness)
       2. Add grey overlay to reduce color saturation perception
    */
    
    float darken_level;
    
    if (intensity <= 0.01f) return;
    
    glDisable(GL_TEXTURE_2D);
    
    /* Step 1: Darken with MULTIPLY */
    /* At 50% intensity (normal): darken to 0.9 (90% brightness) */
    /* At 100% intensity (spike): darken to 0.5 (50% brightness) */
    darken_level = 1.0f - (intensity * 0.5f);
    if (darken_level < 0.5f) darken_level = 0.5f;
    
    glBlendFunc(GL_DST_COLOR, GL_ZERO);
    glColor4f(darken_level, darken_level, darken_level, 1.0f);
    
    glBegin(GL_QUADS);
    glVertex2f(0.0f, 0.0f);
    glVertex2f((float)vcr.width, 0.0f);
    glVertex2f((float)vcr.width, (float)vcr.height);
    glVertex2f(0.0f, (float)vcr.height);
    glEnd();
    
    /* Step 2: Grey overlay to wash out colors */
    /* At 50% intensity (normal): 10% grey overlay */
    /* At 100% intensity (spike): 30% grey overlay */
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(0.5f, 0.5f, 0.5f, intensity * 0.3f);
    
    glBegin(GL_QUADS);
    glVertex2f(0.0f, 0.0f);
    glVertex2f((float)vcr.width, 0.0f);
    glVertex2f((float)vcr.width, (float)vcr.height);
    glVertex2f(0.0f, (float)vcr.height);
    glEnd();
    
    /* Step 3: Add slight sepia/warmth if requested */
    if (sepia_tint > 0.01f) {
        glColor4f(0.3f, 0.2f, 0.1f, sepia_tint * intensity * 0.2f);
        
        glBegin(GL_QUADS);
        glVertex2f(0.0f, 0.0f);
        glVertex2f((float)vcr.width, 0.0f);
        glVertex2f((float)vcr.width, (float)vcr.height);
        glVertex2f(0.0f, (float)vcr.height);
        glEnd();
    }
}

static void vcr_draw_film_grain(float intensity, float quality_mult)
{
    int i;
    int grain_count;
    
    if (quality_mult <= 0.0f) return;
    
    grain_count = (int)((vcr.width * vcr.height) / 2000 * quality_mult);
    
    glPointSize(1.0f);
    glBegin(GL_POINTS);
    
    for (i = 0; i < grain_count; i++) {
        float x = vcr_rand_float() * vcr.width;
        float y = vcr_rand_float() * vcr.height;
        float brightness = vcr_rand_float();
        float alpha = intensity * (0.3f + brightness * 0.7f);
        
        if (brightness > 0.5f) {
            glColor4f(1.0f, 1.0f, 1.0f, alpha);
        } else {
            glColor4f(0.0f, 0.0f, 0.0f, alpha);
        }
        glVertex2f(x, y);
    }
    
    glEnd();
}

static void vcr_draw_scanlines(float alpha, int skip)
{
    int y;
    
    if (skip < 1) skip = 2;
    
    glColor4f(0.0f, 0.0f, 0.0f, alpha);
    glBegin(GL_LINES);
    
    for (y = 0; y < vcr.height; y += skip) {
        glVertex2f(0, (float)y);
        glVertex2f((float)vcr.width, (float)y);
    }
    
    glEnd();
}

static void vcr_draw_noise_dots(int count, float base_alpha)
{
    int i;
    
    if (count <= 0) return;
    
    glPointSize(2.0f);
    glBegin(GL_POINTS);
    
    for (i = 0; i < count; i++) {
        float x = vcr_rand_float() * vcr.width;
        float y = vcr_rand_float() * vcr.height;
        float brightness = 0.7f + vcr_rand_float() * 0.3f;
        float alpha = base_alpha * (0.5f + vcr_rand_float() * 0.5f);
        
        glColor4f(brightness, brightness, brightness, alpha);
        glVertex2f(x, y);
    }
    
    glEnd();
}

static void vcr_draw_tracking_lines(float time)
{
    float band_height = VCR_TRACKING_LINE_HEIGHT;
    float y;
    
    vcr.tracking_line_y += VCR_TRACKING_LINE_SPEED * 0.016f;
    if (vcr.tracking_line_y > vcr.height + band_height) {
        vcr.tracking_line_y = -band_height;
    }
    
    y = vcr.tracking_line_y;
    
    vcr_draw_rect(0, y, (float)vcr.width, band_height, 0.1f, 0.1f, 0.1f, 0.3f);
    vcr_draw_rect(0, y - 1, (float)vcr.width, 1, 1.0f, 0.0f, 0.0f, 0.1f);
    vcr_draw_rect(0, y + band_height, (float)vcr.width, 1, 0.0f, 1.0f, 1.0f, 0.1f);
}

static void vcr_apply_jitter(float intensity)
{
    float jitter_x = (vcr_rand_float() - 0.5f) * 2.0f * intensity;
    float jitter_y = (vcr_rand_float() - 0.5f) * 0.5f * intensity;
    
    glMatrixMode(GL_MODELVIEW);
    glTranslatef(jitter_x, jitter_y, 0);
}

static void vcr_draw_flicker(float intensity, float time)
{
    float flicker;
    
    flicker = (float)sin(time * VCR_CCTV_FLICKER_SPEED) * 0.5f + 0.5f;
    flicker += (float)sin(time * VCR_CCTV_FLICKER_SPEED * 2.3f) * 0.3f;
    flicker *= VCR_CCTV_FLICKER_INTENSITY * intensity;
    
    vcr_draw_rect(0, 0, (float)vcr.width, (float)vcr.height, 1.0f, 1.0f, 1.0f, flicker);
}

static void vcr_draw_color_separation(float intensity)
{
    float offset = intensity * vcr.width * VCR_SPIKE_COLOR_SHIFT;
    
    vcr_draw_rect(-offset, 0, (float)vcr.width, (float)vcr.height,
                  1.0f, 0.0f, 0.0f, 0.03f * intensity);
    vcr_draw_rect(offset, 0, (float)vcr.width, (float)vcr.height,
                  0.0f, 1.0f, 1.0f, 0.03f * intensity);
}

static void vcr_draw_cctv_overlay(float intensity, float time)
{
    const vcr_quality_preset_t *preset = vcr_get_preset();
    float cx, cy, max_dist, dist, vignette_val;
    int x, y, step;
    
    vcr_draw_rect(0, 0, (float)vcr.width, (float)vcr.height, 0.1f, 0.1f, 0.1f, intensity * 0.3f);
    vcr_draw_rect(0, 0, (float)vcr.width, (float)vcr.height, 0.0f, 0.0f, 0.0f, intensity * 0.15f);
    
    if (preset->flicker) {
        vcr_draw_flicker(intensity, time);
    }
    
    cx = vcr.width * 0.5f;
    cy = vcr.height * 0.5f;
    max_dist = (float)sqrt(cx * cx + cy * cy);
    step = (int)preset->vignette_step;
    
    glBegin(GL_QUADS);
    for (y = 0; y < vcr.height; y += step) {
        for (x = 0; x < vcr.width; x += step) {
            float fx = (float)x - cx;
            float fy = (float)y - cy;
            dist = (float)sqrt(fx * fx + fy * fy) / max_dist;
            vignette_val = dist * dist * VCR_CCTV_VIGNETTE * intensity;
            
            glColor4f(0.0f, 0.0f, 0.0f, vignette_val);
            glVertex2f((float)x, (float)y);
            glVertex2f((float)(x + step), (float)y);
            glVertex2f((float)(x + step), (float)(y + step));
            glVertex2f((float)x, (float)(y + step));
        }
    }
    glEnd();
    
    vcr_draw_noise_dots((int)(VCR_CCTV_NOISE_DOTS * preset->noise_mult), 0.8f);
}


/*
 * =============================================================================
 *  FOUND FOOTAGE EFFECTS
 * =============================================================================
 */

/* REC blinking indicator */
static void vcr_draw_rec_indicator(float time)
{
    float blink = (float)fmod(time, VCR_REC_BLINK_SPEED * 2.0f);
    float alpha = (blink < VCR_REC_BLINK_SPEED) ? 1.0f : 0.3f;
    
    float x = 20.0f;
    float y = 20.0f;
    float dot_size = 8.0f;
    
    /* Red recording dot */
    glColor4f(1.0f, 0.0f, 0.0f, alpha);
    glBegin(GL_TRIANGLE_FAN);
    {
        int i;
        glVertex2f(x + dot_size/2, y + dot_size/2);
        for (i = 0; i <= 16; i++) {
            float angle = (float)i * 3.14159f * 2.0f / 16.0f;
            glVertex2f(x + dot_size/2 + (float)cos(angle) * dot_size/2,
                      y + dot_size/2 + (float)sin(angle) * dot_size/2);
        }
    }
    glEnd();
    
    /* REC text */
    vcr_draw_rect(x + 15, y, 30, 12, 1.0f, 0.0f, 0.0f, alpha * 0.8f);
    vcr_draw_rect(x + 17, y + 2, 26, 8, 0.0f, 0.0f, 0.0f, 1.0f);
    
    /* Simple "REC" letters - properly formed */
    glColor4f(1.0f, 1.0f, 1.0f, alpha);
    
    /* R letter */
    vcr_draw_rect(x + 19, y + 3, 2, 6, 1.0f, 1.0f, 1.0f, alpha); /* Left vertical */
    vcr_draw_rect(x + 19, y + 3, 5, 1, 1.0f, 1.0f, 1.0f, alpha); /* Top horizontal */
    vcr_draw_rect(x + 23, y + 3, 1, 3, 1.0f, 1.0f, 1.0f, alpha); /* Right top vertical */
    vcr_draw_rect(x + 19, y + 5, 5, 1, 1.0f, 1.0f, 1.0f, alpha); /* Middle horizontal */
    vcr_draw_rect(x + 22, y + 6, 2, 3, 1.0f, 1.0f, 1.0f, alpha); /* Diagonal leg (right bottom) */
    
    /* E letter */
    vcr_draw_rect(x + 26, y + 3, 2, 6, 1.0f, 1.0f, 1.0f, alpha); /* Left vertical */
    vcr_draw_rect(x + 26, y + 3, 5, 1, 1.0f, 1.0f, 1.0f, alpha); /* Top horizontal */
    vcr_draw_rect(x + 26, y + 5, 4, 1, 1.0f, 1.0f, 1.0f, alpha); /* Middle horizontal */
    vcr_draw_rect(x + 26, y + 8, 5, 1, 1.0f, 1.0f, 1.0f, alpha); /* Bottom horizontal */
    
    /* C letter */
    vcr_draw_rect(x + 33, y + 3, 2, 6, 1.0f, 1.0f, 1.0f, alpha); /* Left vertical */
    vcr_draw_rect(x + 33, y + 3, 5, 1, 1.0f, 1.0f, 1.0f, alpha); /* Top horizontal */
    vcr_draw_rect(x + 33, y + 8, 5, 1, 1.0f, 1.0f, 1.0f, alpha); /* Bottom horizontal */
}

/*
 * =============================================================================
 *  HELPER: SEGMENT DIGIT RENDERING
 * =============================================================================
 */

/* Draw a digit using simple segments (like a digital clock or VCR OSD) */
static void vcr_draw_digit(float x, float y, float size, int digit)
{
    /* 
     * Segment Map:
     *   A
     * F   B
     *   G
     * E   C
     *   D
     */
    float w = size * 0.6f;
    float h = size;
    float t = size * 0.15f; /* Thickness */
    
    qboolean segA = qfalse, segB = qfalse, segC = qfalse, segD = qfalse;
    qboolean segE = qfalse, segF = qfalse, segG = qfalse;
    
    switch (digit) {
        case 0: segA=1; segB=1; segC=1; segD=1; segE=1; segF=1; break;
        case 1: segB=1; segC=1; break;
        case 2: segA=1; segB=1; segG=1; segE=1; segD=1; break;
        case 3: segA=1; segB=1; segG=1; segC=1; segD=1; break;
        case 4: segF=1; segG=1; segB=1; segC=1; break;
        case 5: segA=1; segF=1; segG=1; segC=1; segD=1; break;
        case 6: segA=1; segF=1; segG=1; segE=1; segC=1; segD=1; break;
        case 7: segA=1; segB=1; segC=1; break;
        case 8: segA=1; segB=1; segC=1; segD=1; segE=1; segF=1; segG=1; break;
        case 9: segA=1; segB=1; segC=1; segD=1; segF=1; segG=1; break;
    }
    
    if (segA) vcr_draw_rect(x, y, w, t, 1,1,1,0.9f);
    if (segB) vcr_draw_rect(x+w-t, y, t, h/2, 1,1,1,0.9f);
    if (segC) vcr_draw_rect(x+w-t, y+h/2, t, h/2, 1,1,1,0.9f);
    if (segD) vcr_draw_rect(x, y+h-t, w, t, 1,1,1,0.9f);
    if (segE) vcr_draw_rect(x, y+h/2, t, h/2, 1,1,1,0.9f);
    if (segF) vcr_draw_rect(x, y, t, h/2, 1,1,1,0.9f);
    if (segG) vcr_draw_rect(x, y+h/2-t/2, w, t, 1,1,1,0.9f);
}

/* VHS timestamp overlay */
static void vcr_draw_timestamp(float time_unused)
{
    /* Position at bottom-right */
    float x = vcr.width - 240.0f;
    float y = vcr.height - 30.0f;
    time_t rawtime;
    struct tm * t;
    
    time(&rawtime);
    t = localtime(&rawtime);
    
    /* Background */
    vcr_draw_rect(x - 5, y - 5, 235, 24, 0.0f, 0.0f, 0.0f, 0.5f);
    
    /* Draw Date: MM-DD-2007 (Force 2007) */
    /* Using segment digits. Digit size 12. */
    float ds = 12.0f;
    float dx = x;
    float dy = y;
    
    /* Month */
    int mon = t->tm_mon + 1;
    vcr_draw_digit(dx, dy, ds, mon / 10); dx += 10;
    vcr_draw_digit(dx, dy, ds, mon % 10); dx += 10;
    vcr_draw_rect(dx+2, dy+5, 4, 2, 1,1,1,0.9f); dx += 10; /* Dash */
    
    /* Day */
    vcr_draw_digit(dx, dy, ds, t->tm_mday / 10); dx += 10;
    vcr_draw_digit(dx, dy, ds, t->tm_mday % 10); dx += 10;
    vcr_draw_rect(dx+2, dy+5, 4, 2, 1,1,1,0.9f); dx += 10; /* Dash */
    
    /* Year (Fixed 2007) */
    vcr_draw_digit(dx, dy, ds, 2); dx += 10;
    vcr_draw_digit(dx, dy, ds, 0); dx += 10;
    vcr_draw_digit(dx, dy, ds, 0); dx += 10;
    vcr_draw_digit(dx, dy, ds, 7); dx += 20; /* Space */
    
    /* Time: HH:MM:SS */
    vcr_draw_digit(dx, dy, ds, t->tm_hour / 10); dx += 10;
    vcr_draw_digit(dx, dy, ds, t->tm_hour % 10); dx += 10;
    vcr_draw_rect(dx+2, dy+3, 2, 2, 1,1,1,0.9f); /* Colon dots */
    vcr_draw_rect(dx+2, dy+8, 2, 2, 1,1,1,0.9f); dx += 8;
    
    vcr_draw_digit(dx, dy, ds, t->tm_min / 10); dx += 10;
    vcr_draw_digit(dx, dy, ds, t->tm_min % 10); dx += 10;
    vcr_draw_rect(dx+2, dy+3, 2, 2, 1,1,1,0.9f);
    vcr_draw_rect(dx+2, dy+8, 2, 2, 1,1,1,0.9f); dx += 8;
    
    vcr_draw_digit(dx, dy, ds, t->tm_sec / 10); dx += 10;
    vcr_draw_digit(dx, dy, ds, t->tm_sec % 10);
}

/* Full-screen static burst */
static void vcr_draw_static_burst(float intensity)
{
    int i;
    int static_count = (vcr.width * vcr.height) / 50;
    
    /* Heavy noise */
    glPointSize(2.0f);
    glBegin(GL_POINTS);
    
    for (i = 0; i < static_count; i++) {
        float x = vcr_rand_float() * vcr.width;
        float y = vcr_rand_float() * vcr.height;
        float brightness = vcr_rand_float();
        
        glColor4f(brightness, brightness, brightness, intensity);
        glVertex2f(x, y);
    }
    
    glEnd();
    
    /* Horizontal tear lines */
    for (i = 0; i < 5; i++) {
        float y = vcr_rand_float() * vcr.height;
        float offset = (vcr_rand_float() - 0.5f) * 20.0f;
        vcr_draw_rect(offset, y, (float)vcr.width, 2, 0.5f, 0.5f, 0.5f, intensity * 0.5f);
    }
}

/* Tape damage / corruption lines */
static void vcr_draw_tape_damage(float intensity)
{
    int i;
    
    for (i = 0; i < VCR_TAPE_DAMAGE_LINES; i++) {
        float y = vcr.damage_line_y[i];
        float corrupt_height = 3 + vcr_rand_float() * 5;
        
        /* Corruption band */
        vcr_draw_rect(0, y, (float)vcr.width, corrupt_height, 
                      0.2f, 0.2f, 0.2f, intensity * 0.7f);
        
        /* Random noise in band */
        int dots = 20 + vcr_rand_int(30);
        int j;
        glPointSize(1.0f);
        glBegin(GL_POINTS);
        for (j = 0; j < dots; j++) {
            float dx = vcr_rand_float() * vcr.width;
            float dy = y + vcr_rand_float() * corrupt_height;
            float b = vcr_rand_float();
            glColor4f(b, b, b, intensity);
            glVertex2f(dx, dy);
        }
        glEnd();
        
        /* Color fringe */
        vcr_draw_rect(vcr_rand_float() * 5, y - 1, (float)vcr.width, 1, 
                      1.0f, 0.0f, 0.0f, intensity * 0.3f);
        vcr_draw_rect(-vcr_rand_float() * 5, y + corrupt_height, (float)vcr.width, 1, 
                      0.0f, 1.0f, 1.0f, intensity * 0.3f);
        
        /* Move damage line */
        vcr.damage_line_y[i] += 30.0f * 0.016f;
        if (vcr.damage_line_y[i] > vcr.height + 10) {
            vcr.damage_line_y[i] = -10 - vcr_rand_float() * 50;
        }
    }
}



/* Battery indicator */
static void vcr_draw_battery_indicator(float time)
{
    float x = vcr.width - 60.0f;
    float y = 20.0f;
    float battery_width = 40.0f;
    float battery_height = 16.0f;
    float fill;
    float blink = 1.0f;
    
    /* Blink when low */
    if (vcr.battery_level < VCR_BATTERY_LOW_THRESHOLD) {
        blink = ((float)fmod(time, VCR_BATTERY_BLINK_SPEED) < VCR_BATTERY_BLINK_SPEED * 0.5f) ? 1.0f : 0.3f;
    }
    
    /* Battery outline */
    vcr_draw_rect(x, y, battery_width, battery_height, 1.0f, 1.0f, 1.0f, 0.8f * blink);
    vcr_draw_rect(x + 2, y + 2, battery_width - 4, battery_height - 4, 0.0f, 0.0f, 0.0f, 1.0f);
    
    /* Battery tip */
    vcr_draw_rect(x + battery_width, y + 4, 4, 8, 1.0f, 1.0f, 1.0f, 0.8f * blink);
    
    /* Battery fill */
    fill = vcr.battery_level * (battery_width - 6);
    if (vcr.battery_level < VCR_BATTERY_LOW_THRESHOLD) {
        vcr_draw_rect(x + 3, y + 3, fill, battery_height - 6, 1.0f, 0.0f, 0.0f, 0.9f * blink);
    } else if (vcr.battery_level < 0.5f) {
        vcr_draw_rect(x + 3, y + 3, fill, battery_height - 6, 1.0f, 1.0f, 0.0f, 0.9f * blink);
    } else {
        vcr_draw_rect(x + 3, y + 3, fill, battery_height - 6, 0.0f, 1.0f, 0.0f, 0.9f * blink);
    }
}

/* Chromatic aberration */
static void vcr_draw_chromatic_aberration(float intensity)
{
    float offset = VCR_CHROMATIC_AMOUNT * intensity;
    
    /* Red channel shift left */
    vcr_draw_rect(-offset, 0, (float)vcr.width, (float)vcr.height,
                  1.0f, 0.0f, 0.0f, 0.05f * intensity);
    
    /* Blue channel shift right */
    vcr_draw_rect(offset, 0, (float)vcr.width, (float)vcr.height,
                  0.0f, 0.0f, 1.0f, 0.05f * intensity);
}


/*
 * =============================================================================
 *  PUBLIC API
 * =============================================================================
 */

void VCR_Init(void)
{
    int i;
    
    memset(&vcr, 0, sizeof(vcr));
    
    vcr.initialized = qtrue;
    vcr.cctv_start_time = -1.0f;
    vcr.static_start_time = -1.0f;
    vcr.tape_damage_start = -1.0f;
    vcr.frame_drop_start = -1.0f;
    vcr.battery_level = 0.75f;  /* 75% per client request */
    vcr.tracking_line_y = 0.0f;
    
    /* Initialize tape damage line positions */
    for (i = 0; i < 10; i++) {
        vcr.damage_line_y[i] = -50 - (float)i * 30;
    }
    
    vcr_rand_seed((unsigned)(size_t)&vcr ^ 0x12345678);

    /* REGISTER CVARS WITH ENGINE */
    vcr_enabled = Cvar_Get("vcr_enabled", "1", CVAR_ARCHIVE);
    vcr_quality = Cvar_Get("vcr_quality", "2", CVAR_ARCHIVE); // High default
    vcr_mode = Cvar_Get("vcr_mode", "0", CVAR_ARCHIVE);
    
    // Feature toggles
    vcr_rec_indicator = Cvar_Get("vcr_rec_indicator", "1", CVAR_ARCHIVE);
    vcr_timestamp = Cvar_Get("vcr_timestamp", "1", CVAR_ARCHIVE);
    vcr_tracking_lines = Cvar_Get("vcr_tracking_lines", "1", CVAR_ARCHIVE);
    vcr_static_bursts = Cvar_Get("vcr_static_bursts", "1", CVAR_ARCHIVE);
    vcr_debug = Cvar_Get("vcr_debug", "0", 0);

    // Advanced tuning (non-archive by default to reset on restart)
    // Default 0.5 per user requirement (50% B&W)
    vcr_desaturation = Cvar_Get("vcr_desaturation", "0.5", 0);
    vcr_noise_dots = Cvar_Get("vcr_noise_dots", "1.0", 0);
    vcr_grain_intensity = Cvar_Get("vcr_grain_intensity", "1.0", 0);
    vcr_scanline_alpha = Cvar_Get("vcr_scanline_alpha", "1.0", 0);
    vcr_distortion_interval = Cvar_Get("vcr_distortion_interval", "20.0", 0);
    vcr_distortion_duration = Cvar_Get("vcr_distortion_duration", "1.5", 0);
    vcr_cctv_chance = Cvar_Get("vcr_cctv_chance", "0.3", 0);
    
    /* Create texture for B&W capture */
    glGenTextures(1, &vcr.screen_tex);
    glBindTexture(GL_TEXTURE_2D, vcr.screen_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    /* Increment generation counter to signal desaturation function to reset its cache */
    vcr.tex_generation++;
    
    Com_Printf("VCR Effect Initialized. Type 'vcr_mode 1' for CCTV.\n");
}

void VCR_Shutdown(void)
{
    if (vcr.screen_tex) {
        glDeleteTextures(1, &vcr.screen_tex);
        vcr.screen_tex = 0;
    }
    vcr.initialized = qfalse;
}

void VCR_Enable(void)
{
    if (vcr_enabled) Cvar_SetValue(vcr_enabled, 1.0f, 0);
}

void VCR_Disable(void)
{
    if (vcr_enabled) Cvar_SetValue(vcr_enabled, 0.0f, 0);
}

void VCR_Toggle(void)
{
    if (VCR_IsEnabled()) VCR_Disable();
    else VCR_Enable();
}

int VCR_IsEnabled(void)
{
    return CVAR_INT(vcr_enabled) != 0;
}

void VCR_Reset(void)
{
    vcr.effect_start_time = -1.0f;
    vcr.last_distort_time = -1.0f;
    vcr.cctv_start_time = -1.0f;
    vcr.static_start_time = -1.0f;
    vcr.tape_damage_start = -1.0f;
    vcr.frame_count = 0;
    vcr.tracking_line_y = -50.0f;
    vcr.force_distortion = qfalse;
    vcr.force_cctv = qfalse;
    vcr.force_static = qfalse;
    vcr.force_tape_damage = qfalse;
}

void VCR_SetMode(int mode)
{
    if (mode < 0) mode = 0;
    if (mode > 3) mode = 3;
    CVAR_SET_INT(vcr_mode, mode);
}

int VCR_GetMode(void)
{
    return CVAR_INT(vcr_mode);
}

void VCR_ForceDistortion(void) { vcr.force_distortion = qtrue; }
void VCR_ForceCCTV(void) { vcr.force_cctv = qtrue; }
void VCR_ForceStatic(void) { vcr.force_static = qtrue; }
void VCR_ForceTapeDamage(void) { vcr.force_tape_damage = qtrue; }

void VCR_SetQuality(int level)
{
    if (level < 0) level = 0;
    if (level > 2) level = 2;
    CVAR_SET_INT(vcr_quality, level);
}

int VCR_GetQuality(void)
{
    return CVAR_INT(vcr_quality);
}

void VCR_SetBattery(float level)
{
    if (level < 0.0f) level = 0.0f;
    if (level > 1.0f) level = 1.0f;
    vcr.battery_level = level;
}


/*
 * VCR_DrawEffect - Main rendering function
 */
void VCR_DrawEffect(int screen_width, int screen_height, float time)
{
    const vcr_quality_preset_t *preset;
    int   mode;
    float elapsed;
    float loop_time;
    
    /* Effect State Variables (Default: Constant state) */
    float current_desaturation = 0.08f; /* 1. Constant: 8% B&W (Updated) */
    int   current_dots = 20;            /* 1. Constant: 20 Dots (Updated) */
    float current_jitter = 0.0f;
    float grain_alpha = VCR_NORMAL_GRAIN;
    qboolean show_tracking = qfalse;
    
    /* Early out */
    if (!vcr.initialized || !VCR_IsEnabled()) return;
    
    /* Robustness check */
    if (vcr.screen_tex != 0 && !glIsTexture(vcr.screen_tex)) {
        glGenTextures(1, &vcr.screen_tex);
        glBindTexture(GL_TEXTURE_2D, vcr.screen_tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glBindTexture(GL_TEXTURE_2D, 0); 
    }
    
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    
    preset = vcr_get_preset();
    mode = CVAR_INT(vcr_mode);
    vcr.width = screen_width;
    vcr.height = screen_height;
    vcr.current_time = time;
    
    if (vcr.effect_start_time < 0) {
        vcr.effect_start_time = time;
        vcr.tracking_line_y = -50.0f; /* Start off-screen */
    }
    
    elapsed = time - vcr.effect_start_time;
    loop_time = (float)fmod(elapsed, 50.0f); /* 6. Loop: 50s cycle */
    
    /* 
     * SEQUENCE LOGIC
     * 1. Constant: 8% B&W, 20 Dots (Defaults set above)
     */
    
    /* 2. Event 1 (at 10s): Tracking line down screen ONCE */
    if (loop_time >= 10.0f && loop_time < 30.0f) { /* Give it generous time to pass */
        /* If we just started this phase, reset line to top */
        if (loop_time < 10.1f) {
             /* Reset to top */
             if (vcr.tracking_line_y > screen_height) {
                 vcr.tracking_line_y = -VCR_TRACKING_LINE_HEIGHT;
             }
        }
        
        /* Only move if not finished */
        if (vcr.tracking_line_y <= screen_height + VCR_TRACKING_LINE_HEIGHT + 200.0f) { /* Adjusted for 2nd line */
            float speed = screen_height / 5.0f; /* Traverse in ~5 seconds */
            vcr.tracking_line_y += speed * 0.016f; /* dt approx */
            show_tracking = qtrue;
        }
    } else {
        /* Reset line off-screen when not in phase */
        vcr.tracking_line_y = screen_height + 500.0f;
    }
    
    /* 3. Event 2 (at 20s): Screen shake (2s) */
    if (loop_time >= 20.0f && loop_time < 22.0f) {
        current_jitter = 1.0f;
    }
    
    /* 4. Event 3 (at 30s): More white dots (2s) */
    if (loop_time >= 30.0f && loop_time < 32.0f) {
        /* "More white dots" - increased to ensure visibility */
        current_dots += 100;
    }
    
    /* 5. Event 4 (at 40s): 40% B&W effect (2s) -> Updated to 50% */
    if (loop_time >= 40.0f && loop_time < 42.0f) {
        current_desaturation = 0.50f;
    }
    
    
    /* RENDER PHASES */
    
    /* Reseed RNG */
    vcr.frame_count++;
    vcr_rand_seed(vcr.rng_state ^ (unsigned)vcr.frame_count ^ (unsigned)(time * 1000));
    
    /* Begin Drawing */
    vcr_gl_begin_2d(screen_width, screen_height);
    
    /* Apply Jitter */
    if (current_jitter > 0.0f) {
        vcr_apply_jitter(VCR_SPIKE_JITTER_MAX * current_jitter);
    }
    
    /* Draw Effect Layers (Unified VCR/CCTV Logic) */
    
    /* 1. Desaturation */
    vcr_draw_desaturation(current_desaturation, VCR_SEPIA_TINT);
    
    /* 2. Grain (Constant small amount) */
    vcr_draw_film_grain(grain_alpha, preset->grain_mult * 0.5f);
    
    /* 3. Noise Dots - Ensure int cast doesn't truncate to 0 too easily */
    vcr_draw_noise_dots((int)(current_dots * preset->noise_mult), 0.5f);
    
    /* 4. Tracking Line (if active) - 1 LINE scrolling down for 5 seconds */
    if (show_tracking && CVAR_INT(vcr_tracking_lines)) {
        float y = vcr.tracking_line_y;
        float h = VCR_TRACKING_LINE_HEIGHT;
        
        /* Single tracking line */
        vcr_draw_rect(0, y, (float)vcr.width, h, 0.1f, 0.1f, 0.1f, 0.3f);
        vcr_draw_rect(0, y - 1, (float)vcr.width, 1, 1.0f, 0.0f, 0.0f, 0.1f);
        vcr_draw_rect(0, y + h, (float)vcr.width, 1, 0.0f, 1.0f, 1.0f, 0.1f);
    }
    
    /* 5. Overlays (Mode Specific UI) */
    /* VCR Mode: REC + Battery + Timestamp */
    /* CCTV Mode: Timestamp ONLY */
    
    if (mode == VCR_MODE_VCR) {
        if (preset->rec_indicator && CVAR_INT(vcr_rec_indicator)) {
            vcr_draw_rec_indicator(time);
        }
        vcr_draw_battery_indicator(time);
    }
    
    /* Timestamp - Unified */
    if (CVAR_INT(vcr_timestamp)) {
        if (mode == VCR_MODE_VCR || mode == VCR_MODE_CCTV) {
            vcr_draw_timestamp(time);
        }
    }
    
    /* Chromatic aberration on distortion */
    if (current_jitter > 0.0f && preset->color_shift) {
        vcr_draw_chromatic_aberration(current_jitter);
    }
    
    vcr_gl_end_2d();
    
    /* Restore OpenGL state */
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    /* Explicitly reset critical states to ensure console text renders correctly */
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    glDisable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);
    
    glPopAttrib();
}
