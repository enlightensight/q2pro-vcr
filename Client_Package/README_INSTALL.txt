IMPORTANT: HOW TO FIX "MISSING DLL" AND "LARGE SCREEN"
======================================================

The error "missing gamex86.dll" happens because the file is in the wrong place.
Please follow these steps EXACTLY:

STEP 1: INSTALL THE EXE
-----------------------
1. Find `q2pro_update.exe` in this package.
2. Copy it into your MAIN Quake 2 folder (where `quake2.exe` usually is).

STEP 2: INSTALL THE DLL (CRITICAL)
----------------------------------
1. Find the `baseq2` folder in this package.
2. Open it. You will see `gamex86.dll`.
3. Copy `gamex86.dll` into your GAME'S `baseq2` folder.
   (The full path should look like: `C:\Quake2\baseq2\gamex86.dll`)

   NOTE: If it asks to replace a file, say YES.

STEP 3: RUN THE GAME
--------------------
Run `q2pro_update.exe` (NOT quake2.exe).

STEP 4: FIX SCREEN / RESOLUTION
-------------------------------
If the screen is zoomed in or too large:

1. Press the `~` key (top left, below Esc) to open the console.
2. Type exactly this and press ENTER:

   vid_fullscreen 0; r_mode 4; vid_restart

   (This resets it to a standard 800x600 window).

3. To set it to 1920x1080 Fullscreen:

   r_mode -1; r_customwidth 1920; r_customheight 1080; vid_fullscreen 1; vid_restart
