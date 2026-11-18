#include "VMOilSlick.h"
#include "VMUtil.h"
#include "VMVehiclePawn.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"

AVMOilSlick::AVMOilSlick()
{
	Trigger = CreateDefaultSubobject<UBoxComponent>(TEXT("Trigger"));
	Trigger->SetBoxExtent(FVector(300.f, 300.f, 80.f));
	Trigger->SetCollisionProfileName(TEXT("Trigger"));
	Trigger->SetGenerateOverlapEvents(true);
	RootComponent = Trigger;

	Puddle = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Puddle"));
	Puddle->SetupAttachment(Trigger);
	Puddle->SetStaticMesh(VMUtil::Shape(TEXT("Cylinder")));
	Puddle->SetRelativeLocation(FVector(0.f, 0.f, -78.f));
	Puddle->SetRelativeScale3D(FVector(6.f, 6.f, 0.02f));
	Puddle->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	InitialLifeSpan = 25.f;
}

void AVMOilSlick::BeginPlay()
{
	Super::BeginPlay();
	// Lift the trigger so its bottom sits on the ground where it was dropped.
	AddActorWorldOffset(FVector(0.f, 0.f, 80.f));
	VMUtil::Paint(Puddle, FLinearColor(0.01f, 0.01f, 0.012f));
	Trigger->OnComponentBeginOverlap.AddDynamic(this, &AVMOilSlick::OnTriggerOverlap);
}

void AVMOilSlick::OnTriggerOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	AVMVehiclePawn* Vehicle = Cast<AVMVehiclePawn>(OtherActor);
	if (!Vehicle)
	{
		return;
	}
	// The car that dropped it gets a second to drive clear.
	if (Vehicle == GetOwner() && GetGameTimeSinceCreation() < 1.f)
	{
		return;
	}
	Vehicle->ApplySlip(SlipSeconds);
}
