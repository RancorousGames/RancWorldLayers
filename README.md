# RancWorldLayers
2D grid layers for your world keeping track of things like heightmaps, biomes, navigation, custom fields

Of course. Here is the fully updated documentation reflecting our discussion. It integrates the data-driven asset approach, details the extensive configuration options for each layer, and specifies the implementation challenges and solutions, resulting in a more robust and complete design specification.

***

## Multi-Layered Semantic World Map Plugin for Unreal Engine (V2.0)

### I. Introduction

In modern game development, especially for worlds leveraging procedural content generation (PCG), managing and querying environmental data efficiently at runtime is a paramount challenge. Traditional approaches often involve expensive raycasts, complex collision checks, or fragmented data sources, leading to performance bottlenecks, data inconsistencies, and difficult system integration.

This document outlines the architecture for a **Multi-Layered Semantic World Map Plugin for Unreal Engine**. This plugin provides a unified, data-driven, multi-resolution 2D grid-based representation of your 3D world, serving as a central source of truth for all gameplay systems. By moving the definition of world data into configurable **Data Assets**, this system empowers designers to create, manage, and tailor any conceivable data layer—from terrain height and biome placement to dynamic threat maps and territorial influence—without writing a single line of code.

The core idea is to abstract away 3D complexity into a collection of easily queryable 2D data layers, managed through a central subsystem. With robust support for varying resolutions, CPU-GPU synchronization, spatial query optimization, and a flexible data-driven workflow, this plugin streamlines world data management, enhances performance, and fosters deep, emergent integration between disparate game features.

### II. Core Concepts & Architecture

#### The Problem: Fragmented World Data

Various game systems need different types of information about the world:
*   **PCG:** Where to place forests, mountains, rivers? What's the elevation and slope?
*   **AI:** Where is safe to walk? Where is drinkable water? What territory am I in? Where is the greatest threat?
*   **Gameplay:** Can the player build here? Is this ground fertile? Is this area sacred?
*   **Visuals:** How does corruption spread? Where should dynamic fog appear? Where are biome transitions?

Often, each system acquires this data independently. This leads to redundancy, performance issues from expensive 3D queries, inconsistency between systems, and significant integration challenges.

#### Solution Overview: The Data-Driven Layered Grid

Our plugin tackles these issues by establishing a canonical, real-time updated **Multi-Resolution Layered Grid**. This system is driven entirely by **World Data Layer Assets**, a custom `UDataAsset` that allows designers to define every property of a given data layer. The central subsystem loads these assets at startup to create a collection of 2D grids, each representing a specific semantic property of the world.

**Key Principles:**
*   **Data-Driven Design:** Layers are not defined in C++. They are defined as assets in the Content Browser, giving designers full control.
*   **Centralized Access:** A single subsystem provides a unified API for all world semantic data queries.
*   **Multi-Resolution:** Each layer asset defines its own resolution or cell size, optimizing memory and query speed.
*   **Abstraction:** Game logic interacts with high-level queries (e.g., `GetValueAtLocation("Territory", ...)`) without needing to know the underlying data format, resolution, or storage.
*   **Extensibility:** Adding a new data layer to the game is as simple as creating and configuring a new `UWorldDataLayerAsset`.
*   **Performance-Aware:** Configuration includes options for GPU accessibility, asynchronous readback, and building optimized spatial query structures.

#### The Configuration Hub: UWorldDataLayerAsset

The cornerstone of this design is a new `UDataAsset` class, `UWorldDataLayerAsset`. Designers create these assets in the editor to define the existence and behavior of every data layer in the game.

