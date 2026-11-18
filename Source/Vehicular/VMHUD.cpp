#include "VMHUD.h"
#include "VMCheckpoint.h"
#include "VMDamageComponent.h"
#include "VMGameMode.h"
#include "VMVehiclePawn.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	FLinearColor HealthColor(float Fraction)
	{
		return FLinearColor::LerpUsingHSV(FLinearColor(1.f, 0.05f, 0.05f), FLinearColor(0.1f, 1.f, 0.2f), FMath::Clamp(Fraction, 0.f, 1.f));
	}

	FString Ordinal(int32 N)
	{
		const int32 Mod100 = N % 100;
		const TCHAR* Suffix = TEXT("th");
		if (Mod100 < 11 || Mod100 > 13)
		{
			switch (N % 10)
			{
			case 1: Suffix = TEXT("st"); break;
			case 2: Suffix = TEXT("nd"); break;
			case 3: Suffix = TEXT("rd"); break;
			default: break;
			}
		}
		return FString::Printf(TEXT("%d%s"), N, Suffix);
	}
}

void AVMHUD::Label(const FString& Text, float X, float Y, const FLinearColor& Color, float Scale, bool bCenter)
{
	const float FinalScale = Scale * S;
	if (bCenter)
	{
		float W, H;
		GetTextSize(Text, W, H, Font, FinalScale);
		X -= W * 0.5f;
		Y -= H * 0.5f;
	}
	DrawText(Text, FLinearColor(0.f, 0.f, 0.f, 0.8f), X + 2.f * S, Y + 2.f * S, Font, FinalScale);
	DrawText(Text, Color, X, Y, Font, FinalScale);
}

void AVMHUD::Bar(float X, float Y, float W, float H, float Fraction, const FLinearColor& Color)
{
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.6f), X, Y, W, H);
	DrawRect(Color, X, Y, W * FMath::Clamp(Fraction, 0.f, 1.f), H);
}

bool AVMHUD::WorldToScreen(const FVector& World, FVector2D& OutScreen) const
{
	if (!PlayerOwner || !PlayerOwner->PlayerCameraManager)
	{
		return false;
	}
	const FVector CamLoc = PlayerOwner->PlayerCameraManager->GetCameraLocation();
	const FVector CamDir = PlayerOwner->PlayerCameraManager->GetCameraRotation().Vector();
	if (FVector::DotProduct(World - CamLoc, CamDir) <= 0.f)
	{
		return false;
	}
	const FVector P = const_cast<AVMHUD*>(this)->Project(World, false);
	OutScreen = FVector2D(P.X, P.Y);
	return true;
}

void AVMHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas)
	{
		return;
	}

	S = Canvas->ClipY / 1080.f;
	Font = GEngine->GetLargeFont();

	AVMGameMode* GM = GetWorld()->GetAuthGameMode<AVMGameMode>();
	AVMVehiclePawn* Car = Cast<AVMVehiclePawn>(GetOwningPawn());
	if (!GM)
	{
		return;
	}

	if (Car)
	{
		DrawRivalTags(GM, Car);
		DrawCheckpointMarker(GM, Car);
		DrawBottomLeft(Car);
		DrawDamage(Car);
	}
	DrawTopLeft(GM);
	DrawTopRight(GM, Car);
	DrawFeed(GM);
	DrawCenter(GM, Car);
}

void AVMHUD::DrawTopLeft(AVMGameMode* GM)
{
	const float X = 40.f * S;
	const float TimeLeft = GM->GetTimeLeft();
	const FLinearColor TimeColor = TimeLeft < 10.f ? FLinearColor(1.f, 0.2f, 0.2f) : FLinearColor::White;
	Label(FString::Printf(TEXT("%d:%02d"), FMath::FloorToInt(TimeLeft / 60.f), FMath::FloorToInt(FMath::Fmod(TimeLeft, 60.f))), X, 30.f * S, TimeColor, 3.f);
	Label(FString::Printf(TEXT("POINTS  %d / %d"), GM->GetPoints(), GM->TargetPoints), X, 110.f * S, FLinearColor(1.f, 0.8f, 0.2f), 1.4f);
	Label(FString::Printf(TEXT("Pedestrians %d   Wrecks %d / %d"), GM->GetPedestriansKilled(), GM->GetRivalsWrecked(), GM->GetRivals().Num()), X, 150.f * S, FLinearColor(0.85f, 0.85f, 0.85f), 1.1f);
	if (GM->GetCombo() > 1)
	{
		Label(FString::Printf(TEXT("COMBO x%d"), GM->GetCombo()), X, 185.f * S, FLinearColor(1.f, 0.45f, 0.1f), 1.6f);
	}
}

