#pragma once

#include "CoreMinimal.h"

class USkeletalMesh;

/**
 * Finds the car model at runtime, so the game works whatever folder the Vehicle content
 * pack (or your own car) was imported to.
 */
namespace VMVehicleAssets
{
	struct FResolved
	{
		USkeletalMesh* Mesh = nullptr;
		UClass* AnimClass = nullptr;
	};

	/**
	 * Mesh and animation Blueprint to use for cars: Project Settings > Vehicular Cars first,
	 * otherwise the best-matching skeletal mesh found under /Game (preferring "SportsCar").
	 */
	VEHICULAR_API FResolved Resolve();

	/** Works out the four wheel bones (FL, FR, RL, RR) from the skeleton's bone names and positions. */
	VEHICULAR_API bool FindWheelBones(const USkeletalMesh* Mesh, FName OutWheels[4]);
}
