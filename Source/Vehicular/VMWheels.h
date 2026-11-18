#pragma once

#include "CoreMinimal.h"
#include "ChaosVehicleWheel.h"
#include "VMWheels.generated.h"

UCLASS()
class VEHICULAR_API UVMWheelFront : public UChaosVehicleWheel
{
	GENERATED_BODY()
public:
	UVMWheelFront();
};

UCLASS()
class VEHICULAR_API UVMWheelRear : public UChaosVehicleWheel
{
	GENERATED_BODY()
public:
	UVMWheelRear();
};
