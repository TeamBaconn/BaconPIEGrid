# Bacon PIE Grid

Unreal Engine editor plugin that docks multiplayer PIE clients into a grid over the play viewport. Clients that open in their own windows are tiled onto the same grid, and you can switch between clients with Ctrl+Tab.

## Features

- Docks in-process PIE clients into a grid over the play viewport when you play in "Selected Viewport" with more than one client.
- Tiles client windows onto the same grid cells: separate-process clients (Run Under One Process off), Standalone Game, and PIE clients that aren't docked.
- Picks a near-square layout automatically, or uses a fixed number of columns.
- Can letterbox every client to a fixed aspect ratio, such as 16 : 9.
- Can size the grid against the level editor's panel area instead of the play viewport. The editor layout is put back when play stops.
- Client switcher: hold Ctrl and tap Tab to move the highlight, then release Ctrl to switch. Shift goes backwards and Escape cancels.

## Requirements

- Unreal Engine 5.5 (other versions untested).
- A C++ project, since the plugin ships as source.

It's an editor-only module, so nothing ends up in packaged builds. Developed on Windows with a UE 5.5 source build.

## Installation

1. Clone or copy this repository into your project as `Plugins/BaconPIEGrid`.
2. Add the plugin's editor engine to `Config/DefaultEngine.ini`:

   ```ini
   [/Script/Engine.Engine]
   UnrealEdEngine=/Script/BaconPIEGrid.BaconPIEGridEditorEngine
   ```

3. Regenerate project files and build the editor.

The plugin hooks into PIE through this editor engine class, so it can't be combined with another plugin or project that sets its own `UnrealEdEngine`.

## Usage

1. In the play options next to the Play button, set Multiplayer Options > Number of Players to 2 or more.
2. Play in "Selected Viewport". The clients dock into a grid over the play viewport.
3. With Run Under One Process off, or with Standalone Game, the client windows are placed on the grid cells instead.
4. While one of the clients has focus, press Ctrl+Tab to switch between them.

## Settings

Editor Preferences > Level Editor > PIE Grid. Settings are saved per user.

| Setting | Default | What it does |
|---|---|---|
| Dock Clients | On | Docks in-process clients into the grid. Applies to "Selected Viewport" play with more than one client. |
| Play Screen Size Ratio | Off, 1 : 1 | When ticked, sizes the grid against this width : height share of the level editor's panel area instead of the play viewport. Below 1, the panels next to the play viewport are pushed aside to make room. 1 : 1 covers the whole panel area, with the panels and drawers hidden behind the grid. |
| Tile Process Windows | On | Places client windows that aren't docked on the grid cells. With Dock Clients off, the first client stays in the level viewport, shrunk to the first cell, and the other windows follow the viewport as it moves or resizes. |
| Columns | 0 | Number of grid columns. 0 picks the layout automatically. |
| Cell Padding | 2 | Space around each cell. |
| Client Aspect Ratio | Off, 16 : 9 | When ticked, letterboxes every client to this width : height inside its cell. Separate-process windows get the same shape. |
| Show Cell Labels | On | Labels each cell with its client, such as "Server 0" or "Client 1". |
| Switch Client Chord | Ctrl+Tab | Shortcut for the client switcher. |

## License

MIT. See [LICENSE](LICENSE).