| Parameter | Data Type | Purpose & Details |
| :--- | :--- | :--- |
| **Core Identity** | | |
| `LayerName` | `FName` | The primary, human-readable identifier used to query the layer. E.g., "Heightmap", "TerritorialInfluence". |
| `LayerID` | `FGuid` | An automatically generated, unique, persistent ID to prevent issues if a designer renames the asset file. |
| **Data Representation** | | |
| `ResolutionMode` | `enum` | Defines how the final resolution is determined. Options: `Absolute` (e.g., 2048x2048) or `RelativeToWorld` (e.g., 1 pixel per meter). |
| `Resolution` / `CellSize` | `FIntPoint` / `FVector2D` | The explicit resolution or world size of a single pixel, depending on the `ResolutionMode`. |
| `DataFormat` | `enum` | **Crucial.** Specifies the memory layout of each pixel. E.g., `R8` (single 8-bit int), `R16F` (single 16-bit float), `RGBA8` (four 8-bit ints), `RGBA16F` (four 16-bit floats). This directly impacts memory usage and data range. |
| `InterpolationMethod` | `enum` | How to return a value when queried between pixels. Options: `NearestNeighbor` (fast, blocky) or `Bilinear` (smoothly blends the 4 neighbors). |
| `DefaultValue` | `FLinearColor` | The initial value for every pixel when the layer is created. `FLinearColor` can store four float values, making it versatile for any data format. |
| **Mutability & Persistence** | | |
| `Mutability` | `enum` | `Immutable` (read-only after generation), `InitialOnly` (writable during load, read-only at runtime), or `Continuous` (writable at any time). |
| `bNeedsSaving` | `bool` | If true, this layer's data will be included in the game's save files. Essential for persistent data like territory, but can be disabled for transient data like threat maps. |
| **Runtime Behavior & Optimization** | | |
| `SpatialOptimization` | `struct` | Configures "Find Nearest" queries. Contains: `bool bBuildAccelerationStructure`, an `enum` for `StructureType` (e.g., `Quadtree`), and a `TArray<FLinearColor>` of `ValuesToTrack`. |
| `GPUConfiguration` | `struct` | Configures all GPU interaction. Contains: `bool bKeepUpdatedOnGPU`, `bool bIsGPUWritable`, an `enum` for `ReadbackBehavior` (`None`, `OnDemand`, `Periodic`), a `float` for `PeriodicReadbackSeconds`, and a `TSoftObjectPtr<UNiagaraSystem>` for any associated GPU simulation. |
| **Editor & Debugging** | | |
| `bAllowPNG_IO` | `bool` | If true, enables editor utilities to export this layer to or import it from a `.PNG` file, allowing for an artist-driven workflow. |
| `ChannelSemantics` | `TArray<FString>` | An array of 4 strings describing what each data channel (R, G, B, A) represents. Purely for editor tooltips and debugging. |
| `DebugVisualization` | `struct` | Configures the debug overlay. Contains: an `enum` for `VisualizationMode` (`Grayscale`, `ColorRamp`), and a `TSoftObjectPtr<UCurveLinearColorAtlas>` to map data values to specific colors. |

#### Coordinate Abstraction and Conversion

A critical component is the ability to seamlessly convert between 3D world coordinates and the 2D pixel coordinates of any given layer. This allows systems to query based on their 3D position, and the plugin handles the resolution difference transparently based on the layer's configured `CellSize`.

#### Data Flow Overview



### III. Plugin Structure & Implementation Details

#### Core Classes

*   **`UMyWorldDataSubsystem` (Game Instance Subsystem)**
    *   **Role:** The central manager and public API for the entire system.
    *   **Responsibilities:**
        *   On `Initialize`, discovers all `UWorldDataLayerAsset` assets in the project.
        *   For each asset, creates a runtime `UWorldDataLayer` instance.
        *   Manages the lifecycle of all runtime layer instances.
        *   Provides the primary API for querying and updating world data (CPU-side).
        *   In its `Tick` function, manages the update cycle for "dirty" layers, batching CPU-to-GPU data transfers.
        *   Handles serialization by iterating over layers flagged with `bNeedsSaving`.

*   **`UWorldDataLayer` (Abstract Base Class - Runtime Object)**
    *   **Role:** The runtime instance of a single data layer. There is one `UWorldDataLayer` object for each `UWorldDataLayerAsset`.
    *   **Properties:**
        *   `TStrongObjectPtr<UWorldDataLayerAsset> Config`: A pointer to the asset that defines this layer's behavior.
        *   `FIntPoint Resolution`: The final resolution of this layer.
        *   `TArray<uint8> RawData`: The master CPU-side copy of the pixel data. The actual type (`TArray<FFloat16>`, `TArray<FColor>`) would be determined at initialization based on the asset's `DataFormat`.
        *   `UTexture* GpuRepresentation`: A pointer to the `UTexture2D` or `UTextureRenderTarget2D` on the GPU, if `bKeepUpdatedOnGPU` is true.
        *   `bool bIsDirty`: A flag set to true whenever `RawData` is modified. The subsystem checks this flag to perform batched GPU updates.
        *   A pointer to its spatial acceleration structure (e.g., a Quadtree object), if configured.

