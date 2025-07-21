// Copyright 2024 RancMake Technologies. All rights reserved.

#include "ActorFactoryWorldDataVolume.h"
#include "WorldDataVolume.h"
#include "AssetRegistry/AssetData.h"

UActorFactoryWorldDataVolume::UActorFactoryWorldDataVolume(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    DisplayName = FText::FromString("World Data Volume");
    NewActorClass = AWorldDataVolume::StaticClass();
    bUseSurfaceOrientation = true;
}

bool UActorFactoryWorldDataVolume::CanCreateActorFrom(const FAssetData& AssetData, FText& OutErrorMsg)
{
    if (Super::CanCreateActorFrom(AssetData, OutErrorMsg))
    {
        return true;
    }

    if (AssetData.IsValid() && !AssetData.GetClass()->IsChildOf(AWorldDataVolume::StaticClass()))
    {
        OutErrorMsg = FText::FromString("A valid volume brush must be specified.");
        return false;
    }

    return true;
}

void UActorFactoryWorldDataVolume::PostSpawnActor(UObject* Asset, AActor* NewActor)
{
    Super::PostSpawnActor(Asset, NewActor);
}

UObject* UActorFactoryWorldDataVolume::GetAssetFromActorInstance(AActor* ActorInstance)
{
    return Super::GetAssetFromActorInstance(ActorInstance);
}
