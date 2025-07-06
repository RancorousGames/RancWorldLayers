#include "WorldLayersSubsystem.h"
#include "Engine/TextureRenderTarget2D.h"
#include "ImageUtils.h"
#include "EngineUtils.h"
#include "RHICommandList.h"
#include "WorldDataLayerAsset.h"
#include "DynamicRHI.h"
#include "WorldDataVolume.h"
#include "Spatial/Quadtree.h"

void UWorldLayersSubsystem::InitializeFromVolume(AWorldDataVolume* Volume)
{
	// Clear any previously existing data
	WorldDataLayers.Empty();
	
	if (Volume)
	{
		WorldDataVolume = Volume;
		const FBox VolumeBounds = Volume->GetBounds().GetBox();
		WorldGridOrigin = FVector2D(VolumeBounds.Min.X, VolumeBounds.Min.Y);
		WorldGridSize = FVector2D(VolumeBounds.GetSize().X, VolumeBounds.GetSize().Y);

		if (WorldGridSize.X <= 0 || WorldGridSize.Y <= 0) // Using <= for robustness
		{
			// The bounds are invalid, likely from a programmatically spawned volume.
			// Recalculate both size and origin from the actor's transform.
			WorldGridSize = FVector2D(Volume->GetActorTransform().GetScale3D().X * 100.0f, Volume->GetActorTransform().GetScale3D().Y * 100.0f);
			
			// THE FIX: Also correct the origin based on the new size and the volume's location.
			// A volume's bounds are centered on its actor location.
			const FVector2D Center(Volume->GetActorLocation().X, Volume->GetActorLocation().Y);
			WorldGridOrigin = Center - (WorldGridSize * 0.5f);
		}

		// Load and register all layers specified in the volume
		for (const TSoftObjectPtr<UWorldDataLayerAsset>& LayerAssetPtr : Volume->LayerAssets)
		{
			if (UWorldDataLayerAsset* LayerAsset = LayerAssetPtr.LoadSynchronous())
			{
				RegisterDataLayer(LayerAsset);
			}
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("UWorldLayersSubsystem: InitializeFromVolume called with a null Volume. Subsystem will be inactive."));
		WorldDataVolume = nullptr;
		WorldGridOrigin = FVector2D::ZeroVector;
		WorldGridSize = FVector2D::ZeroVector;
	}
}


void UWorldLayersSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// On initialization, find the AWorldDataVolume in the world to configure the subsystem.
	if (UWorld* World = GetWorld())
	{
		AWorldDataVolume* FoundVolume = nullptr;
		for (TActorIterator<AWorldDataVolume> It(World); It; ++It)
		{
			FoundVolume = *It;
			break; // Find the first one
		}
		
		// FIX: Call the new explicit initializer. This keeps runtime behavior the same.
		InitializeFromVolume(FoundVolume);
	}

	TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UWorldLayersSubsystem::Tick));
}


void UWorldLayersSubsystem::Deinitialize()
{
	FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
	WorldDataLayers.Empty();
	Super::Deinitialize();
}

bool UWorldLayersSubsystem::Tick(float DeltaTime)
{
	for (auto& Elem : WorldDataLayers)
	{
		UWorldDataLayer* Layer = Elem.Value;
		if (!Layer) continue;

		// Handle CPU to GPU sync
		if (Layer->bIsDirty && Layer->GpuRepresentation)
		{
			SyncCPUToGPU(Layer);
			Layer->bIsDirty = false;
		}

		// Handle GPU to CPU readback
		if (Layer->Config->GPUConfiguration.bIsGPUWritable && Layer->GpuRepresentation && WorldDataVolume.IsValid())
		{
			const EWorldDataLayerReadbackBehavior ReadbackBehavior = Layer->Config->GPUConfiguration.ReadbackBehavior;
			const float PeriodicReadbackSeconds = Layer->Config->GPUConfiguration.PeriodicReadbackSeconds;

			if (ReadbackBehavior == EWorldDataLayerReadbackBehavior::Periodic)
			{
				if (GetWorld() && GetWorld()->GetTimeSeconds() - Layer->LastReadbackTime >= PeriodicReadbackSeconds)
				{
					ReadbackTexture(Layer);
					Layer->LastReadbackTime = GetWorld()->GetTimeSeconds();
				}
			}
		}
	}
	return true;
}

