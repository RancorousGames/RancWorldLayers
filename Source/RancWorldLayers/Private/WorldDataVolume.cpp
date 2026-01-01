#include "WorldDataVolume.h"
#include "Components/BrushComponent.h"
#include "WorldLayersSubsystem.h"

AWorldDataVolume::AWorldDataVolume()
{
	// Make the volume visible in the editor but with no collision.
	GetBrushComponent()->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	GetBrushComponent()->SetCanEverAffectNavigation(false);

#if WITH_EDITOR
	GetBrushComponent()->SetIsVisualizationComponent(true);
#endif

	bIsEditorOnlyActor = false;
	
	bColored = true;
	BrushColor.R = 25;
	BrushColor.G = 255;
	BrushColor.B = 25;
	BrushColor.A = 255;
}

void AWorldDataVolume::PostActorCreated()
{
	Super::PostActorCreated();
	UE_LOG(LogTemp, Log, TEXT("AWorldDataVolume: PostActorCreated. Registering with subsystem."));
	if (UWorldLayersSubsystem* WLS = UWorldLayersSubsystem::Get(this))
	{
		WLS->InitializeFromVolume(this);
	}
}

void AWorldDataVolume::PostLoad()
{
	Super::PostLoad();
	UE_LOG(LogTemp, Log, TEXT("AWorldDataVolume: PostLoad. Registering with subsystem."));
	if (UWorldLayersSubsystem* WLS = UWorldLayersSubsystem::Get(this))
	{
		WLS->InitializeFromVolume(this);
	}
}

bool AWorldDataVolume::ShouldCheckCollisionComponentForErrors() const
{
	return false;
}