*   **`UWorldDataLayerAsset` (Data Asset)**
    *   **Role:** The C++ class for the Data Asset described in the section above. It contains all the configurable properties that define a layer. It holds no runtime state.

#### The Update Cycle: Keeping Data Synchronized

To prevent performance issues from excessive GPU data transfers, a "dirty" system is essential.
1.  A gameplay system calls `UMyWorldDataSubsystem::SetValueAtLocation(...)`.
2.  The subsystem finds the correct `UWorldDataLayer` instance and modifies its `RawData` array.
3.  The `UWorldDataLayer` instance sets its internal `bIsDirty` flag to `true`.
4.  During its `Tick`, the `UMyWorldDataSubsystem` iterates through all layers.
5.  If a layer `bIsDirty` and has a GPU representation, the subsystem queues a command on the render thread to update the corresponding GPU texture region from the `RawData`. This batches many updates into a single transfer.
6.  After the update is sent, the `bIsDirty` flag is reset to `false`.

### IV. Interfaces & API

All public API is exposed through the `UMyWorldDataSubsystem` for clean, centralized access.

```cpp
// UMyWorldDataSubsystem.h

UCLASS()
class MYWORLDDATAPLUGIN_API UMyWorldDataSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    // Generic Query Methods - The Core API
    bool GetValueAtLocation(FName LayerName, const FVector2D& WorldLocation, FLinearColor& OutValue) const;
    float GetFloatValueAtLocation(FName LayerName, const FVector2D& WorldLocation) const; // Convenience for single-channel float layers

    // Generic Data Modification (CPU)
    void SetValueAtLocation(FName LayerName, const FVector2D& WorldLocation, const FLinearColor& NewValue);
    
    // Optimized Spatial Queries
    bool FindNearestPointWithValue(FName LayerName, const FVector2D& SearchOrigin, float MaxSearchRadius, 
                                   const FLinearColor& TargetValue, FVector2D& OutWorldLocation) const;

    // GPU Texture Access (for Materials, Niagara, etc.)
    UTexture* GetLayerGpuTexture(FName LayerName) const;

    // Editor Utility and Debugging
    UTexture2D* GetDebugTextureForLayer(FName LayerName);
    void ExportLayerToPNG(UWorldDataLayerAsset* LayerAsset, const FString& FilePath);
    void ImportLayerFromPNG(UWorldDataLayerAsset* LayerAsset, const FString& FilePath);
};
```

### V. Key Features & Use Cases

The power of this system comes from its flexibility. Here’s how you would configure assets for common use cases:

*   **Terrain Heightmap:**
    *   **Configuration:** Create a `UWorldDataLayerAsset` named "Height". Set `DataFormat` to `R16F` or `R32F`. `Mutability` is `Immutable`. `bAllowPNG_IO` is true. `bKeepUpdatedOnGPU` is true.
    *   **Use:** PCG graphs query it for placement. It's imported from a World Machine export or an existing `ALandscape` actor. Materials sample its GPU texture for visual height blending.

*   **Biome & Water Map:**
    *   **Configuration:** Create a "Biomes" asset. `DataFormat` is `RGBA8` for packing data. `ChannelSemantics` are `["BiomeID", "WaterDepth", "TraversabilityCost", "CustomFlags"]`. `InterpolationMethod` is `NearestNeighbor` to prevent blending biome IDs. `bAllowPNG_IO` is true for artists to paint.
    *   **Use:** AI queries "TraversabilityCost" for pathfinding. PCG spawns assets based on "BiomeID". The player character checks "WaterDepth".

*   **Dynamic Spreading Corruption (GPU-Driven):**
    *   **Configuration:** Create a "Corruption" asset. `DataFormat` is `R8`. `Mutability` is `Continuous`. In `GPUConfiguration`, set `bIsGPUWritable` to true, `ReadbackBehavior` to `Periodic` (e.g., every 0.5s), and link the controlling `UNiagaraSystem`.
    *   **Use:** The linked Niagara system simulates the spread directly on a `UTextureRenderTarget2D`. Post-process materials sample this texture for visual effects. The periodic GPU-to-CPU readback updates the CPU `RawData`, allowing AI to react to the corruption's spread with a slight delay.

*   **Territorial Influence Map:**
    *   **Configuration:** Create a "Territory" asset. `DataFormat` is `RGBA8` to store influence for up to four tribes. `Mutability` is `Continuous`. `bNeedsSaving` is true.
    *   **Use:** AI units "paint" their influence into this layer. Other AI queries it to make strategic decisions. The data is saved and loaded with the game.