UWorldLayersSubsystem* UWorldLayersSubsystem::Get(UObject* WorldContext)
{
	if (!WorldContext)
	{
		UE_LOG(LogTemp, Warning, TEXT("WorldContext is null in UMyWorldDataSubsystem::Get."));
		return nullptr;
	}

	auto* World = WorldContext->GetWorld();
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	if (!GameInstance)
	{
		if (!World->IsEditorWorld())
			UE_LOG(LogTemp, Warning, TEXT("GameInstance is null in UMyWorldDataSubsystem::Get."));
		return nullptr;
	}

	UWorldLayersSubsystem* Subsystem = GameInstance->GetSubsystem<UWorldLayersSubsystem>();
	if (!Subsystem)
	{
		UE_LOG(LogTemp, Warning, TEXT("UMyWorldDataSubsystem is not found, but it should have been automatically created."));
	}

	return Subsystem;
}



bool UWorldLayersSubsystem::GetValueAtLocation(FName LayerName, const FVector2D& WorldLocation, FLinearColor& OutValue) const
{
	if (const UWorldDataLayer* DataLayer = WorldDataLayers.FindRef(LayerName))
	{
		FIntPoint PixelCoords = WorldLocationToPixel(WorldLocation, DataLayer);
		OutValue = DataLayer->GetValueAtPixel(PixelCoords);
		return true;
	}
	OutValue = FLinearColor(0.0f, 0.0f, 0.0f, 0.0f);
	return false;
}

float UWorldLayersSubsystem::GetFloatValueAtLocation(FName LayerName, const FVector2D& WorldLocation) const
{
	FLinearColor Value;
	if (GetValueAtLocation(LayerName, WorldLocation, Value))
	{
		return Value.R;
	}
	return 0.0f;
}

void UWorldLayersSubsystem::SetValueAtLocation(FName LayerName, const FVector2D& WorldLocation, const FLinearColor& NewValue)
{
	if (UWorldDataLayer* DataLayer = WorldDataLayers.FindRef(LayerName))
	{
		FIntPoint PixelCoords = WorldLocationToPixel(WorldLocation, DataLayer);
		DataLayer->SetValueAtPixel(PixelCoords, NewValue);
	}
}

void UWorldLayersSubsystem::RegisterDataLayer(UWorldDataLayerAsset* LayerAsset)
{
	if (LayerAsset)
	{
		UWorldDataLayer* NewDataLayer = NewObject<UWorldDataLayer>(this);
		NewDataLayer->Initialize(LayerAsset, WorldGridSize);
		WorldDataLayers.Add(LayerAsset->LayerName, NewDataLayer);

		if (LayerAsset->GPUConfiguration.bKeepUpdatedOnGPU)
		{
			EPixelFormat PixelFormat = PF_Unknown;
			switch (LayerAsset->DataFormat)
			{
				case EDataFormat::R8: PixelFormat = PF_G8; break;
				case EDataFormat::R16F: PixelFormat = PF_R16F; break;
				case EDataFormat::RGBA8: PixelFormat = PF_B8G8R8A8; break;
				case EDataFormat::RGBA16F: PixelFormat = PF_FloatRGBA; break;
			}

			if (PixelFormat != PF_Unknown)
			{
				if (LayerAsset->GPUConfiguration.bIsGPUWritable)
				{
					NewDataLayer->GpuRepresentation = NewObject<UTextureRenderTarget2D>(NewDataLayer);
					Cast<UTextureRenderTarget2D>(NewDataLayer->GpuRepresentation)->InitCustomFormat(NewDataLayer->Resolution.X, NewDataLayer->Resolution.Y, PixelFormat, false);
				}
				else
				{
					NewDataLayer->GpuRepresentation = UTexture2D::CreateTransient(NewDataLayer->Resolution.X, NewDataLayer->Resolution.Y, PixelFormat);
				}
				NewDataLayer->GpuRepresentation->UpdateResource();
			}
		}
	}
}

UTexture* UWorldLayersSubsystem::GetLayerGpuTexture(FName LayerName) const
{
	const UWorldDataLayer* DataLayer = WorldDataLayers.FindRef(LayerName);
	if (DataLayer)
	{
		return DataLayer->GpuRepresentation;
	}
	return nullptr;
}

