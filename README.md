# RancWorldLayers Plugin

A plugin for managing and visualizing 2D data layers mapped to the 3D world.

## Core Concepts

### World Data Volume
The `AWorldDataVolume` defines the spatial bounds for all data layers in a level. Exactly one volume should exist per level.

### World Data Layer Asset
Defines the configuration for a data layer, including resolution, format, and mutability.

## Debug Tooling

### Automatic Setup
The plugin automatically spawns a transient `AWorldLayersDebugActor` whenever a `WorldDataVolume` is initialized. This actor manages the UI overlay and the 3D visualization. It automatically discovers any Widget Blueprint inheriting from `WorldLayersDebugWidget` to use as the overlay.

### 2D Debug Overlay
Visualize active data layers directly in the viewport.

- **Toggle Mode:** `Ctrl + NumPad 0`
  - Cycles through: Hidden -> Mini-map -> Full-screen.
- **Select Layer:** `Ctrl + NumPad 1-9`
  - Switches the displayed texture to the corresponding active WorldLayer.

### 3D In-World Visualization
Render data layers as a floating plane in the 3D world.

- **Toggle 3D Plane:** `Ctrl + NumPad .` (Decimal)
  - Shows/hides a floating plane at the center of the `WorldDataVolume`.
  - The plane displays the same texture selected in the 2D overlay.

## Architectural Constraints
- **Single Volume Authority:** Only the first registered `WorldDataVolume` is used.
- **Immutable Safety:** Layers marked as `Immutable` cannot be modified at runtime via `SetValueAtLocation`.
- **Out-of-Bounds:** Queries outside the volume bounds return the layer's default value.