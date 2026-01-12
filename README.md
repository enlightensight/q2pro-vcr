# VCR / CCTV Effect Integration

This folder contains the complete Q2Pro engine source code with the **VCR/CCTV Effect** fully integrated.

---

## 📁 Files Added

| File | Description |
|:-----|:------------|
| `src/refresh/gl/vcr_effect.c` | Main VCR/CCTV effect implementation (~1100 lines) |
| `src/refresh/gl/vcr_effect.h` | Header file with public API and configuration defines |

---

## 🎬 Effect Sequence (50-Second Loop)

The effect runs on a **50-second repeating cycle** with the following events:

| Time | Event |
|:-----|:------|
| **Constant** | 8% Black & White desaturation, 20 white noise dots |
| **10s - 15s** | 2 tracking lines scroll down the screen (~5 seconds) |
| **20s - 22s** | Screen shake effect (2 seconds) |
| **30s - 32s** | Increased noise dots (+100 dots for 2 seconds) |
| **40s - 42s** | 50% Black & White desaturation (2 seconds) |

### Timestamp Overlay
- Displays **current system time** (month, day, hour, minute, second)
- Year is **hardcoded to 2007** for retro aesthetic

---

## 📦 Build Outputs

| File | Purpose |
|:-----|:--------|
 **`q2pro_update.exe`** | The game client with VCR effect integrated | to check open it/ type map demo1/ after game open press " ` " key to open console commands.
| **`gamex86.dll`** | Game logic module (x86 build) |
| `zlib1.dll`, `libpng16-16.dll`, etc. | Runtime dependencies |

---

## 🚀 How to Run

1. **IMPORTANT:** The `baseq2/` folder included here is for **directory structure testing only** - it does NOT contain actual game assets.
2. Copy your existing Quake 2 `baseq2` folder (containing `pak0.pak` and game assets) into this directory.
3. Run `q2pro.exe` or `q2pro_update.exe`.
4. The VCR effect is **enabled by default**.

---

## 🎮 Console Commands

| Command | Description | Default |
|:--------|:------------|:--------|
| `vcr_enabled 1/0` | Enable/disable the effect | `1` (On) |
| `vcr_mode 0` | VCR Mode (battery, REC indicator, timestamp) | `0` |
| `vcr_mode 1` | CCTV Mode (timestamp only) | |
| `vcr_tracking_lines 1/0` | Enable/disable tracking lines | `1` |
| `vcr_timestamp 1/0` | Show/hide timestamp overlay | `1` |

---

## 🔧 Modified Engine Files

1. **`src/refresh/gl/main.c`** - Hooked `VCR_Init()` and `VCR_DrawEffect()` into renderer
2. **`Makefile`** - Added `vcr_effect.o` to build list

---

## 🛠️ Building from Source

### Cross-compile (Linux to Windows x86):
```bash
bash build_linux_cross.sh
```

### Native Windows (MSYS2/MinGW):
```bash
bash build_x86.sh
```

---

## � Configuration

All effect parameters can be adjusted in `vcr_effect.h`:

```c
#define VCR_NORMAL_NOISE_DOTS        18     // Constant dot count
#define VCR_SPIKE_JITTER_MAX         5.0f   // Shake intensity
#define VCR_TRACKING_LINE_SPEED      50.0f  // Scroll speed
```

---

## 👤 Author

**enlightensight** - VCR/CCTV Effect Implementation
