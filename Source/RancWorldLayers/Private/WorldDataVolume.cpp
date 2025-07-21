#include "WorldDataVolume.h"
#include "Components/BrushComponent.h"

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

bool AWorldDataVolume::ShouldCheckCollisionComponentForErrors() const
{
	return false;
}