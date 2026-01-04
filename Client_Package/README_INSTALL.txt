HOW TO INSTALL & FIX "LARGE SCREEN" ISSUE
=========================================

The game failed to run because `gamex86.dll` was likely in the wrong place.
Quake 2 requires the DLL to be INSIDE the `baseq2` folder.

INSTALLATION STEPS
------------------
1. You will see a `baseq2` folder in this package.
2. Open your existing Quake 2 game folder.
3. Open the `baseq2` folder inside your game.
4. COPY the `gamex86.dll` from this package into your game's `baseq2` folder.
   (Replace the existing file if asked).

5. Go back to your main Quake 2 folder (where quake2.exe is).
6. COPY `q2pro_update.exe` here.

7. Run `q2pro_update.exe`.

FIXING "ALL LARGE" / SCREEN ISSUES
----------------------------------
If the menu is too large or the resolution is wrong:

1. Open the console by pressing the tilde key (`~`) (below Esc).
2. Type these commands one by one and press Enter:

   vid_fullscreen 0
   r_mode 4
   vid_restart

   (This switches to Windowed Mode, 800x600 resolution)

3. If you want 1920x1080 fullscreen:

   r_mode -1
   r_customwidth 1920
   r_customheight 1080
   vid_fullscreen 1
   vid_restart
