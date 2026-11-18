#include "VMCheckpoint.h"
#include "VMGameMode.h"
#include "VMUtil.h"
#include "VMVehiclePawn.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

static const FLinearColor IdleColor(0.25f, 0.25f, 0.28f);
static const FLinearColor NextColor(0.1f, 1.f, 0.25f);

AVMCheckpoint::AVMCheckpoint()
{
	PrimaryActorTick.bCanEverTick = false;

	Trigger = CreateDefaultSubobject<UBoxComponent>(TEXT("Trigger"));
	Trigger->SetCollisionProfileName(TEXT("Trigger"));
	Trigger->SetGenerateOverlapEvents(true);
	RootComponent = Trigger;

	UStaticMesh* Cylinder = VMUtil::Shape(TEXT("Cylinder"));
	UStaticMesh* Cube = VMUtil::Shape(TEXT("Cube"));

	PostLeft = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PostLeft"));
	PostRight = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PostRight"));
	Banner = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Banner"));
	for (UStaticMeshComponent* Part : { PostLeft.Get(), PostRight.Get(), Banner.Get() })
	{
		Part->SetupAttachment(Trigger);
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	PostLeft->SetStaticMesh(Cylinder);
	PostRight->SetStaticMesh(Cylinder);
	Banner->SetStaticMesh(Cube);

	Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
	Label->SetupAttachment(Trigger);
	Label->SetHorizontalAlignment(EHTA_Center);
	Label->SetVerticalAlignment(EVRTA_TextCenter);
	Label->SetWorldSize(180.f);
	Label->SetTextRenderColor(FColor::White);

	LayoutGate();
}

void AVMCheckpoint::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	LayoutGate();
}

void AVMCheckpoint::LayoutGate()
{
	Trigger->SetBoxExtent(FVector(150.f, GateHalfWidth, 400.f));
	// The actor sits on the ground; the box is centred 400cm up.
	PostLeft->SetRelativeLocation(FVector(0.f, -GateHalfWidth, 0.f));
	PostRight->SetRelativeLocation(FVector(0.f, GateHalfWidth, 0.f));
	PostLeft->SetRelativeScale3D(FVector(0.6f, 0.6f, 7.f));
	PostRight->SetRelativeScale3D(FVector(0.6f, 0.6f, 7.f));
	Banner->SetRelativeLocation(FVector(0.f, 0.f, 330.f));
	Banner->SetRelativeScale3D(FVector(0.3f, GateHalfWidth * 2.f / 100.f, 0.8f));
	Label->SetRelativeLocation(FVector(-20.f, 0.f, 330.f));
	Label->SetRelativeRotation(FRotator(0.f, 180.f, 0.f));
	Label->SetText(FText::FromString(Index == 0 ? TEXT("START / FINISH") : FString::Printf(TEXT("CHECKPOINT %d"), Index)));
}

void AVMCheckpoint::BeginPlay()
{
	Super::BeginPlay();
	LayoutGate();
	Trigger->OnComponentBeginOverlap.AddDynamic(this, &AVMCheckpoint::OnTriggerOverlap);

	Materials.Reset();
	for (UStaticMeshComponent* Part : { PostLeft.Get(), PostRight.Get(), Banner.Get() })
	{
		Materials.Add(VMUtil::Paint(Part, IdleColor));
	}
	SetHighlighted(bIsHighlighted);
}

void AVMCheckpoint::SetHighlighted(bool bHighlighted)
{
	bIsHighlighted = bHighlighted;
	for (UMaterialInstanceDynamic* MID : Materials)
	{
		if (MID)
		{
			MID->SetVectorParameterValue(TEXT("Color"), bHighlighted ? NextColor : IdleColor);
		}
	}
}

void AVMCheckpoint::OnTriggerOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (AVMVehiclePawn* Vehicle = Cast<AVMVehiclePawn>(OtherActor))
	{
		if (AVMGameMode* GM = GetWorld()->GetAuthGameMode<AVMGameMode>())
		{
			GM->OnCheckpointReached(Vehicle, Index);
		}
	}
}
