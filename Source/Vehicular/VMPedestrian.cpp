#include "VMPedestrian.h"
#include "VMGameMode.h"
#include "VMUtil.h"
#include "VMVehiclePawn.h"
#include "AIController.h"
#include "Engine/CollisionProfile.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

AVMPedestrian::AVMPedestrian(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;

	UCapsuleComponent* Capsule = GetCapsuleComponent();
	Capsule->InitCapsuleSize(35.f, 90.f);
	// Cars pass through the capsule as an overlap so we can decide what happens.
	Capsule->SetCollisionResponseToChannel(ECC_Vehicle, ECR_Overlap);
	Capsule->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Overlap);
	Capsule->SetGenerateOverlapEvents(true);

	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(Capsule);
	Body->SetStaticMesh(VMUtil::Shape(TEXT("Cylinder")));
	Body->SetRelativeLocation(FVector(0.f, 0.f, -20.f));
	Body->SetRelativeScale3D(FVector(0.5f, 0.5f, 1.3f));
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Head = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Head"));
	Head->SetupAttachment(Capsule);
	Head->SetStaticMesh(VMUtil::Shape(TEXT("Sphere")));
	Head->SetRelativeLocation(FVector(0.f, 0.f, 65.f));
	Head->SetRelativeScale3D(FVector(0.38f));
	Head->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Splat = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Splat"));
	Splat->SetupAttachment(Capsule);
	Splat->SetStaticMesh(VMUtil::Shape(TEXT("Cylinder")));
	Splat->SetRelativeLocation(FVector(0.f, 0.f, -89.f));
	Splat->SetRelativeScale3D(FVector(2.2f, 1.6f, 0.01f));
	Splat->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Splat->SetVisibility(false);

	bUseControllerRotationYaw = false;
	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->bOrientRotationToMovement = true;
	Move->RotationRate = FRotator(0.f, 540.f, 0.f);
	Move->MaxWalkSpeed = WalkSpeed;

	AIControllerClass = AAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
}

void AVMPedestrian::BeginPlay()
{
	Super::BeginPlay();
	GetCapsuleComponent()->OnComponentBeginOverlap.AddDynamic(this, &AVMPedestrian::OnCapsuleOverlap);

	const FLinearColor Shirt = FLinearColor::MakeRandomColor();
	const float Skin = FMath::FRandRange(0.25f, 0.95f);
	VMUtil::Paint(Body, Shirt);
	VMUtil::Paint(Head, FLinearColor(Skin, Skin * 0.75f, Skin * 0.6f));
	VMUtil::Paint(Splat, FLinearColor(0.35f, 0.f, 0.f));

	JukeSign = FMath::RandBool() ? 1.f : -1.f;
	ThinkTimer = FMath::FRandRange(0.f, 2.f);
	PickWanderTarget();
}

AVMVehiclePawn* AVMPedestrian::FindThreat() const
{
	const AVMGameMode* GM = GetWorld()->GetAuthGameMode<AVMGameMode>();
	if (!GM)
	{
		return nullptr;
	}
	AVMVehiclePawn* Best = nullptr;
	float BestDist = PanicRadius;
	for (AVMVehiclePawn* Car : GM->GetAllVehicles())
	{
		if (!Car || Car->IsWrecked() || Car->GetVelocity().Size() < 400.f)
		{
			continue;
		}
		const float Dist = FVector::Dist(Car->GetActorLocation(), GetActorLocation());
		if (Dist < BestDist)
		{
			BestDist = Dist;
			Best = Car;
		}
	}
	return Best;
}

void AVMPedestrian::PickWanderTarget()
{
	const FVector2D Offset = FVector2D(FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-1.f, 1.f)).GetSafeNormal() * FMath::FRandRange(400.f, 1500.f);
	WanderTarget = GetActorLocation() + FVector(Offset.X, Offset.Y, 0.f);
	ThinkTimer = FMath::FRandRange(3.f, 8.f);
}

void AVMPedestrian::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bDead)
	{
		return;
	}

	ThinkTimer -= DeltaSeconds;
	PanicTime -= DeltaSeconds;

	if (AVMVehiclePawn* Threat = FindThreat())
	{
		// Run away from the car, veering to one side, which reads better than running straight ahead of it.
		const FVector Away = (GetActorLocation() - Threat->GetActorLocation()).GetSafeNormal2D();
		FleeDirection = Away.RotateAngleAxis(40.f * JukeSign, FVector::UpVector);
		PanicTime = 2.f;
	}

	UCharacterMovementComponent* Move = GetCharacterMovement();
	if (PanicTime > 0.f)
	{
		Move->MaxWalkSpeed = RunSpeed;
		AddMovementInput(FleeDirection);
		return;
	}

	Move->MaxWalkSpeed = WalkSpeed;
	const FVector ToTarget = (WanderTarget - GetActorLocation()) * FVector(1.f, 1.f, 0.f);
	if (ThinkTimer <= 0.f || ToTarget.Size() < 100.f)
	{
		PickWanderTarget();
		return;
	}
	AddMovementInput(ToTarget.GetSafeNormal());
}

void AVMPedestrian::OnCapsuleOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	AVMVehiclePawn* Car = Cast<AVMVehiclePawn>(OtherActor);
	if (!Car || bDead)
	{
		return;
	}
	if (Car->GetVelocity().Size() >= LethalSpeed)
	{
		Knockdown(Car);
	}
	else
	{
		// Slow car: hop out of the way.
		const FVector Side = (GetActorLocation() - Car->GetActorLocation()).GetSafeNormal2D();
		LaunchCharacter(Side * 500.f + FVector(0.f, 0.f, 250.f), true, true);
	}
}

void AVMPedestrian::Knockdown(AVMVehiclePawn* Killer)
{
	if (bDead)
	{
		return;
	}
	bDead = true;

	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();

	const FVector Launch = (Killer ? Killer->GetVelocity() * 0.7f : FVector::ZeroVector) + FVector(0.f, 0.f, 650.f);
	for (UStaticMeshComponent* Part : { Body.Get(), Head.Get() })
	{
		Part->SetCollisionProfileName(UCollisionProfile::PhysicsActor_ProfileName);
		Part->SetCollisionResponseToChannel(ECC_Vehicle, ECR_Ignore);
		Part->SetSimulatePhysics(true);
		Part->AddImpulse(Launch + FMath::VRand() * 200.f, NAME_None, true);
		Part->AddAngularImpulseInDegrees(FMath::VRand() * 720.f, NAME_None, true);
	}
	Splat->SetVisibility(true);

	if (AVMGameMode* GM = GetWorld()->GetAuthGameMode<AVMGameMode>())
	{
		GM->OnPedestrianKilled(this, Killer);
	}
	SetLifeSpan(12.f);
}
