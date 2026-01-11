#include "WorldLayersSubsystem.h"
#include "Engine/TextureRenderTarget2D.h"
#include "ImageUtils.h"
#include "EngineUtils.h"
#include "RHICommandList.h"
#include "WorldDataLayerAsset.h"
#include "DynamicRHI.h"
#include "RHIResources.h"
#include "WorldDataVolume.h"
#include "Spatial/Quadtree.h"
#include "WorldLayersDebugActor.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "WorldLayersDebugWidget.h"

void UWorldLayersSubsystem::ClearAllLayers()
{
	UE_LOG(LogTemp, Log, TEXT("[RancWorldLayers] Subsystem: Clearing all registered layers."));
	WorldDataLayers.Empty();
	WorldDataVolume = nullptr;
}

void UWorldLayersSubsystem::InitializeFromVolume(AWorldDataVolume* Volume)
{
	if (!Volume) return;

	UWorld* World = GetWorld();
	FString WorldName = World ? World->GetOutermost()->GetName() : TEXT("None");

	if (WorldDataVolume.IsValid() && WorldDataVolume.Get() != Volume)
	{
		UE_LOG(LogTemp, Warning, TEXT("[RancWorldLayers] Subsystem already has a registered WorldDataVolume (%s). Ignoring request from %s. Only one volume per world is supported."), *WorldDataVolume->GetName(), *Volume->GetName());
		return;
	}

	// Always clear and re-register if we are in the Editor to ensure layer list stays accurate
	if (GIsEditor)
	{
		WorldDataLayers.Empty();
	}
	
	WorldDataVolume = Volume;

	UE_LOG(LogTemp, Log, TEXT("[RancWorldLayers] Initializing from Volume '%s' in World %s"), *Volume->GetName(), *WorldName);
	
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

	UE_LOG(LogTemp, Log, TEXT("[RancWorldLayers] Subsystem Bounds Configured: Origin=%s, Size=%s"), *WorldGridOrigin.ToString(), *WorldGridSize.ToString());

	// Load and register all layers specified in the volume
	for (const TSoftObjectPtr<UWorldDataLayerAsset>& LayerAssetPtr : Volume->LayerAssets)
	{
		if (UWorldDataLayerAsset* LayerAsset = LayerAssetPtr.LoadSynchronous())
		{
			RegisterDataLayer(LayerAsset);
		}
	}

	SpawnDebugActor();
}

void UWorldLayersSubsystem::SpawnDebugActor()
{
	UWorld* World = GetWorld();
	if (!World || World->HasAnyFlags(RF_ClassDefaultObject)) return;

	// Check if already exists in the world
	for (TActorIterator<AWorldLayersDebugActor> It(World); It; ++It)
	{
		DebugActor = *It;
		return;
	}

	// Only spawn if the world is initialized enough
	if (!World->IsGameWorld() && !GIsEditor) return;

	FActorSpawnParameters SpawnParams;
	SpawnParams.ObjectFlags |= RF_Transient;
	DebugActor = World->SpawnActor<AWorldLayersDebugActor>(AWorldLayersDebugActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);

	if (DebugActor)
	{
		UE_LOG(LogTemp, Log, TEXT("[RancWorldLayers] Automatically spawned Debug Actor."));
		
		// Attempt to find a suitable widget class if not set
		if (!DebugActor->DebugWidgetClass)
		{
			// 1. Try to load the standard default from Plugin Content
			const FString DefaultWidgetPath = TEXT("/RancWorldLayers/Debug/BP_WorldLayersDebugWidget.BP_WorldLayersDebugWidget_C");
			UClass* DefaultWidgetClass = StaticLoadClass(UObject::StaticClass(), nullptr, *DefaultWidgetPath, nullptr, LOAD_None, nullptr);
			if (DefaultWidgetClass && DefaultWidgetClass->IsChildOf(UWorldLayersDebugWidget::StaticClass()))
			{
				DebugActor->DebugWidgetClass = TSubclassOf<UWorldLayersDebugWidget>(DefaultWidgetClass);
			}

			// 2. Fallback: Search Asset Registry
			if (!DebugActor->DebugWidgetClass)
			{
				FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
				TArray<FAssetData> AssetData;
				// Filter for Widget Blueprints
				FARFilter Filter;
				Filter.ClassPaths.Add(FTopLevelAssetPath(FName("/Script/UMGEditor"), FName("WidgetBlueprint")));
				Filter.bRecursiveClasses = true;
				AssetRegistryModule.Get().GetAssets(Filter, AssetData);

				for (const FAssetData& Data : AssetData)
				{
					// Check parent class using tags (faster than loading) or load and check
					const FString ParentClassPath = Data.GetTagValueRef<FString>(TEXT("ParentClass"));
					if (ParentClassPath.Contains(TEXT("WorldLayersDebugWidget")))
					{
						if (UBlueprint* BP = Cast<UBlueprint>(Data.GetAsset()))
						{
							if (BP->GeneratedClass && BP->GeneratedClass->IsChildOf(UWorldLayersDebugWidget::StaticClass()))
							{
								DebugActor->DebugWidgetClass = TSubclassOf<UWorldLayersDebugWidget>(BP->GeneratedClass);
								break;
							}
						}
					}
				}
			}
		}

		if (DebugActor->DebugWidgetClass)
		{
			DebugActor->CreateDebugWidget();
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[RancWorldLayers] Spawned Debug Actor but could not find a Widget Blueprint inheriting from WorldLayersDebugWidget. Please create one to see the overlay."));
		}
	}
}


void UWorldLayersSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	UWorld* World = GetWorld();
	if (!World || World->IsNetMode(NM_DedicatedServer) || World->HasAnyFlags(RF_ClassDefaultObject))
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[RancWorldLayers] Subsystem Initialized for World: %s"), *World->GetOutermost()->GetName());

	// AUTO-DISCOVERY in Editor: 
	if (GIsEditor && !World->IsGameWorld())
	{
		for (TActorIterator<AWorldDataVolume> It(World); It; ++It)
		{
			UE_LOG(LogTemp, Log, TEXT("[RancWorldLayers] Initialize: Auto-discovered Volume '%s' in Editor"), *It->GetName());
			InitializeFromVolume(*It);
			break;
		}
	}

	TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UWorldLayersSubsystem::Tick));
}


void UWorldLayersSubsystem::Deinitialize()
{
	if (TickHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
	}
	WorldDataLayers.Empty();
	Super::Deinitialize();
}

bool UWorldLayersSubsystem::Tick(float DeltaTime)
{
	// AUTO-DISCOVERY: Try to find a volume if we don't have one
	if (!WorldDataVolume.IsValid())
	{
		UWorld* World = GetWorld();
		if (World)
		{
			for (TActorIterator<AWorldDataVolume> It(World); It; ++It)
			{
				UE_LOG(LogTemp, Log, TEXT("[RancWorldLayers] Tick: Auto-discovered Volume '%s'"), *It->GetName());
				InitializeFromVolume(*It);
				break;
			}
		}
	}

	// Ensure Debug Actor exists if we have a volume
	if (WorldDataVolume.IsValid() && !DebugActor)
	{
		SpawnDebugActor();
	}

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

UWorldLayersSubsystem* UWorldLayersSubsystem::Get(const UObject* WorldContext)
{
	if (!WorldContext) return nullptr;
	UWorld* World = WorldContext->GetWorld();
	if (!World) return nullptr;
	return World->GetSubsystem<UWorldLayersSubsystem>();
}

bool UWorldLayersSubsystem::GetValueAtLocation(FName LayerName, const FVector2D& WorldLocation, FLinearColor& OutValue) const
{
	// AUTO-SYNC (Thread Safe): Only if on Game Thread.
	if (!WorldDataVolume.IsValid() && IsInGameThread())
	{
		AWorldDataVolume* FoundVolume = nullptr;
		for (TActorIterator<AWorldDataVolume> It(GetWorld()); It; ++It)
		{
			FoundVolume = *It;
			break;
		}
		if (FoundVolume)
		{
			const_cast<UWorldLayersSubsystem*>(this)->InitializeFromVolume(FoundVolume);
		}
	}

	if (const UWorldDataLayer* DataLayer = WorldDataLayers.FindRef(LayerName))
	{
		FIntPoint PixelCoords = WorldLocationToPixel(WorldLocation, DataLayer);
		OutValue = DataLayer->GetValueAtPixel(PixelCoords);
		return true;
	}
	OutValue = FLinearColor(0.0f, 0.0f, 0.0f, 0.0f);
	return false;
}

bool UWorldLayersSubsystem::GetValueAtLocationInterpolated(FName LayerName, const FVector2D& WorldLocation, FLinearColor& OutValue) const
{
	if (const UWorldDataLayer* DataLayer = WorldDataLayers.FindRef(LayerName))
	{
		// 1. Calculate continuous pixel coordinates
		FVector2D RelativeLocation = WorldLocation - WorldGridOrigin;
		FVector2D CellSize;
		if (DataLayer->Config->ResolutionMode == EResolutionMode::RelativeToWorld)
		{
			CellSize = DataLayer->Config->CellSize;
		}
		else
		{
			CellSize = FVector2D(WorldGridSize.X / (float)DataLayer->Resolution.X, WorldGridSize.Y / (float)DataLayer->Resolution.Y);
		}

		// Center-aligned sampling: subtract 0.5 to make integer coordinates represent pixel centers
		float PixelX = (RelativeLocation.X / CellSize.X) - 0.5f;
		float PixelY = (RelativeLocation.Y / CellSize.Y) - 0.5f;

		int32 X0 = FMath::FloorToInt(PixelX);
		int32 Y0 = FMath::FloorToInt(PixelY);
		float FracX = PixelX - (float)X0;
		float FracY = PixelY - (float)Y0;

		// 2. Fetch 4 neighbors
		FLinearColor V00 = DataLayer->GetValueAtPixel(FIntPoint(X0, Y0));
		FLinearColor V10 = DataLayer->GetValueAtPixel(FIntPoint(X0 + 1, Y0));
		FLinearColor V01 = DataLayer->GetValueAtPixel(FIntPoint(X0, Y0 + 1));
		FLinearColor V11 = DataLayer->GetValueAtPixel(FIntPoint(X0 + 1, Y0 + 1));

		// 3. Bi-Lerp
		OutValue = FMath::BiLerp(V00, V10, V01, V11, FracX, FracY);
		return true;
	}

	OutValue = FLinearColor::Black;
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
		FRHITexture* Texture2DRHI = TextureResource->GetTextureRHI();
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

void UWorldLayersSubsystem::UpdateDebugRenderTarget(FName LayerName, UTextureRenderTarget2D* RenderTarget)
{
	if (!RenderTarget || !RenderTarget->GetResource()) return;

	UWorldDataLayer* DataLayer = WorldDataLayers.FindRef(LayerName);
	if (!DataLayer) return;

	const int32 Width = DataLayer->Resolution.X;
	const int32 Height = DataLayer->Resolution.Y;

	// Ensure RenderTarget is initialized with correct size and format
	if (RenderTarget->SizeX != Width || RenderTarget->SizeY != Height)
	{
		RenderTarget->InitCustomFormat(Width, Height, PF_B8G8R8A8, false);
		RenderTarget->UpdateResource();
	}

	TArray<FColor> ColorBuffer;
	ColorBuffer.SetNumUninitialized(Width * Height);

	UCurveLinearColor* ColorCurve = Cast<UCurveLinearColor>(DataLayer->Config->DebugVisualization.ColorCurve.LoadSynchronous());

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

			ColorBuffer[y * Width + x] = MappedColor;
		}
	}

	FTextureResource* TextureResource = RenderTarget->GetResource();
	ENQUEUE_RENDER_COMMAND(UpdateDebugRenderTargetCommand)(
		[TextureResource, Width, Height, ColorBuffer = MoveTemp(ColorBuffer)](FRHICommandListImmediate& RHICmdList)
		{
			FUpdateTextureRegion2D Region(0, 0, 0, 0, Width, Height);
			FRHITexture* TextureRHI = TextureResource->GetTextureRHI();
			if (TextureRHI)
			{
				RHICmdList.UpdateTexture2D(TextureRHI, 0, Region, Width * sizeof(FColor), (uint8*)ColorBuffer.GetData());
			}
		}
	);
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

void UWorldLayersSubsystem::UpdateDerivativeLayer(FName LayerName)
{
	UWorldDataLayer* TargetLayer = WorldDataLayers.FindRef(LayerName);
	if (!TargetLayer || TargetLayer->Config->Mutability != EWorldDataLayerMutability::Derivative) return;

	// Search for any actor in the world that provides derivation logic
	bool bHandled = false;
	int32 CheckedActors = 0;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		CheckedActors++;
		AActor* Actor = *It;
		
		IWorldLayersDerivationProvider* Provider = Cast<IWorldLayersDerivationProvider>(Actor);
		if (Provider)
		{
			UE_LOG(LogTemp, Log, TEXT("[RancWorldLayers] Found Provider: %s"), *Actor->GetName());
			if (Provider->OnDeriveLayer(LayerName))
			{
				bHandled = true;
				break;
			}
		}
		else if (Actor->GetName().Contains(TEXT("Procedural")))
		{
			// BP fallback if we ever add a BP implementation later
			if (Actor->GetClass()->ImplementsInterface(UWorldLayersDerivationProvider::StaticClass()))
			{
				UE_LOG(LogTemp, Log, TEXT("[RancWorldLayers] Found BP Provider: %s"), *Actor->GetName());
				// For now, we only support native, but let's log the attempt
			}
		}
	}

	if (!bHandled)
	{
		UE_LOG(LogTemp, Warning, TEXT("[RancWorldLayers] No IWorldLayersDerivationProvider found in world. Checked %d actors. Target Layer: %s"), CheckedActors, *LayerName.ToString());
	}
}

