
# Multi-Layered Semantic World Map Plugin for Unreal Engine (Combined)

---

## 1. Introduction & Core Philosophy

### 1.1. The Problem: Data Chaos in Game Worlds

In modern game development, especially for worlds leveraging procedural content generation (PCG), managing and querying environmental data efficiently at runtime is a paramount challenge. AI needs to understand traversability and threat levels; PCG systems need to know elevation and biome types; gameplay mechanics need to know if an area is sacred, buildable, or resource-rich; and visual systems need to render all of this information cohesively.

Traditionally, this leads to a chaotic and inefficient architecture:
*   **Data Fragmentation:** Each system has its own "view" of the world, often acquiring data independently via expensive raycasts or complex collision checks. This leads to redundancy, performance bottlenecks, and data inconsistencies.
*   **Inefficient Polling:** Actors constantly query the world state (`Am I on fire?`, `Is this water still here?`) every frame, wasting CPU cycles and leading to unresponsive-feeling logic.
*   **Workflow Bottlenecks:** Designers and artists lack direct, intuitive tools to author and visualize complex gameplay data, slowing down iteration.
*   **Poor Scalability:** Adding new types of world data requires significant engineering effort and changes across multiple systems.

### 1.2. The Solution: A Unified, Reactive Data Source

This plugin provides a single, authoritative, and highly performant source of truth for all semantic world data. It abstracts 3D complexity into a unified, data-driven, multi-resolution 2D grid-based representation of your 3D world, serving as a central source of truth for all gameplay systems.

This architecture is built on three core philosophical pillars:

1.  **Data-Driven Design:** The existence, properties, and data of every layer are defined by assets in the Content Browser, not by C++ code. This empowers designers and artists to create and manage layers without writing code.
2.  **Editor-Runtime Duality:** A strict separation between heavyweight editor tools and lightweight runtime objects ensures a rich, interactive authoring experience without sacrificing performance in the final packaged game.
3.  **Reactive Systems over Polling:** An advanced event-based data-binding API allows game systems to subscribe to changes in the world, leading to vastly more efficient and responsive gameplay logic.

---

Of course. Here is a concise Quickstart Guide formatted in Markdown, ready to be pasted directly into your `README.md` file.

---

## Quickstart Guide: Your First Data Layer in 5 Minutes

This guide will walk you through creating your first data layer, visualizing it, and modifying it from a Blueprint.

### 1. Define the World Bounds

The system needs to know what area of your world to manage.

1.  From the **Place Actors** panel, find `AWorldDataVolume` and drag it into your level.
2.  **Scale the volume** to encompass your entire playable area. The system will use these bounds as the 0,0 to max,max mapping for all its data layers.



### 2. Create the Layer Asset

This asset is the "blueprint" for your data layer.

