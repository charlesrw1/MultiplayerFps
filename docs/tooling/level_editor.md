+++
order = -4
+++
# Level Editor

The level editor is the main map-creation tool. Launch it with `.\Scripts\run_editor.ps1` or `App.exe --editor`.

## Camera

1. Hold `Right Mouse` to use the FPS camera.
2. Use `WASD` to move. Use the mouse to look.
3. Use `Mouse Wheel` to change camera speed. With `Right Mouse` held, the wheel also changes speed or orthographic zoom.
4. Press `Z` to move up on the Y-axis. Press `X` to move down.
5. Hold `Middle Mouse` to orbit around the target point.
6. Press `F` with an object selected to set the orbit target.
7. Hold `Shift` and `Middle Mouse` to pan.
8. Press Numpad `1`, `7`, or `3` for an orthographic view. Hold `Ctrl` with those keys for the opposite axis.
9. Press Numpad `5` for a perspective view.

## Gizmos

The gizmos use Blender-style keys. Select one or more objects first.

1. Press `G` for screenspace translation.
2. Press `R` for screenspace rotation.
3. Press `S` for uniform scale.
4. Press `X`, `Y`, or `Z` after a gizmo tool to lock that axis. Hold `Shift` with an axis key to lock the plane.
5. Press `Left Mouse` to confirm. Press `Right Mouse` to cancel.

## Selection

1. Click `Left Mouse` to select an object. Drag `Left Mouse` to box-select.
2. Hold `Shift` and click to add to the selection.
3. Hold `Ctrl` and click to remove from the selection.
4. Press `Ctrl+A` to select all. Press `Ctrl+I` to invert the selection.
5. Press `Delete` to delete the selection.
6. Press `Shift+D` to duplicate the selection. Hold `Ctrl` and drag a transform tool to duplicate and transform in one step.
7. Press `H` to hide the selection. Press `Alt+H` to unhide all.

## Document and Undo

1. Press `Ctrl+Z` to undo.
2. Press `Ctrl+S` to save.
3. Press `Ctrl+Tab` to cycle previous documents.

## Parenting and Context Menu

1. Press `Right Mouse` on selected entities to open the context menu. Use it to parent entities in prefab mode, or to unpack prefabs.
2. Press `Ctrl+P` to parent the first selected entity to the second selected entity.

Parenting works only in prefab mode. Instantiating a prefab removes parenting. See [[gotchas]].

## Other Shortcuts

- Press `T` to toggle entity text labels.
- Press `,` to toggle the debug menu.
- Press `.` to toggle the console.

## Menu Bar

- `File > Restore Backup...` opens a list of automatic backups for the current file.
- `Tools > Asset Size Viewer` shows a tree graph of asset file sizes in the active data directory. Tile size matches file size. Color marks asset type. The view groups files by folder. It includes compiled assets only.
- `Tools > Diagnostics` shows errors on assets or map entities, such as missing fields.
- `Tools > Profiler` opens the CPU profiler.

## Scene Menu Bar

- Magnet button: enable or disable snapping. Click the arrow to change snap settings.
- Coordinate icon: switch the gizmos between global and local coordinates.
- Box handles: Off, Faces, or Edges. Faces add a handle on each bound of the selection. Drag a handle to expand the bounds. This changes position and scale. Edges place handles on the bounding-box edges, so you can drag two axes at once.
- T icon: toggle entity text labels (same as `T`).
- Lit label: change scene debug render modes.
- OT: Off label: overlay an internal render texture.

## Asset Browser

1. Click the green upload button to import `.glb` models.
2. Use the Grid checkbox to switch between grid and list mode. Grid mode shows a thumbnail when one exists.
3. Select folders on the left. Expand or collapse them.
4. Use the filter button to filter by asset type.
5. Right-click an asset for a popup:
   - Copy the in-engine asset path to the clipboard.
   - Open in Notepad for text assets such as textures, models, and materials.
   - Show in Explorer to open the containing folder.
   - View References to open the reference viewer.
   - Create New... to create an asset in that folder.
6. Drag Lua or C++ components, models, and prefabs from the browser into the scene to place them.
7. Click an asset to select it. The Asset Inspector shows its import settings.

## Outline Filter

Use the outline filter to hide or show entities by type.
