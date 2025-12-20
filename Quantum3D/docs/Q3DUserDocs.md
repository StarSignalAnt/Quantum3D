# Quantum 3D User Guide

> [!WARNING]
> **Pre-Alpha Software**
> Quantum 3D is currently in pre-alpha development. This documentation covers features that are implemented, but the application is not yet complete. Some features described here may be partially implemented, and additional features will be added in future releases.

---

## Table of Contents

1. [Overview](#overview)
2. [User Interface](#user-interface)
3. [Viewport Navigation](#viewport-navigation)
4. [Selecting Objects](#selecting-objects)
5. [Transform Gizmos](#transform-gizmos)
6. [Importing 3D Models](#importing-3d-models)
7. [Lighting](#lighting)
8. [QLang Scripting](#qlang-scripting)
9. [Script Editor](#script-editor) *(New in v0.3)*
10. [Play Mode](#play-mode)
11. [Keyboard Shortcuts Reference](#keyboard-shortcuts-reference)
12. [Troubleshooting](#troubleshooting)
13. [Changelog](#changelog)

---

## Overview

Quantum 3D is a 3D scene editor built on the Quantum Engine. It provides a viewport for viewing and manipulating 3D scenes, along with tools for importing models, transforming objects, and managing scene hierarchy.

**Key Features:**
- Real-time Vulkan-based 3D rendering
- Transform gizmos (translate, rotate, scale)
- Model importing (FBX, OBJ, glTF, GLB)
- QLang scripting for game logic
- **IntelliSense-style code completion** *(New in v0.3)*
- Play mode for testing scripts

---

## User Interface

The application window is divided into several key areas:

### Main Menu
Located at the top of the window with the following menus:
- **File** — File operations (coming soon)
- **Edit** — Editing commands (coming soon)
- **View** — View options (coming soon)
- **Help** — Help and documentation (coming soon)

### Toolbar
Located below the menu bar, the toolbar provides quick access to common tools:

| Icon | Tool | Shortcut | Description |
|------|------|----------|-------------|
| ![Local](icons/local.png) | **Local Mode** | — | Transform objects using local coordinates |
| ![Global](icons/global.png) | **Global Mode** | — | Transform objects using world coordinates |
| ![Translate](icons/translate.png) | **Translate** | `F1` | Move selected objects |
| ![Rotate](icons/rotate.png) | **Rotate** | `F2` | Rotate selected objects |
| ![Scale](icons/scale.png) | **Scale** | `F3` | Scale selected objects |
| ![Play](icons/play.png) | **Play** | — | Start play mode |
| ![Stop](icons/stop.png) | **Stop** | — | Stop play mode |

### Viewport
The central 3D viewport where you view and interact with your scene. This is a fully rendered 3D view powered by the Vulkan graphics API.

### Browser Panel
Located at the bottom of the window. This panel lets you browse files on your system (defaults to `c:\qcontent\`). Use it to navigate folders and import 3D models into your scene.

**Features:**
- Folder navigation with double-click
- Image thumbnail previews for supported formats
- Back button navigation (mouse button 4)

### Scene Graph Panel
A hierarchical view of all objects in your scene (coming soon).

### Properties Panel
Displays and allows editing of properties for selected objects (coming soon).

---

## Viewport Navigation

### Camera Movement

Hold **Right Mouse Button** to enable camera look mode. While holding:

| Key | Action |
|-----|--------|
| `W` | Move forward |
| `S` | Move backward |
| `A` | Move left |
| `D` | Move right |
| `E` | Move up |
| `Q` | Move down |
| **Mouse** | Look around (while right-click held) |

> [!TIP]
> The cursor becomes hidden and allows infinite movement while in look mode. Release the right mouse button to restore normal cursor behavior.

### Special Keys

| Key | Action |
|-----|--------|
| `Space` | Move main light to camera position |
| `T` | Move secondary light to camera position |

---

## Selecting Objects

**Left Click** on any object in the viewport to select it. When selected:
- A transform gizmo appears at the object's position
- The object name is logged to the console
- You can use the transform tools to modify the object

**Left Click** on empty space to clear the selection.

---

## Transform Gizmos

When an object is selected, a transform gizmo appears for manipulating the object.

### Gizmo Types

| Gizmo | Shortcut | Description |
|-------|----------|-------------|
| **Translate** | `F1` | Displays axis arrows for moving objects along X, Y, or Z axis |
| **Rotate** | `F2` | Displays rotation rings for rotating objects around X, Y, or Z axis |
| **Scale** | `F3` | Displays handles for scaling objects along X, Y, or Z axis |

### Coordinate Modes

- **Local** — Gizmo axes align with the object's local orientation
- **Global** — Gizmo axes align with world X, Y, Z directions

Use the toolbar buttons to switch between Local and Global coordinate modes.

### Using the Gizmo

1. Select an object (left click)
2. Choose your transform tool (F1, F2, or F3)
3. Click and drag on a gizmo axis to transform along that axis
4. Release to confirm the transformation

---

## Importing 3D Models

### Supported Formats
- **FBX** (.fbx)
- **OBJ** (.obj)
- **glTF** (.gltf)
- **GLB** (.glb)

### How to Import

1. Use the **Browser Panel** to navigate to your model files
2. **Double-click** on a folder to enter it
3. **Double-click** on a supported 3D model file to import it
4. The model will be added to your scene under the root node

> [!NOTE]
> Use the mouse back button (mouse button 4) in the Browser to go back to the previous folder.

---

## Lighting

The scene includes dynamic point lights for real-time lighting preview:

- **Main Light** — Primary light source (white light)
- **Secondary Light** — Additional light source (cyan tint)

Use the `Space` and `T` keys to reposition lights to the camera location for easy lighting adjustments.

---

## QLang Scripting

Quantum 3D supports QLang scripting for adding custom game logic to scene nodes.

### Overview

QLang is a statically-typed scripting language designed for the Quantum Engine. Scripts can be attached to nodes to control their behavior during play mode.

### Built-In Classes

| Class | Description |
|-------|-------------|
| `Vec3` | 3D vector with operator overloading (+, -, *, /) and utility methods |
| `Matrix` | 4x4 transformation matrix with identity, scale, rotate, translate |
| `GameNode` | Base class for game objects with lifecycle hooks |

### GameNode Lifecycle

Scripts extending `GameNode` can override these methods:

| Method | When Called |
|--------|-------------|
| `OnPlay()` | When play mode starts |
| `OnUpdate(float32 dt)` | Every frame (dt = delta time in seconds) |
| `OnRender()` | During rendering |
| `OnStop()` | When play mode stops |

### Example Script

```
class Player(GameNode)
    float32 speed = 5.0

    method void OnUpdate(float32 dt)
        Vec3 move = new Vec3(speed * dt, 0, 0)
        Vec3 pos = GetPosition()
        SetPosition(pos + move)
    end
end
```

### Transform Methods

| Method | Description |
|--------|-------------|
| `Turn(Vec3 rotation)` | Rotate the node by degrees (Euler angles) |
| `SetPosition(Vec3 position)` | Set the node's world position |
| `GetPosition()` | Get the node's current position |

> [!TIP]
> See the [QLang User Guide](../QLang/docs/QLang_User_Guide.md) for complete language documentation.

---

## Script Editor

*New in v0.3*

Quantum 3D includes a built-in Script Editor for writing and editing QLang scripts with IDE-like features.

### Opening the Script Editor

Double-click on a `.q` file in the Browser panel to open it in the Script Editor.

### IntelliSense Features

The Script Editor provides intelligent code completion:

#### Dot Completion

When you type `.` after a variable, the editor shows available members and methods:

```
Vec3 pos = new Vec3(1, 2, 3)
pos.  // Shows: X, Y, Z, Plus(), Minus(), Cross(), etc.
```

#### Icon Indicators

Completion items show icons to indicate their type:
- **Blue circle (●)** — Member variables (e.g., X, Y, Z)
- **Purple circle (●)** — Methods (e.g., Plus(), Minus())

#### Auto-Complete

The editor automatically loads engine classes (`Vec3`, `Matrix`, `GameNode`) and shows their members in dot-completion.

### Live Compilation

The Script Editor compiles your code as you type (after a brief delay) and shows errors in the console panel below the editor.

### Console Output

The bottom panel shows:
- Compilation status ("Compiled OK" or error messages)
- Line numbers for any errors
- Debug output when using dot-completion

---

## Play Mode

Play mode allows you to test your QLang scripts in real-time.

### Starting Play Mode

1. Click the **Play** button in the toolbar
2. All nodes with attached QLang scripts will receive `OnPlay()` calls
3. Scripts will receive `OnUpdate(dt)` calls every frame
4. The scene will run your game logic in real-time

### Stopping Play Mode

1. Click the **Stop** button in the toolbar
2. All scripts will receive `OnStop()` calls
3. The scene returns to edit mode

### Delta Time

The `OnUpdate` method receives a `dt` (delta time) parameter representing the time in seconds since the last frame. Use this for frame-rate independent movement:

```
// Move 5 units per second, regardless of frame rate
float32 speed = 5.0
Vec3 move = new Vec3(speed * dt, 0, 0)
```

---

## Keyboard Shortcuts Reference

| Shortcut | Action |
|----------|--------|
| `F1` | Translate tool |
| `F2` | Rotate tool |
| `F3` | Scale tool |
| `W` | Move camera forward (with RMB) |
| `S` | Move camera backward (with RMB) |
| `A` | Move camera left (with RMB) |
| `D` | Move camera right (with RMB) |
| `E` | Move camera up (with RMB) |
| `Q` | Move camera down (with RMB) |
| `Space` | Move main light to camera |
| `T` | Move secondary light to camera |

---

## Troubleshooting

### Model Not Appearing
- Verify the model file is in a supported format
- Check that the file path contains no special characters
- Models may be imported at a very large or very small scale

### Viewport Not Rendering
- Ensure your graphics card supports Vulkan
- Update your graphics drivers to the latest version

### Gizmo Not Responding
- Make sure an object is selected (click on it first)
- Try clicking directly on the gizmo axis arrows/rings

### Scripts Not Running
- Ensure play mode is active (click Play button)
- Check console output for QLang parsing errors
- Verify your script extends `GameNode`

### Dot-Completion Not Working
- Ensure the file has been compiled successfully (check for "Compiled OK")
- The variable must have a known type (Vec3, Matrix, or user-defined class)
- You must be inside a class and method for context detection

---

## Changelog

### v0.3 (20.12.25)

**New Features:**
- **IntelliSense Dot-Completion**: Type `.` after a variable to see available members and methods
- **Typed Completion Icons**: Blue circles for member variables, purple circles for methods
- **Engine Class Auto-Loading**: Vec3, Matrix, and other engine classes are automatically registered for IntelliSense
- **Live Symbol Collection**: Classes defined in your script are available for completion after compilation
- **Debug Console Output**: Script Editor console shows IntelliSense debugging information

**Improvements:**
- Script Editor now connects compilation to IntelliSense for real-time code assistance
- Improved class context detection for accurate member lookup
- Completion popup properly sorted: member variables first, then methods
- Wider completion popup to accommodate icon display

**Bug Fixes:**
- Fixed `getCurrentClassName()` incorrectly clearing class context on method `end` keywords
- Fixed engine class path resolution when app runs from different directories

### v0.2 (19.12.25)

**New Features:**
- **QLang Scripting Integration**: Full support for QLang scripts on scene nodes
- **Play Mode**: Test scripts in real-time with Play/Stop controls
- **Delta Time**: Accurate frame-time calculation for smooth, frame-rate independent updates
- **Operator Overloading in QLang**: Vec3 supports +, -, *, / operators
- **Enhanced Vec3 Class**: Added Dot, Cross, Lerp, Negate, and factory methods
- **New Native Functions**: `NodeSetPosition`, `NodeTurn` for script-to-engine communication

**Improvements:**
- Camera movement now uses real delta time instead of fixed time step
- Image thumbnail previews in the Browser panel
- Improved error reporting for QLang scripts

**Bug Fixes:**
- Fixed parser false positive errors with parenthesized expressions
- Fixed literal handling for integers, floats, and booleans in QLang
- Improved constructor error messages

### v0.1 (Initial Release)

- Core 3D viewport with Vulkan rendering
- Transform gizmos (translate, rotate, scale)
- Model importing (FBX, OBJ, glTF, GLB)
- Scene graph with hierarchical nodes
- Dynamic point lighting
- Browser panel for file navigation
- Keyboard camera controls

---

> [!IMPORTANT]
> **Feedback Welcome**
> As pre-alpha software, your feedback helps improve Quantum 3D. Please report any bugs or feature requests to the development team.