bool UWorldLayersSubsystem::FindNearestPointWithValue(FName LayerName, const FVector2D& SearchOrigin, float MaxSearchRadius, const FLinearColor& TargetValue, FVector2D& OutWorldLocation) const
{
	const UWorldDataLayer* DataLayer = WorldDataLayers.FindRef(LayerName);
	if (!DataLayer || !DataLayer->Config->SpatialOptimization.bBuildAccelerationStructure || DataLayer->SpatialIndices.IsEmpty())
	{
		return false;
	}

	TSharedPtr<FQuadtree> TargetQuadtree;
	for (const auto& Elem : DataLayer->SpatialIndices)
	{
		if (TargetValue.Equals(Elem.Key, KINDA_SMALL_NUMBER))
		{
			TargetQuadtree = Elem.Value;
			break;
		}
	}

	if (!TargetQuadtree)
	{
		return false;
	}

	FIntPoint SearchPixel = WorldLocationToPixel(SearchOrigin, DataLayer);
	const float PixelSizeX = WorldGridSize.X / DataLayer->Resolution.X;
	const float PixelRadius = MaxSearchRadius / PixelSizeX;

	FIntPoint NearestPixel;
	if (TargetQuadtree->FindNearest(SearchPixel, PixelRadius, NearestPixel))
	{
		OutWorldLocation = PixelToWorldLocation(NearestPixel, DataLayer);
		return true;
	}

	return false;
}

/** Queues an asynchronous readback of a GPU texture to a staging buffer. This is non-blocking. */
void UWorldLayersSubsystem::ReadbackTexture(UWorldDataLayer* DataLayer)
{
	if (!DataLayer || !DataLayer->GpuRepresentation || !DataLayer->Config->GPUConfiguration.bIsGPUWritable)
	{
		return;
	}

	UTextureRenderTarget2D* RenderTarget = Cast<UTextureRenderTarget2D>(DataLayer->GpuRepresentation);
	if (!RenderTarget || !RenderTarget->GetResource()) return;

	FTextureResource* TextureResource = RenderTarget->GetResource();
	const FName LayerName = DataLayer->Config->LayerName;
	
	// Enqueue a command on the render thread to perform the readback.
	ENQUEUE_RENDER_COMMAND(ReadSurfaceCommand)(
	[this, TextureResource, LayerName](FRHICommandListImmediate& RHICmdList)
	{
		// This code now runs on the Render Thread.
		
		FRHITexture* TextureRHI = TextureResource->GetTexture2DRHI();
		if (!TextureRHI) return;

		FIntRect ReadbackRect(0, 0, TextureRHI->GetSizeX(), TextureRHI->GetSizeY());

		// Create a TArray to hold the readback data. This will be populated by the RHI function.
		TArray<FColor> ReadbackData;

		// Call the CORRECT, SYNCHRONOUS readback function. This will block the render thread briefly.
		// GetReference() is used to get the raw pointer from the RHI ref.
		RHICmdList.ReadSurfaceData(
			TextureRHI,
			ReadbackRect,
			ReadbackData,
			FReadSurfaceDataFlags()
		);

		// Now that we have the data, schedule a task to process it back on the game thread.
		// We move the ReadbackData array into the lambda to transfer ownership safely.
		AsyncTask(ENamedThreads::GameThread, [this, LayerName, ReadbackData = MoveTemp(ReadbackData)]()
		{
			// This code now runs safely on the Game Thread.
			if (UWorldDataLayer* LayerToUpdate = WorldDataLayers.FindRef(LayerName))
			{
				const int32 TotalBytes = LayerToUpdate->Resolution.X * LayerToUpdate->Resolution.Y * LayerToUpdate->GetBytesPerPixel();
				if (ReadbackData.Num() * sizeof(FColor) == TotalBytes)
				{
					LayerToUpdate->RawData.SetNum(TotalBytes);
					FMemory::Memcpy(LayerToUpdate->RawData.GetData(), ReadbackData.GetData(), TotalBytes);
					LayerToUpdate->bIsDirty = true;
				}
			}
		});
	});
}


