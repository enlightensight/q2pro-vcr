/*
 * VCR/CCTV/Found Footage Screen Effect for Quake II Engine
 * 
 * Simulates old VCR/CCTV/recorded footage overlay for story moments.
 * Includes found footage horror game effects (REC indicator, static bursts, etc.)
 * Client-side only, uses legacy OpenGL fixed-function pipeline.
 * 
 * Integration:
 *   - Call VCR_Init() in your renderer initialization
 *   - Call VCR_DrawEffect() after scene rendering, before UI
 *   - Call VCR_Shutdown() in your renderer shutdown
 * 
 * Toggle: Use cvar "vcr_enabled" (0=off, 1=on)
 * Quality: Use cvar "vcr_quality" (0=low, 1=medium, 2=high)
 * Mode: Use cvar "vcr_mode" (0=VCR, 1=CCTV, 2=Found Footage, 3=Night Vision)
 */

#ifndef VCR_EFFECT_H
#define VCR_EFFECT_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * =============================================================================
 *  EFFECT MODES
 * =============================================================================
 */

#define VCR_MODE_VCR            0   /* Classic VCR tape look */
#define VCR_MODE_CCTV           1   /* Security camera style */

/*
 * =============================================================================
 *  QUALITY PRESETS
 * =============================================================================
 */

#define VCR_QUALITY_LOW      0   /* Minimal effects for very old GPUs */
#define VCR_QUALITY_MEDIUM   1   /* Balanced quality/performance */
#define VCR_QUALITY_HIGH     2   /* Full effect quality */

/*
 * =============================================================================
 *  CONFIGURATION - Adjust these values to tune the effect
 * =============================================================================
 */

/* Timing (in seconds) */
#define VCR_DISTORTION_INTERVAL     20.0f   /* Time between distortion spikes */
#define VCR_DISTORTION_DURATION      1.5f   /* Demo: 1.5s */
#define VCR_CCTV_DURATION            2.5f   /* CCTV high-contrast moment length */
#define VCR_CCTV_CHANCE              0.30f  /* Demo: 30% */

/* === SUBTLE VCR MODE (Client requested updates) === */
/* Normal state: */
#define VCR_NORMAL_NOISE_DOTS        18     /* User request: 18 dots */
#define VCR_NORMAL_GRAIN             0.04f  /* Demo: 4% */
#define VCR_NORMAL_DESATURATION      0.8f   /* Demo: 80% */

/* Spike state (every ~20 seconds): heavy effects */
#define VCR_SPIKE_NOISE_DOTS         40     /* User: Jumps to 40 dots */
#define VCR_SPIKE_DESATURATION       1.0f   /* User: 100% B&W (Gray) */
#define VCR_SPIKE_GRAIN              0.2f   /* Heavier grain during spike */
#define VCR_SPIKE_JITTER_MAX         5.0f   /* User: Screen shakes by 5 pixels */
#define VCR_SPIKE_COLOR_SHIFT        0.02f  /* User: RGB Split */

/* Legacy defines for compatibility */
#define VCR_BASE_NOISE_DOTS          VCR_NORMAL_NOISE_DOTS
#define VCR_DESATURATION             VCR_SPIKE_DESATURATION
#define VCR_SEPIA_TINT               0.05f
#define VCR_GRAIN_INTENSITY          VCR_NORMAL_GRAIN
#define VCR_SCANLINE_ALPHA           0.03f  /* Demo: 3% (visible but light) */

/* CCTV moment intensity - inherit new desaturation/noise ideals */
#define VCR_CCTV_CONTRAST            1.2f   /* Adjusted contrast */
#define VCR_CCTV_NOISE_DOTS          80     /* Demo: 80 dots */
#define VCR_CCTV_VIGNETTE            0.3f   /* Vignette darkness at edges */
#define VCR_CCTV_FLICKER_INTENSITY   0.15f  /* Brightness flicker amount */
#define VCR_CCTV_FLICKER_SPEED       8.0f   /* Flicker frequency */

/* VCR tracking lines */
#define VCR_TRACKING_LINE_HEIGHT     8      /* Height of tracking line band */
#define VCR_TRACKING_LINE_SPEED      50.0f  /* Pixels per second drift */

/* ============= FOUND FOOTAGE EFFECTS ============= */

/* REC indicator */
#define VCR_REC_BLINK_SPEED          1.0f   /* Blink frequency in seconds */
#define VCR_REC_SIZE                 12     /* Font size for REC text */

/* Timestamp overlay */
#define VCR_TIMESTAMP_ENABLED        1      /* Show date/time overlay */

/* Static bursts (random full-screen static) */
#define VCR_STATIC_CHANCE            0.02f  /* Chance per frame (2%) */
#define VCR_STATIC_DURATION          0.15f  /* Duration of static burst */
#define VCR_STATIC_INTENSITY         0.8f   /* How intense the static is */

/* Tape damage / horizontal corruption */
#define VCR_TAPE_DAMAGE_CHANCE       0.01f  /* Chance of tape damage per frame */
#define VCR_TAPE_DAMAGE_LINES        5      /* Number of corruption lines */
#define VCR_TAPE_DAMAGE_DURATION     0.3f   /* How long damage shows */

/* Frame drop / stutter */
#define VCR_FRAME_DROP_CHANCE        0.005f /* Chance of frame drop */
#define VCR_FRAME_DROP_DURATION      0.1f   /* Duration of freeze */

/* Chromatic aberration */
#define VCR_CHROMATIC_AMOUNT         3.0f   /* RGB split in pixels */

