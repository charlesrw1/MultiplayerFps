+++
order = -4
+++
# Level Editor

## Interface Overview

![Editor Interface](tooling/screenshot1.png)

## Controls

- Camera can be controlled with an FPS-like camera and orbit camera. Hold `Right Mouse` to go into the FPS camera, use `WASD` controls to move around and use mouse to look around. Use `Mouse Wheel` to change camera speed. `Z` to go up on Y-axis, `X` to go down on Y-axis. Hold `Middle Mouse` to go into orbit camera mode. It orbits around the set target point. The target point can be set by pressing `F` with an object selected. Hold `Shift` and `Middle Mouse` to pan the camera around. 

- Uses Blender-style keybinds for translation/rotation/scale gizmos. With object(s) selected: press `G` to activate screenspace translation, press `R` for screenspace rotation, or press `S` for uniform scale. Press `Right Mouse` to cancel, `Left Mouse` to confirm. After activating one of the gizmo tools, you can also press `X`, `Y`, and `Z` to only translate/rotate/scale about that axis. Press `Shift` along with one of the axis keys to translate about that plane.

- `Ctrl+Z` to undo last command.

- `Ctrl+S` to save document.

- `Ctrl+Tab` to change current document to previous documents.

- Use keypad keys to set camera into orthographic mode. 

- `T` to toggle text labels on entities. 

- `Delete` to delete current selection.

- `Shift+D` to duplicate and select current selection.

- `Right Mouse` to open context menu of selected entities. Use this to parent entities (in prefab mode) or other actions like unpacking prefabs.

- `Ctrl+P` to parent first selected entity to second selected entity.

- `Mouse Left` to select an object. Drag the mouse with `Mouse Left` to box select. Hold `Shift` to add to selection. Hold `Ctrl` to remove from selection. 

- `Ctrl+A` to select all. `Ctrl+I` to invert selection.

- `H` to hide selected. `Alt+H` to unhide all.

- `,` to toggle open debug menu.

- `.` to toggle open console.

## Menu Bar

- Use `File>Restore Backup...` to open a window where you can select a past automatic backup to reload current file.

- Use `Tools>Asset Size Viewer` to open a tree graph of asset file sizes in the active data directory, where tile size corresponds to file size. Color coded according to asset type, and grouped by folders. Only includes compilied assets.

- Use `Tools>Diagnostics` to open a window that can display some errors related to assets or map entities like missing fields.

- Use `Tools>Profilier` to open the profilier. See [[profilier|Profilier]] for information.

## Scene Menu Bar

- Use the magnet button to enable/disable snapping. Click the arrow to change the snap settings. 

- Use the coordinate icon to enable global/local coordinates on the gizmo tools.

- Use the box handles button to select between 3 modes: off, faces, edges. Faces add a handle to the bounds of the selected object that let you drag to expand it, which modifies the position and/or scale to move the bounding box. Edges is similar, but places the handles on the edges of the bounding box, letting you manipulate in 2 axis.

- Use the T icon to toggle text labels on entities (shortcut `T` key). 

- Use the "Lit" label to change between scene debug render modes. 

- Use the "OT: Off" label to select between an internal render texture to display.

## Asset Browser

- Use the green "upload" button to select .glb model(s) to import. 

- Use the "Grid" checkbox to enable grid vs list mode. Grid mode displays assets with a thumbnail if available.

- The folders on the left can be selected and/or expanded/collapsed. 

- Use the filter button to select a type filter.

- Right click on an asset to open a popup. You can copy the asset path (the path you can use in-engine) to the OS clipboard. Use "Open in notepad" on some assets like textures, models, materials, to open the text file in the OS text editor. Use "Show in Explorer" to open the containg folder in the OS explorer. Use "View References" to open reference viewer. Use "Create New..." to create a new asset in that folder. 

- Some assets allow drag-drop into the scene. Drag-dop Lua/C++ components, models, and prefabs to place them into the scene.

- Click on an asset to select it, and it opens in the Asset Inspector window for editing its import settings.

## Outline Filter

Lets you hide/show entities of certain types.