/** Updates a GPU texture from CPU data. Now handles both UTexture2D and UTextureRenderTarget2D. */
void UWorldLayersSubsystem::SyncCPUToGPU(UWorldDataLayer* DataLayer)
{
	if (!DataLayer || !DataLayer->GpuRepresentation || !DataLayer->GpuRepresentation->GetResource())
	{
		return;
	}

	FTextureResource* TextureResource = DataLayer->GpuRepresentation->GetResource();
	const int32 Width = DataLayer->Resolution.X;
	const int32 Height = DataLayer->Resolution.Y;
	const int32 BytesPerPixel = DataLayer->GetBytesPerPixel();
	const uint32 Stride = Width * BytesPerPixel;
	TArray<uint8> RawDataCopy = DataLayer->RawData; // Make a copy for the lambda

	ENQUEUE_RENDER_COMMAND(UpdateWorldDataLayerTexture)(
	[TextureResource, Width, Height, Stride, RawDataCopy](FRHICommandListImmediate& RHICmdList)
	{
		FUpdateTextureRegion2D UpdateRegion(0, 0, 0, 0, Width, Height);
		FTexture2DRHIRef Texture2DRHI = TextureResource->GetTexture2DRHI();
		if (Texture2DRHI)
		{
			RHICmdList.UpdateTexture2D(Texture2DRHI, 0, UpdateRegion, Stride, RawDataCopy.GetData());
		}
	});
}

#include "Curves/CurveLinearColor.h"

UTexture2D* UWorldLayersSubsystem::GetDebugTextureForLayer(FName LayerName, UTexture2D* InDebugTexture)
{
	UWorldDataLayer* DataLayer = WorldDataLayers.FindRef(LayerName);
	if (!DataLayer)
	{
		return nullptr;
	}

	const int32 Width = DataLayer->Resolution.X;
	const int32 Height = DataLayer->Resolution.Y;

	// Create a new texture if one isn't provided or if the size is wrong
	UTexture2D* DebugTexture = InDebugTexture;
	if (!DebugTexture || DebugTexture->GetSizeX() != Width || DebugTexture->GetSizeY() != Height)
	{
		DebugTexture = UTexture2D::CreateTransient(Width, Height, PF_B8G8R8A8);
		if (!DebugTexture)
		{
			return nullptr;
		}
	}

	// Lock the texture for writing
	uint8* MipData = static_cast<uint8*>(DebugTexture->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE));

	UCurveLinearColor* ColorCurve = Cast<UCurveLinearColor>(DataLayer->Config->DebugVisualization.ColorCurve.LoadSynchronous());

	// Copy data from our layer to the texture
	for (int32 y = 0; y < Height; ++y)
	{
		for (int32 x = 0; x < Width; ++x)
		{
			FLinearColor PixelValue = DataLayer->GetValueAtPixel(FIntPoint(x, y));
			FColor MappedColor;

			if (DataLayer->Config->DebugVisualization.VisualizationMode == EWorldDataLayerVisualizationMode::ColorRamp && ColorCurve)
			{
				float NormalizedValue = (PixelValue.R - DataLayer->Config->DebugValueRange.X) / (DataLayer->Config->DebugValueRange.Y - DataLayer->Config->DebugValueRange.X);
				MappedColor = ColorCurve->GetLinearColorValue(FMath::Clamp(NormalizedValue, 0.f, 1.f)).ToFColor(true);
			}
			else // Grayscale
			{
				MappedColor = PixelValue.ToFColor(true); // to sRGB
			}

			int32 PixelIndex = (y * Width + x) * 4; // 4 bytes per pixel (BGRA)
			MipData[PixelIndex] = MappedColor.B;
			MipData[PixelIndex + 1] = MappedColor.G;
			MipData[PixelIndex + 2] = MappedColor.R;
			MipData[PixelIndex + 3] = MappedColor.A;
		}
	}

	// Unlock the texture
	DebugTexture->GetPlatformData()->Mips[0].BulkData.Unlock();
	DebugTexture->UpdateResource();

	return DebugTexture;
}