FIntPoint UWorldLayersSubsystem::WorldLocationToPixel(const FVector2D& WorldLocation, const UWorldDataLayer* DataLayer) const
{
	FVector2D RelativeLocation = WorldLocation - WorldGridOrigin;

	int32 PixelX;
	int32 PixelY;

	FVector2D CellSize;
	if (DataLayer->Config->ResolutionMode == EResolutionMode::RelativeToWorld)
	{
		CellSize = DataLayer->Config->CellSize;
	}
	else // Absolute Mode
	{
		CellSize = FVector2D(WorldGridSize.X / (float)DataLayer->Resolution.X, WorldGridSize.Y / (float)DataLayer->Resolution.Y);
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

const UWorldDataLayer* UWorldLayersSubsystem::GetDataLayer(FName LayerName) const
{
	// AUTO-SYNC (Thread Safe): If we have no volume, try to find one now.
	if (!WorldDataVolume.IsValid() && IsInGameThread())
	{
		UWorld* World = GetWorld();
		AWorldDataVolume* FoundVolume = nullptr;
		if (World)
		{
			for (TActorIterator<AWorldDataVolume> It(World); It; ++It)
			{
				FoundVolume = *It;
				break;
			}
		}

		if (FoundVolume)
		{
			UE_LOG(LogTemp, Log, TEXT("[RancWorldLayers] GetDataLayer: Auto-initializing from found volume '%s'."), *FoundVolume->GetName());
			const_cast<UWorldLayersSubsystem*>(this)->InitializeFromVolume(FoundVolume);
		}
	}

	return WorldDataLayers.FindRef(LayerName);
}

TArray<FName> UWorldLayersSubsystem::GetActiveLayerNames() const
{
	TArray<FName> Names;
	WorldDataLayers.GetKeys(Names);
	return Names;
}