### VI. Development Workflow & Debugging

*   **Editor Workflow:** The primary workflow is creating and configuring `UWorldDataLayerAsset` files in the Content Browser. Right-clicking one of these assets would bring up editor utility actions like "Export to PNG..." or "Import from PNG...".
*   **Runtime Debug Visualization:** An in-game debug overlay can be toggled. It will list all loaded layers. When a layer is selected for viewing, the subsystem reads its `DebugVisualization` configuration. It generates a temporary `UTexture2D` from the CPU data and applies the correct visualization (grayscale or a color ramp from the specified curve asset), displaying it on the HUD.
*   **Performance Considerations:**
    *   **Memory:** The `DataFormat` and `Resolution` properties on the asset give designers direct control over the memory footprint of each layer.
    *   **CPU <-> GPU Sync:** The cost is managed via the `GPUConfiguration` on the asset. Most layers will be CPU->GPU only. Expensive GPU->CPU readbacks are only enabled for specific layers that require it, and their frequency is controlled.
    *   **Query Speed:** For layers requiring frequent "Find Nearest" queries, setting up `SpatialOptimization` is critical. This trades higher memory usage (for the Quadtree) for drastically faster queries.

### VII. Implementation Deep Dive & Known Challenges

While the design is robust, the implementation of several features requires advanced engine knowledge.

*   **Spatial Acceleration Structure:** The system for `FindNearestPointWithValue` is not trivial. It requires implementing a dynamic Quadtree (or similar structure) in C++. This structure must support efficient addition and removal of points for mutable layers, preventing the need for costly full rebuilds.
*   **Asynchronous GPU Readback:** Implementing the `Periodic` or `OnDemand` readback requires direct interaction with Unreal's Render Hardware Interface (RHI). This involves using mechanisms like `RHICopyTextureToStagingBuffer` and managing callbacks from the render thread to the game thread to avoid stalling the game. This is the most technically complex part of the plugin.
*   **Editor Tooling & I/O:**
    *   A robust PNG importer must correctly handle **color space conversion** (sRGB to Linear) to ensure data integrity.
    *   Creating a user-friendly editor experience for configuring the `DebugVisualization` (especially the color ramp curve) will require building a **custom details panel** using Slate C++.

This data-driven, asset-based architecture provides an exceptionally powerful and flexible foundation for managing a complex game world. It empowers designers, streamlines development, and creates a single source of truth that enables deep, emergent gameplay systems.

## Implementation Plan

### Stage 1: The Core Data Framework (CPU-Only)

**Goal:** Establish the fundamental C++ classes and data storage. At the end of this stage, you will have a system that can load, store, and be queried for world data on the CPU, with no GPU or editor integration yet.

**Implementation Steps:**
1.  **Create the Plugin Module:** Set up a new C++ plugin module in Unreal Engine (e.g., `MyWorldDataPlugin`).
2.  **Define Core Classes (Skeletons):**
    *   Create `UWorldDataLayerAsset` (`UDataAsset`) with just the core identity (`LayerName`, `LayerID`) and data representation properties (`ResolutionMode`, `Resolution`/`CellSize`, `DataFormat`, `DefaultValue`).
    *   Create `UMyWorldDataSubsystem` (`UGameInstanceSubsystem`) with a `TMap<FName, UWorldDataLayer*>`.
    *   Create `UWorldDataLayer` (`UObject`) with a `TArray<uint8> RawData` and a pointer to its `UWorldDataLayerAsset` config.
3.  **Implement Subsystem Initialization:**
    *   In `UMyWorldDataSubsystem::Initialize`, write the logic to find all `UWorldDataLayerAsset` assets in the project using the `AssetRegistry`.
    *   For each asset found, `new` a `UWorldDataLayer` runtime object, initialize its `RawData` array based on the configured resolution and `DefaultValue`, and add it to the `TMap`.
4.  **Implement Core CPU API:**
    *   Implement the coordinate conversion logic (World to Layer Pixel and back).
    *   Implement `GetValueAtLocation(FName, FVector2D, FLinearColor&)` and `SetValueAtLocation(FName, FVector2D, FLinearColor)`. These will be the workhorses of the system. They should perform the coordinate conversion and then read/write directly to the correct `UWorldDataLayer`'s `RawData` array.