1.  In the **Content Browser**, right-click and go to **Miscellaneous -> World Data Layer**.
2.  Name the new asset `DA_Height`.
3.  Open `DA_Height` and configure its core properties:
    *   **Layer Name**: `Height` (This is the name you'll use in code/Blueprints).
    *   **Resolution Mode**: `Absolute`
    *   **Resolution**: `X: 512`, `Y: 512`
    *   **Data Format**: `R8` (A single 8-bit channel, perfect for simple grayscale values).
4.  Save the asset.

### 3. Link the Layer to the World

Tell the `AWorldDataVolume` to load your new layer.

1.  Select the `AWorldDataVolume` in your level.
2.  In the **Details** panel, find the **Layer Assets** array.
3.  Click the **+** icon to add an element, and select your `DA_Height` asset from the dropdown.



### 4. Visualize the Data

Let's see the layer in action.

1.  From the **Place Actors** panel, find `BP_WorldLayersDebugActor` and drag it into your level.
2.  Press **Play (PIE)**.
3.  A debug widget will appear in the top-left corner. Select **"Height"** from the dropdown. You should see a solid black square, representing the layer's default value of 0.

### 5. Modify and Query the Data

Let's "paint" a value into the layer and then read it back.

1.  Create a new **Actor Blueprint**, name it `BP_DataPainter`.
2.  Open `BP_DataPainter` and go to the **Event Graph**.
3.  Recreate the following Blueprint logic:
    *   On **Event Begin Play**, we get the subsystem, set a value of `1.0` (white) at our actor's location, and then immediately query the value at that same location to print it.

    

4.  Place one or more `BP_DataPainter` actors into your level.

### 6. See the Result!

Press **Play** again. You should now see:

*   The **Debug Widget** for the "Height" layer now has white pixels where your `BP_DataPainter` actors are located.
*   The **Output Log** will show "Value at location is: 1.000", confirming the data was written and read back successfully.

Congratulations! You have successfully set up, modified, and queried a world data layer. From here, you can explore more complex data formats, use the data in materials, drive PCG, or inform AI decisions.

## 2. System Architecture

### 2.1. The Duality Model: Editor vs. Runtime

The plugin is composed of two parallel sets of classes that exist in different contexts. This separation is the most critical architectural concept.

*   **Editor Context (Loaded only when `WITH_EDITOR` is true):**
    *   **Purpose:** To provide a rich user experience for data authoring, visualization, and baking (e.g., from `ALandscape` actors). Performance is secondary to functionality and interactivity.
    *   **Key Classes:** `UMyWorldDataEditorSubsystem`, `UWorldDataLayer_Editor` (a heavyweight object for live previews and data handling).

*   **Runtime Context (Exists in PIE and Packaged Games):**
    *   **Purpose:** To provide lean, high-performance access to the world data for gameplay systems. It is optimized for speed, low memory overhead, and efficient querying.
    *   **Key Classes:** `UWorldLayersSubsystem` (Game Instance Subsystem), `URuntimeWorldDataLayer` (a lightweight object holding only necessary gameplay data), `AWorldDataVolume`.

### 2.2. The On-Disk Data Pipeline: Native Unreal Assets

The bridge between these two contexts is a set of native Unreal Engine assets (`.uasset`), ensuring seamless integration with source control, the Asset Registry, cooking, and packaging.

1.  **Configuration Asset (`UWorldDataLayerAsset`):** A `UDataAsset` that defines *how* a layer behaves. This is the central configuration object for designers.
2.  **Bulk Data Asset (`UWorldLayerBulkDataAsset`):** A custom `UObject`-based asset class whose sole purpose is to hold the large `TArray` of serialized pixel data for a single layer. When data is baked in the editor, it's saved to one of these assets. This makes the data fully visible to Unreal's reference tracking and build systems, replacing earlier, less robust methods like external `.bin` files.
3.  **Artist-Authored Data (`UTexture2D`):** For layers sourced from artist-painted textures, the source `.uasset` texture file itself remains the on-disk data, which is then processed into a `BulkDataAsset` via an editor action.

---

## 3. Core Components & Data Structures

### 3.1. AWorldDataVolume: The World Definition

This is the central anchor point placed in each level. It defines the spatial bounds and active layers for a specific map.

*   **Class:** `AWorldDataVolume` (inherits from `AVolume`)
*   **Purpose:** To provide a single, visually editable source for global world data configuration. Designers place and scale this volume to encompass the playable map area.
*   **Properties:**
    *   `TArray<TSoftObjectPtr<UWorldDataLayerAsset>> LayerAssets;`
        *   **Description:** The primary configuration array. The designer populates this list with all the layer assets that should be loaded for this level.
    *   `EOutOfBoundsBehavior OutOfBoundsBehavior = EOutOfBoundsBehavior::ReturnDefaultValue;`
        *   **Description:** An enum (`ReturnDefaultValue`, `ClampToEdge`) that globally defines how queries for locations outside the volume's bounds are handled.
*   **Implicit Properties (Derived at Runtime):**
    *   **WorldGridOrigin (`FVector2D`):** The world-space coordinate (X, Y) corresponding to the bottom-left corner of the `(0, 0)` grid cell. Calculated from `GetBounds().GetBox().Min`.
    *   **WorldGridSize (`FVector2D`):** The total world-space size (X, Y) of the managed area. Calculated from `GetBounds().GetBox().GetSize()`.

	Of course. Here is a dense, to-the-point documentation block for just the coordinate conversion system.

---

### \#\#\# Coordinate System: World & Grid Mapping

All conversions between world-space and grid-space are handled internally by the `UWorldLayersSubsystem` to ensure precision and consistency. The system is anchored by the `AWorldDataVolume` actor placed in the level, whose bounds define the global mapping.

#### **(h4) Core Definitions**

*   **World Location (`FVector2D`):** Standard Unreal world coordinate (cm). The input for public API functions.
*   **Grid Coordinate (`FIntPoint`):** The integer `(X, Y)` address of a cell in a layer's grid. `(0, 0)` is the bottom-left cell.
*   **World Grid Origin (`FVector2D`):** The world-space coordinate corresponding to the bottom-left corner of the `(0, 0)` grid cell. Derived from `AWorldDataVolume`'s minimum bounds.
*   **Layer Cell Size (`FVector2D`):** The world-space size of a single grid cell for a given layer.

#### **(h4) World to Grid Conversion**

This is used for all queries and modifications. It translates a `WorldLocation` into a `GridCoordinate`.

**Formula:**
```
PixelX = floor((WorldLocation.X - WorldGridOrigin.X) / LayerCellSize.X)
PixelY = floor((WorldLocation.Y - WorldGridOrigin.Y) / LayerCellSize.Y)
```

**Process:**
1.  **Normalize:** The `WorldLocation` is made relative to the `WorldGridOrigin`. This correctly handles negative world coordinates.
2.  **Scale:** The result is divided by the layer-specific `LayerCellSize` to convert from world units to grid units.
3.  **Floor:** The `floor()` operation discards the fractional part, mapping any point within a cell's boundary to the same integer `GridCoordinate`.

#### **(h4) Grid to World Conversion**

This is used internally to provide world-space results from grid-based searches. It translates a `GridCoordinate` to the `WorldLocation` at the **center** of that cell.

**Formula:**
```
WorldLocation.X = ((PixelX + 0.5) * LayerCellSize.X) + WorldGridOrigin.X
WorldLocation.Y = ((PixelY + 0.5) * LayerCellSize.Y) + WorldGridOrigin.Y
```

**Process:**
1.  **Center:** `0.5` is added to the integer `GridCoordinate` to find the cell's center point.
2.  **Scale:** The result is multiplied by the `LayerCellSize` to convert from grid units to world units.
3.  **Translate:** The `WorldGridOrigin` is added back to shift the relative coordinate into its final world-space position.

### 3.2. UWorldDataLayerAsset: The Layer Blueprint

This `UDataAsset` is the "blueprint" for a single layer. It contains no runtime state, only configuration, giving designers complete control.

| Parameter Group | Parameter | Data Type | Purpose & Details |
| :--- | :--- | :--- | :--- |
| **Identity** | `LayerName` | `FName` | The primary, human-readable identifier used to query the layer (e.g., "Heightmap", "TerritorialInfluence"). |
| | `LayerID` | `FGuid` | An automatically generated, unique, persistent ID to prevent issues if a designer renames the asset file. |
| **Data Format** | `DataFormat` | `enum` | **CRITICAL.** Enum defining the in-memory data type for each pixel. **Enum Values:** `UInt8`, `Int16`, `Int32`, `Float16` (half), `Float32` (float), `Vector4_8Bit` (equivalent to RGBA8). |
| | `ResolutionMode` | `enum` | Defines how resolution is determined. `Absolute` (e.g., 2048x2048) or `RelativeToWorld` (e.g., 1 pixel per meter). |
| | `Resolution` | `FIntPoint` | Used if `ResolutionMode` is `Absolute`. |
| | `CellSize` | `FVector2D` | Used if `ResolutionMode` is `RelativeToWorld`. |
| | `InterpolationMethod` | `enum` | How to return a value when queried between pixels. **Options:** `NearestNeighbor` (fast, blocky, good for ID maps) or `Bilinear` (smoothly blends 4 neighbors, good for continuous data). |
| **Initialization** | `InitializationSource` | `enum` | How the layer is populated at load time. **Enum Values:** `FromDefaultValue`, `FromSourceTexture`, `FromBakedData`. |
| | `SourceTexture` | `TSoftObjectPtr<UTexture2D>` | The texture to use if initializing `FromSourceTexture`. |
| | `BakedData` | `TSoftObjectPtr<UWorldLayerBulkDataAsset>` | A soft pointer to the companion asset holding the bulk data. This is populated by editor tools when baking. |
| | `DefaultValue` | `double` | The initial numeric value for every pixel. The system correctly casts this `double` to the target `DataFormat`. |
| **Mutability & Persistence** | `Mutability` | `enum` | `Immutable` (read-only after generation), `InitialOnly` (writable during load, read-only at runtime), or `Continuous` (writable at any time). |
| | `bNeedsSaving` | `bool` | If true, this layer's data will be included in the game's save files. Essential for persistent data like territory. |
| **Runtime Optimization**| `SpatialOptimization` | `struct` | Configures "Find Nearest" queries. Contains: `bool bBuildAccelerationStructure`, an `enum` for `StructureType` (e.g., `Quadtree`), and a `TArray<FLinearColor>` of `ValuesToTrack`. |
| | `GPUConfiguration` | `struct` | Configures GPU interaction. Contains: `bool bKeepUpdatedOnGPU`, `bool bIsGPUWritable`, an `enum` for `ReadbackBehavior` (`None`, `OnDemand`, `Periodic`), a `float` for `PeriodicReadbackSeconds`, and a `TSoftObjectPtr<UNiagaraSystem>` for any associated GPU simulation. |
| **Editor & Debugging** | `bAllowPNG_IO` | `bool` | Enables editor utilities to export/import a visual representation of this layer to/from a `.png` file. |
| | `DebugValueRange` | `FVector2D` | Defines the expected min/max range of the data (e.g., -1000 to 5000 for height). Used to map numeric values to a grayscale (0-1) range for debug textures and PNG exports. |
| | `ChannelSemantics` | `TArray<FString>` | An array of up to 4 strings describing what each data channel represents (e.g., `["TribeID", "InfluenceStr", "LastUpdate", "Reserved"]`). Purely for editor tooltips. |
| | `DebugVisualization` | `struct` | Configures the debug overlay. Contains: an `enum` for `VisualizationMode` (`Grayscale`, `ColorRamp`), and a `TSoftObjectPtr<UCurveLinearColorAtlas>` to map data values to specific colors. |

### 3.3. URuntimeWorldDataLayer: The Live Layer Object

This is the runtime instance of a single data layer, managed by the `UWorldLayersSubsystem`.

*   **Properties:**
    *   `TStrongObjectPtr<UWorldDataLayerAsset> Config`: A pointer to the asset that defines this layer's behavior.
    *   `TArray<uint8> RawData`: The master CPU-side copy of the pixel data, cast to the correct type at initialization based on the asset's `DataFormat`.
    *   `UTexture* GpuRepresentation`: A pointer to the `UTexture2D` or `UTextureRenderTarget2D` on the GPU, if `bKeepUpdatedOnGPU` is true.
    *   `bool bIsDirty`: A flag set to true whenever `RawData` is modified. The subsystem checks this flag to perform batched CPU-to-GPU updates.
    *   A pointer to its spatial acceleration structure (e.g., a Quadtree), if configured.

---

## 4. Data Authoring & Editing Workflows

### 4.1. Editor-Time Authoring

These methods are managed by the `UMyWorldDataEditorSubsystem` to provide a seamless, native-feeling authoring experience.

1.  **Baking from `ALandscape`:**
    *   **Workflow:** A designer uses an Editor Utility Widget, selects the `ALandscape` actor and a target `UWorldDataLayerAsset`, and clicks "Bake".
    *   **Implementation:** The `EditorSubsystem` extracts the height data. It then programmatically creates a new `UWorldLayerBulkDataAsset`, serializes the height data into it, saves the new asset, and links it to the `BakedData` property on the source `UWorldDataLayerAsset`.

2.  **Importing from Texture (`.png`, `.exr`):**
    *   **Workflow:** An artist creates a high-precision map (e.g., 16-bit grayscale `.png`). In Unreal, they use a right-click utility on the target `UWorldDataLayerAsset` to "Import and Bake...".
    *   **Implementation:** The utility reads the source texture, correctly handling color space conversion (sRGB to Linear) to ensure data integrity. It then performs the same process as above: creates a new `UWorldLayerBulkDataAsset`, serializes the data, and links the two assets.

3.  **Direct In-Editor Painting:**
    *   **Workflow:** A designer uses a custom "World Data Paint Tool" in the editor modes panel to directly modify layers in the 3D viewport.
    *   **Implementation:** The paint tool interacts with the `EditorSubsystem`. On save, the subsystem saves the modified data into the linked `UWorldLayerBulkDataAsset`.

### 4.2. Runtime Editing

These actions are performed by the `UWorldLayersSubsystem` on the lightweight runtime layer objects.

1.  **Point-Based Modification:**
    *   **Workflow:** A "Frost" spell freezes the ground. The spell actor calls `UWorldLayersSubsystem::SetFloatValueAtLocation("Temperature", HitLocation, -10.0f);`.
    *   **Implementation:** The subsystem's API includes strongly-typed setters. It writes the new value into the `RawData` buffer, sets the `bIsDirty` flag, and broadcasts any bound events.

2.  **Procedural Bulk Modification:**
    *   **Workflow:** A PCG system generates a `TArray<float>` for the heightmap. It calls `UWorldLayersSubsystem::SetLayerDataFromArray_Float("Height", TheNewData);`.
    *   **Implementation:** A suite of `SetLayerDataFromArray_*` functions exists for each data type, ensuring type safety and performance for large-scale updates.

---

## 5. Interfaces & API (`UWorldLayersSubsystem`)

### 5.1. Data Query & Modification

```cpp
// Strongly-typed getters for performance and type safety
bool GetFloatValueAtLocation(FName LayerName, const FVector2D& WorldLocation, float& OutValue) const;
bool GetIntValueAtLocation(FName LayerName, const FVector2D& WorldLocation, int32& OutValue) const;

// Convenience getter for multi-channel data
bool GetValueAtLocation(FName LayerName, const FVector2D& WorldLocation, FLinearColor& OutValue) const;

// Strongly-typed setters
void SetFloatValueAtLocation(FName LayerName, const FVector2D& WorldLocation, float NewValue);
void SetIntValueAtLocation(FName LayerName, const FVector2D& WorldLocation, int32 NewValue);

// Bulk data modification
bool SetLayerDataFromArray_Float(FName LayerName, const TArray<float>& InData);
```

### 5.2. Optimized Spatial Queries

Requires `SpatialOptimization` to be configured on the asset.

```cpp
/** Efficiently finds the nearest world location with a specific value. */
bool FindNearestPointWithValue(FName LayerName, const FVector2D& SearchOrigin, float MaxSearchRadius, 
                               const FLinearColor& TargetValue, FVector2D& OutWorldLocation) const;
```

### 5.3. Reactive Event System

The event system uses a handle object, `UWorldDataSubscription`, to manage subscriptions cleanly.

```cpp
// Delegate signatures
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPixelValueChanged, FGenericDataUnion, NewValue); // FGenericDataUnion is a custom struct that can hold any of our supported data types.
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnRegionChanged);

/**
 * Creates a subscription to changes at a single pixel.
 * @param Owner The object that owns this subscription, used for automatic cleanup.
 * @return A subscription handle object. You must bind your function to the delegate on this object.
 */
UFUNCTION(BlueprintCallable, Category = "World Data|Events")
UWorldDataSubscription* SubscribeToPixel(FName LayerName, const FVector2D& WorldCoordinate, UObject* Owner);

/** Creates a subscription to changes within a square radius around a pixel. */
UFUNCTION(BlueprintCallable, Category = "World Data|Events")
UWorldDataSubscription* SubscribeToRegion(FName LayerName, const FVector2D& WorldCoordinate, int32 Radius, UObject* Owner);

/** Updates the world coordinate that an existing subscription is tracking. Useful for moving actors. */
UFUNCTION(BlueprintCallable, Category = "World Data|Events")
void UpdateSubscriptionLocation(UWorldDataSubscription* Subscription, const FVector2D& NewWorldCoordinate);

/** Manually destroys a subscription. Also called automatically if the owner is destroyed. */
UFUNCTION(BlueprintCallable, Category = "World Data|Events")
void Unsubscribe(UWorldDataSubscription* Subscription);
```

### 5.4. GPU & Debugging API

```cpp
/** Returns the GPU texture representation of a layer for use in Materials, Niagara, etc. */
UTexture* GetLayerGpuTexture(FName LayerName) const;

/** Generates a temporary texture for visualizing layer data in a debug HUD. */
UTexture2D* GetDebugTextureForLayer(FName LayerName);

// Editor Utility Functions
void ExportLayerToPNG(UWorldDataLayerAsset* LayerAsset, const FString& FilePath);
void ImportLayerFromPNG(UWorldDataLayerAsset* LayerAsset, const FString& FilePath);
```

---

## 6. Key Features & Example Use Cases

*   **Terrain Heightmap:**
    *   **Configuration:** A "Height" asset. `DataFormat`: `R16F` or `R32F`. `Mutability`: `Immutable`. `InitializationSource`: `FromBakedData` after baking from `ALandscape`. `bKeepUpdatedOnGPU`: true.
    *   **Use:** PCG graphs query it for placement. Materials sample its GPU texture for visual height blending.

*   **Biome & Water Map:**
    *   **Configuration:** A "Biomes" asset. `DataFormat`: `Vector4_8Bit`. `ChannelSemantics`: `["BiomeID", "WaterDepth", "Fertility", "TraversabilityCost"]`. `InterpolationMethod`: `NearestNeighbor` to prevent blending IDs. `InitializationSource`: `FromSourceTexture` using an artist-painted map.
    *   **Use:** AI queries "TraversabilityCost" for pathfinding. PCG spawns assets based on "BiomeID". The player character checks "WaterDepth".

*   **Dynamic Spreading Corruption (GPU-Driven):**
    *   **Configuration:** A "Corruption" asset. `DataFormat`: `R8`. `Mutability`: `Continuous`. In `GPUConfiguration`, set `bIsGPUWritable` to true, `ReadbackBehavior` to `Periodic` (e.g., every 0.5s), and link the controlling `UNiagaraSystem`.
    *   **Use:** The linked Niagara system simulates the spread directly on a `UTextureRenderTarget2D`. Post-process materials sample this texture for visual effects. The periodic GPU-to-CPU readback updates the CPU data, allowing AI to react to the corruption's spread via the event system.

*   **Territorial Influence Map:**
    *   **Configuration:** A "Territory" asset. `DataFormat`: `Vector4_8Bit` to store influence for up to four factions. `Mutability`: `Continuous`. `bNeedsSaving`: true.
    *   **Use:** AI units "paint" their influence into this layer. Other AI queries it to make strategic decisions. The data is saved and loaded with the game. A "Purifier" building could use `SubscribeToRegion` to be notified when enemy influence enters its perimeter.

---

## 7. Implementation Deep Dive & Known Challenges

*   **Spatial Acceleration Structure:** The system for `FindNearestPointWithValue` requires implementing a dynamic Quadtree (or similar structure). This structure must support efficient addition and removal of points for mutable layers to prevent costly full rebuilds.
*   **Asynchronous GPU Readback:** Implementing the `Periodic` or `OnDemand` readback requires direct interaction with Unreal's Render Hardware Interface (RHI). This involves using mechanisms like `RHICopyTextureToStagingBuffer` and managing callbacks from the render thread to the game thread to avoid stalling the game. This is the most technically complex part of the plugin.
*   **Editor Tooling:** A robust PNG importer must correctly handle **color space conversion** (sRGB to Linear). Creating a user-friendly editor experience for the paint tools and debug visualization will require building custom details panels and editor modes using Slate/UMG.

---

## 8. Implementation Notes
*   **Coordinate System:** Implement the `WorldToPixel` and `PixelToWorld` conversion functions carefully. They must use the `AWorldDataVolume`'s `WorldGridOrigin` as the anchor point for all calculations to correctly handle negative world coordinates. Use `FMath::FloorToInt` for the conversion, not a simple cast to `int`.
*   **Data Access:** When accessing the `TArray<uint8> RawData`, you will need to cast the pointer based on the layer's `DataFormat`. For example: `float* FloatData = reinterpret_cast<float*>(Layer->RawData.GetData());`. Be extremely careful with this. An incorrect cast will cause memory corruption.
*   **Event System:** The event binding maps (`TMap<FIntPoint, ...>`) can become large. When an object that has bound to an event is destroyed (e.g., a building is destroyed), you **must** call `UnbindAllFromObject(TheBuilding)` to remove its delegates from the maps. Failure to do so will result in dangling pointers and crashes.
*   **Editor vs. Runtime Distinction:** Remember to wrap all editor-only code (`#include "Editor/EditorEngine.h"`, Slate UI, `FEdMode` logic) inside an `#if WITH_EDITOR` preprocessor directive. This ensures it is completely compiled out of the final packaged game.
*   **Serialization:** When saving/loading the `.bin` files or save game data, always write a `Version` number as the very first entry. This allows you to maintain backward compatibility if you change the data layout in the future. You can read the version number first and then decide how to interpret the rest of the data.

## 9. Conclusion

This plugin provides a comprehensive, three-pronged approach to world data management:
1.  **On-Demand Querying** for simple, direct state checks.
2.  **Optimized Spatial Searching** for discovering features in a large world.
3.  **Event-Driven Data-Binding** for highly efficient, reactive systems that respond to change rather than polling for it.


## Implementation Plan

### Stage 1: The Core Data Framework (CPU-Only)

**Goal:** Establish the fundamental C++ classes and data storage. At the end of this stage, you will have a system that can load, store, and be queried for world data on the CPU, with no GPU or editor integration yet.

**Implementation Steps:**
1.  **Create the Plugin Module:** Set up a new C++ plugin module in Unreal Engine (e.g., `MyWorldDataPlugin`).
2.  **Define Core Classes (Skeletons):**
    *   Create `UWorldDataLayerAsset` (`UDataAsset`) with just the core identity (`LayerName`, `LayerID`) and data representation properties (`ResolutionMode`, `Resolution`/`CellSize`, `DataFormat`, `DefaultValue`).
    *   Create `UWorldLayersSubsystem` (`UGameInstanceSubsystem`) with a `TMap<FName, UWorldDataLayer*>`.
    *   Create `UWorldDataLayer` (`UObject`) with a `TArray<uint8> RawData` and a pointer to its `UWorldDataLayerAsset` config.
3.  **Implement Subsystem Initialization:**
    *   In `UWorldLayersSubsystem::Initialize`, write the logic to find all `UWorldDataLayerAsset` assets in the project using the `AssetRegistry`.
    *   For each asset found, `new` a `UWorldDataLayer` runtime object, initialize its `RawData` array based on the configured resolution and `DefaultValue`, and add it to the `TMap`.
4.  **Implement Core CPU API:**
    *   Implement the coordinate conversion logic (World to Layer Pixel and back).
    *   Implement `GetValueAtLocation(FName, FVector2D, FLinearColor&)` and `SetValueAtLocation(FName, FVector2D, FLinearColor)`. These will be the workhorses of the system. They should perform the coordinate conversion and then read/write directly to the correct `UWorldDataLayer`'s `RawData` array.

**Testing Plan for Stage 1:**
1.  **Unit Tests:** Create an Automation Test in Unreal.
    *   Create a test `UWorldDataLayerAsset` in the editor with a known resolution (e.g., 100x100) and data format (`R8`).
    *   In the test, get the subsystem, `SetValueAtLocation` at a few known world coordinates, and then use `GetValueAtLocation` to verify that the correct values are returned.
    *   Test edge cases: querying coordinates outside the world bounds, querying a layer that doesn't exist.

---

### Stage 2: Debug Visualization and Editor I/O

**Goal:** Make the invisible data visible. This stage is all about creating the tools to see and manipulate the data layers, which is critical for all future development.

**Implementation Steps:**
1.  **Implement `GetDebugTextureForLayer`:**
    *   In `UWorldLayersSubsystem`, create this function. It will get the specified `UWorldDataLayer`.
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
    *   In `UWorldLayersSubsystem::Tick`, iterate all layers.
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

3. **Event Subscriptions:**
    *   Implement the `UWorldDataSubscription` class and the `SubscribeTo...` API in the subsystem.
    *   This includes the underlying `TMap`s for tracking delegates.
    *   Modify `SetValueAt...` functions to broadcast events to any relevant subscribers.
---

### Stage 5: GPU-to-CPU 

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
	
	