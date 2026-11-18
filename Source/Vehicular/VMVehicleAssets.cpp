#include "VMVehicleAssets.h"
#include "Vehicular.h"
#include "VMCarCatalog.h"

#include "Animation/AnimBlueprintGeneratedClass.h"
#include "Animation/AnimInstance.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Engine/SkeletalMesh.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"

namespace
{
	IAssetRegistry& Registry()
	{
		IAssetRegistry& AR = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
		// In the editor the registry scans in the background; make sure it's finished.
		if (AR.IsLoadingAssets())
		{
			AR.SearchAllAssets(true);
		}
		return AR;
	}

	int32 ScoreMesh(const FAssetData& Asset)
	{
		const FString Name = Asset.AssetName.ToString().ToLower();
		const FString Path = Asset.PackagePath.ToString().ToLower();
		// Never pick characters or loose wheel meshes.
		for (const TCHAR* Bad : { TEXT("mannequin"), TEXT("manny"), TEXT("quinn"), TEXT("wheel"), TEXT("tire"), TEXT("tyre"), TEXT("character") })
		{
			if (Name.Contains(Bad))
			{
				return 0;
			}
		}
		int32 Score = 0;
		if (Name.Contains(TEXT("sportscar")) || Name.Contains(TEXT("sports_car"))) Score += 100;
		if (Name.Contains(TEXT("car"))) Score += 30;
		if (Name.Contains(TEXT("vehicle")) || Name.Contains(TEXT("offroad")) || Name.Contains(TEXT("truck"))) Score += 25;
		if (Path.Contains(TEXT("vehicle"))) Score += 20;
		return Score;
	}

	/** The name with prefixes like SKM_/SK_ removed, e.g. "sportscar". */
	FString CoreName(const USkeletalMesh* Mesh)
	{
		FString Name = Mesh->GetName().ToLower();
		for (const TCHAR* Prefix : { TEXT("skm_"), TEXT("sk_"), TEXT("sm_") })
		{
			Name.RemoveFromStart(Prefix);
		}
		return Name;
	}

	UClass* FindAnimClassFor(USkeletalMesh* Mesh)
	{
		IAssetRegistry& AR = Registry();
		const FString Folder = FPackageName::GetLongPackagePath(Mesh->GetOutermost()->GetName());
		const FString Parent = FPackageName::GetLongPackagePath(Folder);
		const FString Core = CoreName(Mesh);

		UClass* Best = nullptr;
		int32 BestScore = 0;
		for (const FString& Search : { Folder, Parent })
		{
			if (Search.IsEmpty() || Search == TEXT("/"))
			{
				continue;
			}
			FARFilter Filter;
			Filter.PackagePaths.Add(*Search);
			Filter.bRecursivePaths = true;
			TArray<FAssetData> Assets;
			AR.GetAssets(Filter, Assets);

			for (const FAssetData& Asset : Assets)
			{
				const FString Name = Asset.AssetName.ToString().ToLower();
				if (!Name.StartsWith(TEXT("abp")) && !Name.Contains(TEXT("anim")))
				{
					continue;
				}
				// An Animation Blueprint's class lives in the same package with a _C suffix.
				UClass* Class = LoadClass<UAnimInstance>(nullptr, *(Asset.GetObjectPathString() + TEXT("_C")), nullptr, LOAD_NoWarn | LOAD_Quiet);
				if (!Class)
				{
					continue;
				}
				// Must be made for this car's skeleton.
				if (const UAnimBlueprintGeneratedClass* Generated = Cast<UAnimBlueprintGeneratedClass>(Class))
				{
					if (Generated->TargetSkeleton && Generated->TargetSkeleton != Mesh->GetSkeleton())
					{
						continue;
					}
				}
				const int32 Score = 10 + (Name.Contains(Core) ? 50 : 0) + (Name.StartsWith(TEXT("abp")) ? 5 : 0);
				if (Score > BestScore)
				{
					BestScore = Score;
					Best = Class;
				}
			}
			if (Best)
			{
				break;
			}
		}
		return Best;
	}
}

