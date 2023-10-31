# ROTTR-mod-loader  (a modification of TRAE-menu-hook, no menu)

## What is included: 
This is an expansion of ThelIndra55's TRAE-menu-hook  (https://github.com/TheIndra55/TRAE-menu-hook) to enable ROTTR loading of external mod file.
It does not offer any menu, it load mod files (AlphaZomega's style ROTTR mod files https://www.nexusmods.com/riseofthetombraider/mods/20?tab=files) (*.tr2mesh *.tr2pcd) into game on game start, no need to install these files to update tiger archive.
This currently only works for steam build 1026 and the current stream build (1027 ???).

## Installation:
- copy d3d12.dll and ROTTR-Menu-Hook.asi from bin\ direcotry to ROTTR installation directory.
- create a mods\ subdirectory under ROTTR installation directory

## To convert existing mods:
- Get the mod_prep_tools\ and contents folder from this github
- Assumptions: all .tr2mesh and .tr2pcd  file conatins section number in their file name.
- Copy  ??????.drm and the corresponding *.tr2mesh *.tr2pcd file to the mod_prep_tools\ directory
- switch to ROTTR_mod_prep_tools\mod_prep_tools\ dir, run  convert.cmd  <drm file name>
- if all goes well, you will get a mod_out\ folder contain the renamed assets. 
- rename mod_out\ to something like 00001_<mod_name>\ and move this directory inside <ROTTR_game_dir>/mods/
- you can have more than one  000??_<mod_name> inside mods/. The bigger 000?? number mods override the lowest number if they contain the same asset file name
 
## Limited support of in-game mod reload: 
1. load a save game  where Lara is near a camp fire.
2. After add/replace/delete some mod files, immediately press "ESC" and select "reload checkpoint" to re-sync internal mod list. Game will crash if forget to reload.
3. New additions (mesh/texture) do not shows up right away. use the camp fire and switch to another costume and switch back to see the added sections.
4. There are still some sequences of add/remove mods causing a reload to crash, haven't pinned down the exact sequence to reproduce problem.

## Uninstall a mods:
delete the 000??_<mod name> folder

## Uninstall hook
delete  d3d12.dll and ROTTR-menu-hook.asi



# TRAE-menu-hook

Reverse engineering project for Tomb Raider LAU games, this repo contains the code for TRAE-menu-hook menu for Tomb Raider Anniversary, Legend and Underworld.

[![](https://github.com/TheIndra55/TRAE-menu-hook/actions/workflows/build.yml/badge.svg)](https://github.com/TheIndra55/TRAE-menu-hook/actions/workflows/build.yml)

## Videos

(screenshots at bottom of readme)

| [![](https://i.imgur.com/RMnCPck.png)](https://www.youtube.com/watch?v=orv2mYjBNhM) | [![](https://i.imgur.com/qgDQuio.png)](https://www.youtube.com/watch?v=k1FIa8Pel3E&t) | [![](https://i.imgur.com/cJe3pYa.png)](https://www.youtube.com/watch?v=RwU80Pj0PR8) | [![](https://i.imgur.com/MEPYZBD.png)](https://www.youtube.com/watch?v=BfUCcC6z1jo) | [![](https://i.imgur.com/8HvLmPG.png)](https://www.youtube.com/watch?v=JvH0RPbaUI0) |
|----|----|----|----|----|
| Menu in Underworld | Playtrough with level debug prints | Instance viewer showcase | Loading .drm files without bigfile | Menu in Anniversary |

## Features

* Skew/flight cheat
* Free camera
* Switch player character
* Load files from outside the bigfile
* View and manipulate instances
* Spawn instances
* Level debug restored
* Switch levels
* Show level collisions, markup, portals, triggers (signals), enemy navigation
* Disable death fade/respawn
* Disable cinematic bars
* Slow down game (slow motion)
* Wireframe rendering

## Fixes/improvements

* Game no longer crashes while DEP is enabled
* Game legal screen can be skipped

## Build

Make sure you have [premake5](https://premake.github.io/) installed and in your PATH.

1. Clone the repository and all submodules
2. Include [MinHook.x86.lib](https://github.com/TsudaKageyu/minhook/releases) in the folder.
3. Run `premake5 vs2019` in your terminal.
4. Open the generated solution (.sln) and build for the right game.

## FAQ

### How to load files outside bigfile

Create a folder named 'mods' in your game folder, inside any files can be placed the game should load as replacement. The folder structure in the 'mods' folder is the same as in the PC-W folder in the bigfile.

### How to add new objects

Start by adding the .drm file of your object to the mods folder, next copy the orginal objectlist.txt to mods\(game codename)\pc-w\ and open it in notepad.
Now add a new line with your object name and the new number.
```
959,yourobjectwithoutdotdrm
```

Go back to the start of the file and increase the first number and save the file.

**Note** objectlist.txt must always end with a line ending (empty line) else the game will crash on start.

### Controls

<kbd>F7</kbd> Hide the menu \
<kbd>F8</kbd> Toggle menu focus, this allows you to interact with the menu \
<kbd>F9</kbd> Switch player character \
<kbd>F11</kbd> Instant ragdoll death

<kbd>F2</kbd> Toggle skew/flight cheat \
<kbd>F3</kbd> Freeze the game \
<kbd>Shift</kbd> + <kbd>F3</kbd> Cycle Slow motion \
<kbd>F4</kbd> Toggle free camera \
<kbd>F5</kbd> Toggle player control \
<kbd>Insert/Ins</kbd> Toggle hud

<kbd>Q</kbd> \/ <kbd>A</kbd> Flight up \
<kbd>Z</kbd> \/ <kbd>W</kbd> Flight down

Flight controls depend on QWERTY/AZERTY keyboard layout

## Thanks to

* SunBeam906
* Xwilarg

## Screenshots

![image](https://user-images.githubusercontent.com/15322107/111395421-1e496700-86bd-11eb-997b-b73f2a3ec244.png)
![image](https://user-images.githubusercontent.com/15322107/113633351-19992280-966d-11eb-9924-27cb87a3830f.png)
![image](https://user-images.githubusercontent.com/15322107/112666334-5ccce780-8e5c-11eb-8592-4ccc47627dba.png)
