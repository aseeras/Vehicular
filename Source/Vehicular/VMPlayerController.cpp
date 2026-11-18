#include "VMPlayerController.h"

AVMPlayerController::AVMPlayerController()
{
	// Keep processing input while paused so the pause key can unpause.
	bShouldPerformFullTickWhenPaused = true;
}

void AVMPlayerController::BeginPlay()
{
	Super::BeginPlay();
	SetInputMode(FInputModeGameOnly());
	bShowMouseCursor = false;
}
