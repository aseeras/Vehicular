#include "VMPickup.h"
#include "VMUtil.h"
#include "VMVehiclePawn.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "TimerManager.h"

AVMPickup::AVMPickup()
{
	PrimaryActorTick.bCanEverTick = true;

	Trigger = CreateDefaultSubobject<USphereComponent>(TEXT("Trigger"));
	Trigger->InitSphereRadius(260.f);
	Trigger->SetCollisionProfileName(TEXT("Trigger"));
	Trigger->SetGenerateOverlapEvents(true);
	RootComponent = Trigger;

	Crate = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Crate"));
	Crate->SetupAttachment(Trigger);
	Crate->SetStaticMesh(VMUtil::Shape(TEXT("Cube")));
	Crate->SetRelativeScale3D(FVector(1.1f));
	Crate->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
	Label->SetupAttachment(Trigger);
	Label->SetRelativeLocation(FVector(0.f, 0.f, 150.f));
	Label->SetHorizontalAlignment(EHTA_Center);
	Label->SetWorldSize(70.f);
}

void AVMPickup::BeginPlay()
{
	Super::BeginPlay();
	Rng.Initialize(GetUniqueID() ^ FMath::Rand());
	Trigger->OnComponentBeginOverlap.AddDynamic(this, &AVMPickup::OnTriggerOverlap);
	CrateMaterial = VMUtil::Paint(Crate, FLinearColor::White);
	Activate();
}

void AVMPickup::Activate()
{
	if (bRandomType)
	{
		Type = VMPickups::Random(Rng);
	}
	const FLinearColor Color = VMPickups::GetColor(Type);
	if (CrateMaterial)
	{
		CrateMaterial->SetVectorParameterValue(TEXT("Color"), Color);
	}
	Label->SetText(FText::FromString(VMPickups::GetName(Type)));
	Label->SetTextRenderColor(Color.ToFColor(true));

	bActive = true;
	SetActorHiddenInGame(false);
	Trigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
}

void AVMPickup::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bActive)
	{
		return;
	}
	Age += DeltaSeconds;
	Crate->SetRelativeLocation(FVector(0.f, 0.f, 90.f + FMath::Sin(Age * 2.5f) * 25.f));
	Crate->SetRelativeRotation(FRotator(25.f, Age * 90.f, 25.f));
	// Keep the label facing the player camera.
	if (const APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		if (PC->PlayerCameraManager)
		{
			const FVector ToCam = PC->PlayerCameraManager->GetCameraLocation() - Label->GetComponentLocation();
			Label->SetWorldRotation(FRotator(0.f, ToCam.Rotation().Yaw, 0.f));
		}
	}
}

void AVMPickup::OnTriggerOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	AVMVehiclePawn* Vehicle = Cast<AVMVehiclePawn>(OtherActor);
	if (!bActive || !Vehicle || Vehicle->IsWrecked())
	{
		return;
	}
	Vehicle->ReceivePickup(Type);

	bActive = false;
	SetActorHiddenInGame(true);
	Trigger->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetWorldTimerManager().SetTimer(RespawnTimer, this, &AVMPickup::Activate, RespawnDelay, false);
}
