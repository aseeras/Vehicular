#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "VMHUD.generated.h"

class AVMGameMode;
class AVMVehiclePawn;
class UFont;

/** Canvas-drawn HUD, so the game needs no UMG assets. */
UCLASS()
class VEHICULAR_API AVMHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

private:
	void Label(const FString& Text, float X, float Y, const FLinearColor& Color, float Scale, bool bCenter = false);
	void Bar(float X, float Y, float W, float H, float Fraction, const FLinearColor& Color);
	bool WorldToScreen(const FVector& World, FVector2D& OutScreen) const;

	void DrawTopLeft(AVMGameMode* GM);
	void DrawTopRight(AVMGameMode* GM, AVMVehiclePawn* Car);
	void DrawBottomLeft(AVMVehiclePawn* Car);
	void DrawDamage(AVMVehiclePawn* Car);
	void DrawFeed(AVMGameMode* GM);
	void DrawCheckpointMarker(AVMGameMode* GM, AVMVehiclePawn* Car);
	void DrawRivalTags(AVMGameMode* GM, AVMVehiclePawn* Car);
	void DrawCenter(AVMGameMode* GM, AVMVehiclePawn* Car);

	float S = 1.f;
	UFont* Font = nullptr;
};