**Testing Plan for Stage 1:**
1.  **Unit Tests:** Create an Automation Test in Unreal.
    *   Create a test `UWorldDataLayerAsset` in the editor with a known resolution (e.g., 100x100) and data format (`R8`).
    *   In the test, get the subsystem, `SetValueAtLocation` at a few known world coordinates, and then use `GetValueAtLocation` to verify that the correct values are returned.
    *   Test edge cases: querying coordinates outside the world bounds, querying a layer that doesn't exist.
2.  **Manual In-Game Test:**
    *   Create a simple Blueprint Actor. In its `BeginPlay`, get the subsystem and programmatically set a few values on a layer (e.g., draw a line of '255' values).
    *   Use a `DrawDebugSphere` to visually confirm the locations where you are setting the data.
    *   Use a console command that calls `GetValueAtLocation` at the player's current position and prints the result to the screen. Walk over the line you drew and verify the printed values change.

---

### Stage 2: Debug Visualization and Editor I/O

**Goal:** Make the invisible data visible. This stage is all about creating the tools to see and manipulate the data layers, which is critical for all future development.

**Implementation Steps:**
1.  **Implement `GetDebugTextureForLayer`:**
    *   In `UMyWorldDataSubsystem`, create this function. It will get the specified `UWorldDataLayer`.
    *   Inside the function, create a transient `UTexture2D` with the same resolution and a compatible pixel format.
    *   Lock the texture's mip data and `memcpy` the layer's `RawData` into the texture buffer. This creates a simple grayscale representation.
2.  **Create Debug UMG Widget:**
    *   Create a `WBP_DebugOverlay` widget. In its graph, get the subsystem and call `GetDebugTextureForLayer`.
    *   Set the brush of an `Image` widget to the returned texture.
    *   Add buttons or a combo box to allow switching between different layers at runtime.
3.  **Implement PNG Import/Export:**
    *   Add the `bAllowPNG_IO` property to the `UWorldDataLayerAsset`.
    *   In a C++ Editor module for your plugin, create two `UEditorUtilityBlueprint` functions (or custom menu actions).
    *   **Export:** Takes a layer asset, reads its CPU `RawData`, and uses `FImageUtils::SaveImageToDisk()` to write it to a `.PNG` file. **Crucially, perform a Linear to sRGB color space conversion on the data before saving.**
    *   **Import:** Reads a `.PNG` file using `FImageUtils`, **converts the data from sRGB to Linear**, and populates the `RawData` of the selected layer.

**Testing Plan for Stage 2:**
1.  **Visualization Test:**
    *   Use the console command from Stage 1 to set a value. Verify that a corresponding change appears on the UMG debug overlay in real-time.
    *   Paint a simple image (e.g., a smiley face) in Photoshop.
2.  **I/O Round-Trip Test:**
    *   Create a layer and programmatically set some values.
    *   Use the Export utility to save it to a PNG. Open the PNG and verify it looks correct (e.g., a grayscale image).
    *   Create a *new*, empty layer. Use the Import utility to load the PNG you just saved.
    *   Use the debug UMG widget to view this new layer and confirm the data has been loaded correctly.

---

### Stage 3: GPU Integration (CPU-to-GPU)

**Goal:** Get the data onto the GPU so it can be used by materials and visual effects. This stage focuses only on the one-way street from CPU to GPU.

**Implementation Steps:**
1.  **Add GPU Configuration:** Add the `GPUConfiguration` struct to the `UWorldDataLayerAsset`, starting with just `bKeepUpdatedOnGPU`.
2.  **GPU Resource Management:**
    *   In `UWorldDataLayer`, add a `UTexture2D* GpuRepresentation` member.
    *   When the subsystem creates a layer, if `bKeepUpdatedOnGPU` is true, create a dynamic `UTexture2D` and store it in `GpuRepresentation`.
3.  **Implement the "Dirty" System:**
    *   Add the `bool bIsDirty` flag to `UWorldDataLayer`. The `SetValueAtLocation` function must now set this flag to `true`.
    *   In `UMyWorldDataSubsystem::Tick`, iterate all layers.
    *   If a layer `isDirty`, call a new internal function `SyncCPUToGPU(Layer)`. This function will update the `GpuRepresentation` texture with the `RawData`. For now, a simple full texture update is fine.
    *   Reset `bIsDirty` to `false` after the update.
4.  **Expose GPU Texture:** Implement `GetLayerGpuTexture(FName)` in the subsystem.