void AVMHUD::DrawTopRight(AVMGameMode* GM, AVMVehiclePawn* Car)
{
	if (!Car || GM->GetNumCheckpoints() < 2)
	{
		return;
	}
	const float X = Canvas->ClipX - 330.f * S;
	const int32 NumCars = GM->GetAllVehicles().Num();
	Label(FString::Printf(TEXT("POS  %s / %d"), *Ordinal(GM->GetRacePosition(Car)), NumCars), X, 30.f * S, FLinearColor::White, 1.8f);
	Label(FString::Printf(TEXT("LAP  %d / %d"), GM->GetLapFor(Car), GM->NumLaps), X, 80.f * S, FLinearColor::White, 1.5f);
	if (const AVMCheckpoint* Next = GM->GetNextCheckpointFor(Car))
	{
		Label(FString::Printf(TEXT("NEXT  %s"), Next->Index == 0 ? TEXT("FINISH") : *FString::FromInt(Next->Index)), X, 120.f * S, FLinearColor(0.3f, 1.f, 0.35f), 1.3f);
	}
}

void AVMHUD::DrawBottomLeft(AVMVehiclePawn* Car)
{
	const float X = 40.f * S;
	const float Bottom = Canvas->ClipY;
	Label(FString::Printf(TEXT("%3d km/h"), FMath::Abs(FMath::RoundToInt(Car->GetSpeedKmh()))), X, Bottom - 200.f * S, FLinearColor::White, 2.4f);

	Label(TEXT("NITRO"), X, Bottom - 120.f * S, FLinearColor(0.4f, 0.7f, 1.f), 1.f);
	Bar(X + 90.f * S, Bottom - 115.f * S, 220.f * S, 16.f * S, Car->GetNitroCharge(), FLinearColor(0.1f, 0.5f, 1.f));

	const EVMPickupType Held = Car->GetHeldPickup();
	Label(FString::Printf(TEXT("WEAPON  %s"), *VMPickups::GetName(Held)), X, Bottom - 90.f * S,
		Held == EVMPickupType::None ? FLinearColor(0.6f, 0.6f, 0.6f) : VMPickups::GetColor(Held), 1.1f);

	FString Status;
	if (Car->GetArmourTimeLeft() > 0.f) Status += FString::Printf(TEXT("ARMOUR %.0fs   "), Car->GetArmourTimeLeft());
	if (Car->GetSpikesTimeLeft() > 0.f) Status += FString::Printf(TEXT("SPIKES %.0fs   "), Car->GetSpikesTimeLeft());
	if (Car->IsStunned()) Status += TEXT("STUNNED   ");
	if (!Status.IsEmpty())
	{
		Label(Status, X, Bottom - 55.f * S, FLinearColor(1.f, 0.9f, 0.5f), 1.f);
	}
	if (Car->IsUpsideDown() && !Car->IsWrecked())
	{
		Label(TEXT("Press R / Y to recover"), Canvas->ClipX * 0.5f, Canvas->ClipY * 0.7f, FLinearColor(1.f, 0.7f, 0.2f), 1.5f, true);
	}
}

void AVMHUD::DrawDamage(AVMVehiclePawn* Car)
{
	const UVMDamageComponent* Dmg = Car->GetDamage();
	if (!Dmg)
	{
		return;
	}
	const float W = 70.f * S;
	const float H = 140.f * S;
	const float T = 12.f * S;
	const float CX = Canvas->ClipX - 140.f * S;
	const float CY = Canvas->ClipY - 140.f * S;
	const float L = CX - W * 0.5f;
	const float Top = CY - H * 0.5f;

	DrawRect(FLinearColor(0.1f, 0.1f, 0.1f, 0.7f), L, Top, W, H);
	DrawRect(HealthColor(Dmg->GetZoneFraction(EVMDamageZone::Front)), L, Top - T - 3.f * S, W, T);
	DrawRect(HealthColor(Dmg->GetZoneFraction(EVMDamageZone::Rear)), L, Top + H + 3.f * S, W, T);
	DrawRect(HealthColor(Dmg->GetZoneFraction(EVMDamageZone::Left)), L - T - 3.f * S, Top, T, H);
	DrawRect(HealthColor(Dmg->GetZoneFraction(EVMDamageZone::Right)), L + W + 3.f * S, Top, T, H);
	Label(FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Dmg->GetTotalFraction() * 100.f)), CX, CY, HealthColor(Dmg->GetTotalFraction()), 1.2f, true);
}

void AVMHUD::DrawFeed(AVMGameMode* GM)
{
	float Y = Canvas->ClipY * 0.32f;
	for (const FVMFeedMessage& Msg : GM->GetMessages())
	{
		FLinearColor C = Msg.Color;
		C.A = FMath::Clamp(Msg.TimeLeft, 0.f, 1.f);
		Label(Msg.Text, 40.f * S, Y, C, 1.2f);
		Y += 36.f * S;
	}
}

