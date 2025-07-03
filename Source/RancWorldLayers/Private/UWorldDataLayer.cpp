#include "UWorldDataLayer.h"

void UWorldDataLayer::Initialize(UWorldDataLayerAsset* InConfig)
{
	Config = TObjectPtr<UWorldDataLayerAsset>(InConfig);

	if (Config->ResolutionMode == EResolutionMode::Absolute)
	{
		Resolution = Config->Resolution;
	}
	else // RelativeToWorld
	{
		// For now, we'll just use a default resolution. This will be properly calculated later.
		Resolution = FIntPoint(1024, 1024);
	}

	int32 BytesPerPixel = GetBytesPerPixel();
	RawData.SetNumZeroed(Resolution.X * Resolution.Y * BytesPerPixel);

	// Initialize with DefaultValue
	for (int32 Y = 0; Y < Resolution.Y; ++Y)
	{
		for (int32 X = 0; X < Resolution.X; ++X)
		{
			SetValueAtPixel(FIntPoint(X, Y), Config->DefaultValue);
		}
	}

	bIsDirty = false;
}

FLinearColor UWorldDataLayer::GetValueAtPixel(const FIntPoint& PixelCoords) const
{
	if (!RawData.IsValidIndex(GetPixelIndex(PixelCoords)))
	{
		return FLinearColor::Black; // Or some other default/error value
	}

	int32 PixelIndex = GetPixelIndex(PixelCoords);
	int32 BytesPerPixel = GetBytesPerPixel();

	switch (Config->DataFormat)
	{
		case EDataFormat::R8:
			return FLinearColor(RawData[PixelIndex] / 255.0f, 0.0f, 0.0f, 0.0f);
		case EDataFormat::R16F:
			return FLinearColor(*((FFloat16*)&RawData[PixelIndex]), 0.0f, 0.0f, 0.0f);
		case EDataFormat::RGBA8:
			return FLinearColor(
				RawData[PixelIndex] / 255.0f,
				RawData[PixelIndex + 1] / 255.0f,
				RawData[PixelIndex + 2] / 255.0f,
				RawData[PixelIndex + 3] / 255.0f
			);
		case EDataFormat::RGBA16F:
			return FLinearColor(
				*((FFloat16*)&RawData[PixelIndex]),
				*((FFloat16*)&RawData[PixelIndex + 2]),
				*((FFloat16*)&RawData[PixelIndex + 4]),
				*((FFloat16*)&RawData[PixelIndex + 6])
			);
		default:
			return FLinearColor::Black;
	}
}

void UWorldDataLayer::SetValueAtPixel(const FIntPoint& PixelCoords, const FLinearColor& NewValue)
{
	if (!RawData.IsValidIndex(GetPixelIndex(PixelCoords)))
	{
		return; // Out of bounds
	}

	int32 PixelIndex = GetPixelIndex(PixelCoords);

	switch (Config->DataFormat)
	{
		case EDataFormat::R8:
			RawData[PixelIndex] = FMath::RoundToInt(NewValue.R * 255.0f);
			break;
		case EDataFormat::R16F:
			*((FFloat16*)&RawData[PixelIndex]) = FFloat16(NewValue.R);
			break;
		case EDataFormat::RGBA8:
			RawData[PixelIndex] = FMath::RoundToInt(NewValue.R * 255.0f);
			RawData[PixelIndex + 1] = FMath::RoundToInt(NewValue.G * 255.0f);
			RawData[PixelIndex + 2] = FMath::RoundToInt(NewValue.B * 255.0f);
			RawData[PixelIndex + 3] = FMath::RoundToInt(NewValue.A * 255.0f);
			break;
		case EDataFormat::RGBA16F:
			*((FFloat16*)&RawData[PixelIndex]) = FFloat16(NewValue.R);
			*((FFloat16*)&RawData[PixelIndex + 2]) = FFloat16(NewValue.G);
			*((FFloat16*)&RawData[PixelIndex + 4]) = FFloat16(NewValue.B);
			*((FFloat16*)&RawData[PixelIndex + 6]) = FFloat16(NewValue.A);
			break;
	}

	bIsDirty = true;
}

int32 UWorldDataLayer::GetPixelIndex(const FIntPoint& PixelCoords) const
{
	return (PixelCoords.Y * Resolution.X + PixelCoords.X) * GetBytesPerPixel();
}

int32 UWorldDataLayer::GetBytesPerPixel() const
{
	switch (Config->DataFormat)
	{
		case EDataFormat::R8:
			return 1;
		case EDataFormat::R16F:
			return 2; // FFloat16 is 2 bytes
		case EDataFormat::RGBA8:
			return 4;
		case EDataFormat::RGBA16F:
			return 8; // 4 FFloat16s = 8 bytes
		default:
			return 0; // Should not happen
	}
}
