#include "VMUtil.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Components/MeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"

UStaticMesh* VMUtil::Shape(const TCHAR* Name)
{
	const FString Path = FString::Printf(TEXT("/Engine/BasicShapes/%s.%s"), Name, Name);
	return LoadObject<UStaticMesh>(nullptr, *Path);
}

UMaterialInstanceDynamic* VMUtil::Paint(UMeshComponent* Comp, const FLinearColor& Color)
{
	if (!Comp)
	{
		return nullptr;
	}
	static const TCHAR* BasePath = TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial");
	UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, BasePath);
	if (!Base)
	{
		return nullptr;
	}
	UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Base, Comp);
	MID->SetVectorParameterValue(TEXT("Color"), Color);
	Comp->SetMaterial(0, MID);
	return MID;
}

bool VMUtil::GroundAt(const UWorld* World, const FVector& Point, FVector& OutGround)
{
	if (!World)
	{
		return false;
	}
	FHitResult Hit;
	const FVector Start = Point + FVector(0.f, 0.f, 5000.f);
	const FVector End = Point - FVector(0.f, 0.f, 10000.f);
	if (World->LineTraceSingleByObjectType(Hit, Start, End, FCollisionObjectQueryParams(ECC_WorldStatic)))
	{
		OutGround = Hit.ImpactPoint;
		return true;
	}
	return false;
}