**Testing Plan for Stage 3:**
1.  **Material Test:**
    *   Create a simple Landscape Material. Add a `TextureSample` node and parameterize it.
    *   Create a Blueprint that, on `BeginPlay`, gets the subsystem, gets the GPU texture for a specific layer, and sets it as the parameter for the landscape material.
    *   Use your existing console command to `SetValueAtLocation` on the CPU data.
    *   **Verification:** The landscape's appearance should change in real-time where you "paint" the data, as the dirty system updates the GPU texture each frame.

---

### Stage 4: Advanced Queries & Optimizations

**Goal:** Implement the high-performance spatial queries and refine the GPU update pipeline.

**Implementation Steps:**
1.  **Implement a Quadtree:**
    *   Write or integrate a simple Quadtree C++ class that can store 2D integer points.
    *   Add the `SpatialOptimization` struct to your `UWorldDataLayerAsset`.
    *   When a layer is initialized, if `bBuildAccelerationStructure` is true, iterate through the `RawData`, find all pixels matching `ValuesToTrack`, and insert their integer coordinates into the Quadtree instance owned by the `UWorldDataLayer`.
2.  **Implement `FindNearestPointWithValue`:**
    *   This function in the subsystem will access the layer's Quadtree and perform an efficient nearest-neighbor search. It will return the world coordinates of the found point.
3.  **Refine GPU Updates:**
    *   Modify the `SyncCPUToGPU` logic. Instead of updating the whole texture, track dirty *regions* (rectangles). Update only the dirty parts of the texture using `RHIUpdateTexture2D`, which is more efficient than recreating the whole thing.
4.  **Implement Mutability Rules:** Add the `Mutability` enum and enforce it. If a layer is `Immutable`, the `SetValueAtLocation` function should do nothing (or log a warning) after the initial load.

**Testing Plan for Stage 4:**
1.  **Quadtree Test:**
    *   In an automation test, create a layer, populate it with a few known points (e.g., at (10,10) and (100,100)), and build the structure.
    *   Call `FindNearestPointWithValue` from various search origins (e.g., (12,12), (80,80)) and assert that the correct nearest point is returned.
2.  **Performance Test:**
    *   Create a scenario where you are rapidly updating many small, separate areas of a layer.
    *   Use Unreal's `Stat GPU` and `Stat CPU` to compare the performance of the old full-texture update vs. the new dirty-region update. You should see a significant improvement.

---

### Stage 5: GPU-to-CPU and Final Polish

**Goal:** Implement the most complex part of the system—getting data back from the GPU—and add the final user-facing polish.

**Implementation Steps:**
1.  **Implement GPU Readback:**
    *   Complete the `GPUConfiguration` struct in the asset (add `bIsGPUWritable`, `ReadbackBehavior`, etc.).
    *   This is the advanced RHI part. You will need to implement a system that can queue an asynchronous texture copy (`RHICopyTextureToStagingBuffer`) and use a callback delegate that fires on the game thread 1-2 frames later when the data is ready.
    *   The callback will receive the raw data and update the `UWorldDataLayer`'s CPU-side `RawData` array.
2.  **Integrate with a GPU Simulation:**
    *   Create a sample Niagara system that writes to a `UTextureRenderTarget2D`.
    *   Make your `UWorldDataLayer`'s GPU representation a Render Target if `bIsGPUWritable` is true.
    *   Trigger the asynchronous readback from this Render Target.
3.  **Finalize Editor Tooling:**
    *   Build the custom details panel for the `UWorldDataLayerAsset` using Slate to provide a nice UI for the `DebugVisualization` color curve.
    *   Add comprehensive tooltips to all asset properties explaining what they do.
    *   Add the `ChannelSemantics` property and display those strings in the editor UI to help designers remember what each channel is for.

**Testing Plan for Stage 5:**
1.  **GPU-to-CPU Test:**
    *   Have your sample Niagara system "paint" a value (e.g., 255) at the center of its render target.
    *   In a Blueprint, use a simple `GetValueAtLocation` polling the center of the world.
    *   **Verification:** After 1-2 frames, the Blueprint's printed value should change from 0 to 255, proving the data has completed the round trip from GPU to CPU.
2.  **Designer Workflow Test:**
    *   Hand the system off to a non-programmer. Ask them to create a new layer for "Fertility," paint it in Photoshop, import it, and hook it up to a material for visualization, all without writing code. Their ability to do this successfully is the ultimate test of your UX and tooling.