// Copyright 2024 RancMake Technologies. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "ActorFactories/ActorFactoryVolume.h"
#include "ActorFactoryWorldDataVolume.generated.h"

UCLASS()
class UActorFactoryWorldDataVolume : public UActorFactoryVolume
{
    GENERATED_UCLASS_BODY()

public:
    virtual bool CanCreateActorFrom(const FAssetData& AssetData, FText& OutErrorMsg) override;
    virtual void PostSpawnActor(UObject* Asset, AActor* NewActor) override;
    virtual UObject* GetAssetFromActorInstance(AActor* ActorInstance) override;
};