void UWorldLayersSubsystem::ExportLayerToPNG(UWorldDataLayerAsset* LayerAsset, const FString& FilePath)
{
	if (!LayerAsset)
	{
		return;
	}
    
	UWorldDataLayer* DataLayer = WorldDataLayers.FindRef(LayerAsset->LayerName);
	if (!DataLayer)
	{
		UE_LOG(LogTemp, Warning, TEXT("ExportLayerToPNG: Could not find registered data layer '%s'"), *LayerAsset->LayerName.ToString());
		return;
	}

	TArray<FColor> RawColorData;
	const int32 Width = DataLayer->Resolution.X;
	const int32 Height = DataLayer->Resolution.Y;
	RawColorData.SetNumUninitialized(Width * Height);

	for (int32 y = 0; y < Height; ++y)
	{
		for (int32 x = 0; x < Width; ++x)
		{
			const FLinearColor PixelValue = DataLayer->GetValueAtPixel(FIntPoint(x, y));
			RawColorData[y * Width + x] = PixelValue.ToFColor(true);
		}
	}

	const FImageView ImageView(RawColorData.GetData(), Width, Height, ERawImageFormat::BGRA8);
	TArray64<uint8> CompressedData;
	if (FImageUtils::CompressImage(CompressedData, TEXT("png"), ImageView))
	{
		FFileHelper::SaveArrayToFile(CompressedData, *FilePath);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("ExportLayerToPNG: Failed to compress image data for layer '%s'"), *LayerAsset->LayerName.ToString());
	}
}

void UWorldLayersSubsystem::ImportLayerFromPNG(UWorldDataLayerAsset* LayerAsset, const FString& FilePath)
{
	UWorldDataLayer* DataLayer = WorldDataLayers.FindRef(LayerAsset->LayerName);
	if (DataLayer)
	{
		UTexture2D* ImportedTexture = FImageUtils::ImportFileAsTexture2D(FilePath);
		if (ImportedTexture)
		{
			const FColor* MipData = static_cast<const FColor*>(ImportedTexture->GetPlatformData()->Mips[0].BulkData.LockReadOnly());

			for (int32 y = 0; y < ImportedTexture->GetSizeY(); ++y)
			{
				for (int32 x = 0; x < ImportedTexture->GetSizeX(); ++x)
				{
					FColor PixelColor = MipData[y * ImportedTexture->GetSizeX() + x];
					FLinearColor LinearColor = FLinearColor::FromSRGBColor(PixelColor);
					DataLayer->SetValueAtPixel(FIntPoint(x, y), LinearColor);
				}
			}

			ImportedTexture->GetPlatformData()->Mips[0].BulkData.Unlock();
			LayerAsset->Modify();
		}
	}
}

FIntPoint UWorldLayersSubsystem::WorldLocationToPixel(const FVector2D& WorldLocation, const UWorldDataLayer* DataLayer) const
{
	FVector2D RelativeLocation = WorldLocation - WorldGridOrigin;

	int32 PixelX;
	int32 PixelY;

	FVector2D CellSize;
	// FIX: Use the authoritative CellSize for each resolution mode to avoid precision errors.
	if (DataLayer->Config->ResolutionMode == EResolutionMode::RelativeToWorld)
	{
		// For relative mode, the asset's CellSize is the source of truth.
		CellSize = DataLayer->Config->CellSize;
	}
	else // Absolute Mode
	{
		// For absolute mode, the cell size is derived from the total grid size and fixed resolution.
		CellSize = FVector2D(WorldGridSize.X / DataLayer->Resolution.X, WorldGridSize.Y / DataLayer->Resolution.Y);
	}

	PixelX = FMath::FloorToInt(RelativeLocation.X / CellSize.X);
	PixelY = FMath::FloorToInt(RelativeLocation.Y / CellSize.Y);

	if (WorldDataVolume.IsValid() && WorldDataVolume->OutOfBoundsBehavior == EOutOfBoundsBehavior::ClampToEdge)
	{
		PixelX = FMath::Clamp(PixelX, 0, DataLayer->Resolution.X - 1);
		PixelY = FMath::Clamp(PixelY, 0, DataLayer->Resolution.Y - 1);
	}

	return FIntPoint(PixelX, PixelY);
}

FVector2D UWorldLayersSubsystem::PixelToWorldLocation(const FIntPoint& PixelLocation, const UWorldDataLayer* DataLayer) const
{
	FVector2D WorldLocation;
	const FVector2D CellSize = FVector2D(WorldGridSize.X / DataLayer->Resolution.X, WorldGridSize.Y / DataLayer->Resolution.Y);

	// Return the center of the pixel
	WorldLocation.X = (PixelLocation.X + 0.5f) * CellSize.X + WorldGridOrigin.X;
	WorldLocation.Y = (PixelLocation.Y + 0.5f) * CellSize.Y + WorldGridOrigin.Y;
	
	return WorldLocation;
}