VMVehicleAssets::FResolved VMVehicleAssets::Resolve()
{
	// Cache a successful result; keep retrying while nothing is found (e.g. pack just imported).
	static FResolved Cached;
	if (Cached.Mesh)
	{
		return Cached;
	}

	FResolved Result;
	const UVMCarCatalog* Catalog = GetDefault<UVMCarCatalog>();
	if (!Catalog->DefaultVehicleMesh.IsNull())
	{
		Result.Mesh = Catalog->DefaultVehicleMesh.LoadSynchronous();
	}
	if (!Catalog->DefaultVehicleAnimClass.IsNull())
	{
		Result.AnimClass = Catalog->DefaultVehicleAnimClass.LoadSynchronous();
	}

	if (!Result.Mesh)
	{
		FARFilter Filter;
		Filter.ClassPaths.Add(USkeletalMesh::StaticClass()->GetClassPathName());
		Filter.PackagePaths.Add(TEXT("/Game"));
		Filter.bRecursivePaths = true;
		Filter.bRecursiveClasses = true;
		TArray<FAssetData> Meshes;
		Registry().GetAssets(Filter, Meshes);

		const FAssetData* Best = nullptr;
		int32 BestScore = 0;
		for (const FAssetData& Asset : Meshes)
		{
			const int32 Score = ScoreMesh(Asset);
			if (Score > BestScore)
			{
				BestScore = Score;
				Best = &Asset;
			}
		}
		if (Best)
		{
			Result.Mesh = Cast<USkeletalMesh>(Best->GetAsset());
		}
	}

	if (Result.Mesh && !Result.AnimClass)
	{
		Result.AnimClass = FindAnimClassFor(Result.Mesh);
	}

	if (Result.Mesh)
	{
		UE_LOG(LogVehicular, Log, TEXT("Car model: %s  (animation: %s)"), *Result.Mesh->GetPathName(), Result.AnimClass ? *Result.AnimClass->GetPathName() : TEXT("none - wheels won't turn visually"));
		Cached = Result;
	}
	else
	{
		UE_LOG(LogVehicular, Warning, TEXT("No car model found under /Game. Add the Vehicle content pack, or set one in Project Settings > Vehicular Cars."));
	}
	return Result;
}

bool VMVehicleAssets::FindWheelBones(const USkeletalMesh* Mesh, FName OutWheels[4])
{
	if (!Mesh)
	{
		return false;
	}
	const FReferenceSkeleton& Ref = Mesh->GetRefSkeleton();
	const TArray<FTransform>& Pose = Ref.GetRefBonePose();

	auto ComponentSpace = [&](int32 Index)
	{
		FTransform T = Pose[Index];
		for (int32 P = Ref.GetParentIndex(Index); P != INDEX_NONE; P = Ref.GetParentIndex(P))
		{
			T = T * Pose[P];
		}
		return T.GetLocation();
	};

	// Candidate bones: anything named "wheel"; prefer the physics ones (e.g. Phys_Wheel_FL) if present.
	TArray<int32> Candidates;
	bool bAnyPhys = false;
	for (int32 i = 0; i < Ref.GetNum(); ++i)
	{
		const FString Name = Ref.GetBoneName(i).ToString().ToLower();
		if (Name.Contains(TEXT("wheel")))
		{
			Candidates.Add(i);
			bAnyPhys |= Name.Contains(TEXT("phys"));
		}
	}
	if (bAnyPhys)
	{
		Candidates.RemoveAll([&](int32 i) { return !Ref.GetBoneName(i).ToString().ToLower().Contains(TEXT("phys")); });
	}
	if (Candidates.Num() < 4)
	{
		return false;
	}

	// Cars face +X; +Y is right. Order matches the wheel setups: FL, FR, RL, RR.
	const FVector2D Corners[4] = { FVector2D(1, -1), FVector2D(1, 1), FVector2D(-1, -1), FVector2D(-1, 1) };
	for (int32 Corner = 0; Corner < 4; ++Corner)
	{
		int32 BestBone = INDEX_NONE;
		float BestReach = -1.f;
		for (int32 Bone : Candidates)
		{
			const FVector P = ComponentSpace(Bone);
			if (FMath::Sign(P.X) != Corners[Corner].X || FMath::Sign(P.Y) != Corners[Corner].Y)
			{
				continue;
			}
			const float Reach = FMath::Abs(P.X) + FMath::Abs(P.Y);
			if (Reach > BestReach)
			{
				BestReach = Reach;
				BestBone = Bone;
			}
		}
		if (BestBone == INDEX_NONE)
		{
			return false;
		}
		OutWheels[Corner] = Ref.GetBoneName(BestBone);
	}
	return true;
}