/* Night vision */
#define VCR_NIGHT_VISION_TINT_R      0.2f   /* Red component */
#define VCR_NIGHT_VISION_TINT_G      1.0f   /* Green component (dominant) */
#define VCR_NIGHT_VISION_TINT_B      0.2f   /* Blue component */
#define VCR_NIGHT_VISION_NOISE       30     /* Extra noise dots */
#define VCR_NIGHT_VISION_BLOOM       0.1f   /* Glow intensity */

/* Battery indicator */
#define VCR_BATTERY_LOW_THRESHOLD    0.2f   /* When to show low battery */
#define VCR_BATTERY_BLINK_SPEED      0.5f   /* Blink speed when low */


/*
 * =============================================================================
 *  PUBLIC API
 * =============================================================================
 */

/*
 * VCR_Init
 * 
 * Initialize the VCR effect system and register cvars.
 * Call once during renderer initialization.
 */
void VCR_Init(void);

/*
 * VCR_Shutdown
 * 
 * Clean up the VCR effect system.
 * Call during renderer shutdown.
 */
void VCR_Shutdown(void);

/*
 * VCR_DrawEffect
 * 
 * Render the VCR/CCTV effect overlay.
 * Call after scene rendering, before UI rendering.
 * 
 * Parameters:
 *   screen_width  - Current viewport width in pixels
 *   screen_height - Current viewport height in pixels
 *   time          - Current client time in seconds (e.g., cl.time or cls.realtime)
 */
void VCR_DrawEffect(int screen_width, int screen_height, float time);

/*
 * VCR_Enable / VCR_Disable / VCR_Toggle
 * 
 * Programmatic control of the effect.
 * These are alternatives to using the vcr_enabled cvar.
 */
void VCR_Enable(void);
void VCR_Disable(void);
void VCR_Toggle(void);

/*
 * VCR_IsEnabled
 * 
 * Returns non-zero if the effect is currently enabled.
 */
int VCR_IsEnabled(void);

/*
 * VCR_Reset
 * 
 * Reset the effect timing to start fresh.
 * Useful when entering a new story moment.
 */
void VCR_Reset(void);

/*
 * =============================================================================
 *  MODE CONTROL
 * =============================================================================
 */

/*
 * VCR_SetMode
 * 
 * Set the effect mode.
 * 0 = VCR (default)
 * 1 = CCTV
 * 2 = Found Footage (horror camcorder)
 * 3 = Night Vision
 */
void VCR_SetMode(int mode);
int VCR_GetMode(void);

/*
 * =============================================================================
 *  DEBUG / QA COMMANDS
 * =============================================================================
 */

/*
 * VCR_ForceDistortion
 * 
 * Force trigger a distortion spike immediately.
 * Useful for QA testing.
 */
void VCR_ForceDistortion(void);

/*
 * VCR_ForceCCTV
 * 
 * Force trigger a CCTV moment immediately.
 * Useful for QA testing.
 */
void VCR_ForceCCTV(void);

/*
 * VCR_ForceStatic
 * 
 * Force trigger a static burst immediately.
 */
void VCR_ForceStatic(void);

/*
 * VCR_ForceTapeDamage
 * 
 * Force trigger tape damage effect.
 */
void VCR_ForceTapeDamage(void);

/*
 * VCR_SetQuality
 * 
 * Set effect quality level.
 * 0 = Low (fallback for very old GPUs)
 * 1 = Medium (balanced)
 * 2 = High (full quality)
 */
void VCR_SetQuality(int level);

/*
 * VCR_GetQuality
 * 
 * Returns current quality level (0-2).
 */
int VCR_GetQuality(void);

/*
 * VCR_SetBattery
 * 
 * Set simulated battery level (0.0 to 1.0).
 * When low, shows battery warning indicator.
 */
void VCR_SetBattery(float level);


/*
 * =============================================================================
 *  CVAR DECLARATIONS (for external access if needed)
 * =============================================================================
 */

/* Forward declaration */
struct cvar_s;

/* Primary toggle */
extern struct cvar_s *vcr_enabled;           /* cvar_t* - Master on/off (0 or 1) */
extern struct cvar_s *vcr_quality;           /* cvar_t* - Quality level (0=low, 1=medium, 2=high) */
extern struct cvar_s *vcr_mode;              /* cvar_t* - Effect mode (0=VCR, 1=CCTV, 2=Found, 3=NV) */

/* Intensity controls */
extern struct cvar_s *vcr_desaturation;      /* cvar_t* - Desaturation amount (0.0-1.0) */
extern struct cvar_s *vcr_noise_dots;        /* cvar_t* - Base noise dot count */
extern struct cvar_s *vcr_grain_intensity;   /* cvar_t* - Film grain strength */
extern struct cvar_s *vcr_scanline_alpha;    /* cvar_t* - Scanline visibility */

/* Timing controls */
extern struct cvar_s *vcr_distortion_interval;  /* cvar_t* - Seconds between spikes */
extern struct cvar_s *vcr_distortion_duration;  /* cvar_t* - Spike duration */
extern struct cvar_s *vcr_cctv_chance;          /* cvar_t* - CCTV moment probability */

/* Optional features */
extern struct cvar_s *vcr_tracking_lines;    /* cvar_t* - Enable tracking lines (0 or 1) */
extern struct cvar_s *vcr_rec_indicator;     /* cvar_t* - Show REC indicator (0 or 1) */
extern struct cvar_s *vcr_timestamp;         /* cvar_t* - Show timestamp (0 or 1) */
extern struct cvar_s *vcr_static_bursts;     /* cvar_t* - Enable random static (0 or 1) */

/* Debug controls */
extern struct cvar_s *vcr_debug;             /* cvar_t* - Show debug info (0 or 1) */


#ifdef __cplusplus
}
#endif

#endif /* VCR_EFFECT_H */