void AVMHUD::DrawCheckpointMarker(AVMGameMode* GM, AVMVehiclePawn* Car)
{
	const AVMCheckpoint* Next = GM->GetNextCheckpointFor(Car);
	if (!Next || !GM->IsRaceLive() || GM->GetNumCheckpoints() < 2)
	{
		return;
	}
	const FVector Target = Next->GetActorLocation() + FVector(0.f, 0.f, 700.f);
	const int32 Meters = FMath::RoundToInt(FVector::Dist(Car->GetActorLocation(), Target) / 100.f);
	FVector2D Screen;
	if (WorldToScreen(Target, Screen))
	{
		const float Margin = 60.f * S;
		Screen.X = FMath::Clamp(Screen.X, Margin, Canvas->ClipX - Margin);
		Screen.Y = FMath::Clamp(Screen.Y, Margin, Canvas->ClipY - Margin);
		Label(FString::Printf(TEXT("[ %d m ]"), Meters), Screen.X, Screen.Y, FLinearColor(0.3f, 1.f, 0.35f), 1.3f, true);
	}
	else
	{
		Label(FString::Printf(TEXT("CHECKPOINT BEHIND YOU  %d m"), Meters), Canvas->ClipX * 0.5f, Canvas->ClipY - 60.f * S, FLinearColor(0.3f, 1.f, 0.35f), 1.2f, true);
	}
}

void AVMHUD::DrawRivalTags(AVMGameMode* GM, AVMVehiclePawn* Car)
{
	for (const TObjectPtr<AVMVehiclePawn>& Rival : GM->GetRivals())
	{
		if (!IsValid(Rival) || FVector::Dist(Rival->GetActorLocation(), Car->GetActorLocation()) > 9000.f)
		{
			continue;
		}
		FVector2D Screen;
		if (!WorldToScreen(Rival->GetActorLocation() + FVector(0.f, 0.f, 260.f), Screen))
		{
			continue;
		}
		if (Rival->IsWrecked())
		{
			Label(FString::Printf(TEXT("%s (WRECKED)"), *Rival->DriverName), Screen.X, Screen.Y, FLinearColor(0.6f, 0.6f, 0.6f), 0.9f, true);
			continue;
		}
		const float Health = Rival->GetDamage()->GetTotalFraction();
		Label(Rival->DriverName, Screen.X, Screen.Y - 14.f * S, FLinearColor::White, 0.9f, true);
		Bar(Screen.X - 40.f * S, Screen.Y + 4.f * S, 80.f * S, 7.f * S, Health, HealthColor(Health));
	}
}

void AVMHUD::DrawCenter(AVMGameMode* GM, AVMVehiclePawn* Car)
{
	const float CX = Canvas->ClipX * 0.5f;
	const float CY = Canvas->ClipY * 0.4f;

	if (Car && !Car->HasVehicleMesh())
	{
		Label(TEXT("No car mesh found - add the Vehicle feature pack (see README)"), CX, Canvas->ClipY * 0.15f, FLinearColor(1.f, 0.2f, 0.2f), 1.4f, true);
	}

	switch (GM->GetMatchState())
	{
	case EVMMatchState::Countdown:
		Label(FString::FromInt(FMath::CeilToInt(GM->GetCountdownLeft())), CX, CY, FLinearColor(1.f, 0.85f, 0.2f), 6.f, true);
		break;
	case EVMMatchState::Won:
		Label(TEXT("VICTORY"), CX, CY, FLinearColor(0.3f, 1.f, 0.35f), 5.f, true);
		Label(GM->GetResultText(), CX, CY + 90.f * S, FLinearColor::White, 1.8f, true);
		Label(FString::Printf(TEXT("%d points   %d pedestrians   %d wrecks"), GM->GetPoints(), GM->GetPedestriansKilled(), GM->GetRivalsWrecked()), CX, CY + 140.f * S, FLinearColor(1.f, 0.8f, 0.2f), 1.3f, true);
		Label(TEXT("Press Enter / Back to play again"), CX, CY + 190.f * S, FLinearColor(0.8f, 0.8f, 0.8f), 1.2f, true);
		break;
	case EVMMatchState::Lost:
		Label(TEXT("DEFEAT"), CX, CY, FLinearColor(1.f, 0.2f, 0.15f), 5.f, true);
		Label(GM->GetResultText(), CX, CY + 90.f * S, FLinearColor::White, 1.8f, true);
		Label(FString::Printf(TEXT("%d points   %d pedestrians   %d wrecks"), GM->GetPoints(), GM->GetPedestriansKilled(), GM->GetRivalsWrecked()), CX, CY + 140.f * S, FLinearColor(1.f, 0.8f, 0.2f), 1.3f, true);
		Label(TEXT("Press Enter / Back to play again"), CX, CY + 190.f * S, FLinearColor(0.8f, 0.8f, 0.8f), 1.2f, true);
		break;
	default:
		break;
	}

	if (UGameplayStatics::IsGamePaused(this) && !GM->IsMatchOver())
	{
		Label(TEXT("PAUSED"), CX, CY, FLinearColor::White, 4.f, true);
	}
}
