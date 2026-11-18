#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "VMPlayerController.generated.h"

UCLASS()
class VEHICULAR_API AVMPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AVMPlayerController();
	virtual void BeginPlay() override;
};
