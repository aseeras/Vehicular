#pragma once

#include "CoreMinimal.h"

class UStaticMesh;
class UMeshComponent;
class UMaterialInstanceDynamic;

/** Small helpers so the whole game can run on engine basic shapes, with no project assets. */
namespace VMUtil
{
	/** Loads /Engine/BasicShapes/<Name> (Cube, Cylinder, Sphere, Cone, Plane). */
	VEHICULAR_API UStaticMesh* Shape(const TCHAR* Name);

	/** Gives a mesh component a solid colour using the engine's basic shape material. */
	VEHICULAR_API UMaterialInstanceDynamic* Paint(UMeshComponent* Comp, const FLinearColor& Color);

	/** Finds static ground below (or above) a point. */
	VEHICULAR_API bool GroundAt(const UWorld* World, const FVector& Point, FVector& OutGround);
}
