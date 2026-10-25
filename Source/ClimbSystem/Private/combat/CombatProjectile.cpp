// Copyright Epic Games / Modular Combat System. All Rights Reserved.

#include "combat/CombatProjectile.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "Engine/DamageEvents.h"
#include "Curves/CurveFloat.h"
#include "Kismet/GameplayStatics.h"
#include "combat/DamageHandlerComponent.h"
#include "combat/WeaponBase.h"

ACombatProjectile::ACombatProjectile()
{
	PrimaryActorTick.bCanEverTick = true;

	CollisionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionSphere"));
	RootComponent = CollisionSphere;
	CollisionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CollisionSphere->SetCollisionObjectType(ECC_WorldDynamic);
	CollisionSphere->SetCollisionResponseToAllChannels(ECR_Block);
	CollisionSphere->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	CollisionSphere->SetGenerateOverlapEvents(false);

	ProjectileVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ProjectileVisual"));
	ProjectileVisual->SetupAttachment(CollisionSphere);
	ProjectileVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ProjectileVisual->SetGenerateOverlapEvents(false);

	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->UpdatedComponent = CollisionSphere;
	ProjectileMovement->InitialSpeed = 3200.0f;
	ProjectileMovement->MaxSpeed = 4200.0f;
	ProjectileMovement->ProjectileGravityScale = 0.15f;
	ProjectileMovement->bRotationFollowsVelocity = true;
	ProjectileMovement->bShouldBounce = false;
}

void ACombatProjectile::InitializeProjectile(const FProjectileConfig& InConfig, const FVector& Direction, AActor* InDamageInstigator, AWeaponBase* InSourceWeapon)
{
	ProjectileConfig = InConfig;
	InitialDirection = Direction.GetSafeNormal();
	DamageInstigator = InDamageInstigator;
	SourceWeapon = InSourceWeapon;
}

void ACombatProjectile::BeginPlay()
{
	Super::BeginPlay();

	ProjectileConfig.InitialSpeed = FMath::Max(0.0f, FMath::IsFinite(ProjectileConfig.InitialSpeed) ? ProjectileConfig.InitialSpeed : 0.0f);
	ProjectileConfig.MaxSpeed = FMath::Max(ProjectileConfig.InitialSpeed, FMath::IsFinite(ProjectileConfig.MaxSpeed) ? ProjectileConfig.MaxSpeed : ProjectileConfig.InitialSpeed);
	ProjectileConfig.GravityScale = FMath::IsFinite(ProjectileConfig.GravityScale) ? ProjectileConfig.GravityScale : 0.0f;
	ProjectileConfig.LifeSeconds = FMath::Max(0.05f, FMath::IsFinite(ProjectileConfig.LifeSeconds) ? ProjectileConfig.LifeSeconds : 8.0f);
	ProjectileConfig.CollisionRadius = FMath::Max(1.0f, FMath::IsFinite(ProjectileConfig.CollisionRadius) ? ProjectileConfig.CollisionRadius : 5.0f);
	ProjectileConfig.MinimumDamageMultiplier = FMath::Clamp(FMath::IsFinite(ProjectileConfig.MinimumDamageMultiplier) ? ProjectileConfig.MinimumDamageMultiplier : 0.0f, 0.0f, 1.0f);
	ProjectileConfig.FalloffStartDistance = FMath::Max(0.0f, ProjectileConfig.FalloffStartDistance);
	ProjectileConfig.FalloffEndDistance = FMath::Max(ProjectileConfig.FalloffStartDistance, ProjectileConfig.FalloffEndDistance);

	CollisionSphere->SetSphereRadius(ProjectileConfig.CollisionRadius);
	CollisionSphere->OnComponentHit.AddDynamic(this, &ACombatProjectile::HandleProjectileHit);
	if (DamageInstigator.IsValid())
	{
		CollisionSphere->IgnoreActorWhenMoving(DamageInstigator.Get(), true);
	}
	if (SourceWeapon.IsValid())
	{
		CollisionSphere->IgnoreActorWhenMoving(SourceWeapon.Get(), true);
	}

	ProjectileMovement->InitialSpeed = ProjectileConfig.InitialSpeed;
	ProjectileMovement->MaxSpeed = ProjectileConfig.MaxSpeed;
	ProjectileMovement->ProjectileGravityScale = ProjectileConfig.GravityScale;
	ProjectileMovement->Velocity = InitialDirection * ProjectileConfig.InitialSpeed;
	PreviousLocation = GetActorLocation();
	SetLifeSpan(ProjectileConfig.LifeSeconds);
}

void ACombatProjectile::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const FVector CurrentLocation = GetActorLocation();
	if (!CurrentLocation.ContainsNaN() && !PreviousLocation.ContainsNaN())
	{
		DistanceTravelled += FVector::Distance(PreviousLocation, CurrentLocation);
	}
	PreviousLocation = CurrentLocation;
}

float ACombatProjectile::CalculateFalloffMultiplier() const
{
	const float Minimum = FMath::Clamp(ProjectileConfig.MinimumDamageMultiplier, 0.0f, 1.0f);
	if (ProjectileConfig.FalloffMode == EProjectileFalloffMode::None || DistanceTravelled <= ProjectileConfig.FalloffStartDistance)
	{
		return 1.0f;
	}

	const float Range = ProjectileConfig.FalloffEndDistance - ProjectileConfig.FalloffStartDistance;
	const float Alpha = Range <= KINDA_SMALL_NUMBER ? 1.0f : FMath::Clamp((DistanceTravelled - ProjectileConfig.FalloffStartDistance) / Range, 0.0f, 1.0f);
	if (ProjectileConfig.FalloffMode == EProjectileFalloffMode::Curve && IsValid(ProjectileConfig.FalloffCurve))
	{
		return FMath::Clamp(ProjectileConfig.FalloffCurve->GetFloatValue(Alpha), Minimum, 1.0f);
	}
	return FMath::Lerp(1.0f, Minimum, Alpha);
}

void ACombatProjectile::HandleProjectileHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, FVector NormalImpulse, const FHitResult& Hit)
{
	if (bHasHit || !IsValid(OtherActor)) return;
	if (OtherActor == DamageInstigator.Get() || OtherActor == SourceWeapon.Get()) return;

	bHasHit = true;
	if (OtherActor != GetOwner())
	{
		ApplyHitDamage(OtherActor, Hit);
	}
	Destroy();
}

void ACombatProjectile::ApplyHitDamage(AActor* TargetActor, const FHitResult& Hit)
{
	if (!IsValid(TargetActor) || !DamageInstigator.IsValid()) return;

	FDamageInfo DamageInfo;
	DamageInfo.BaseDamage = ProjectileConfig.BaseDamage;
	DamageInfo.DamageMultiplier = ProjectileConfig.DamageMultiplier * CalculateFalloffMultiplier();
	DamageInfo.PoiseDamage = ProjectileConfig.PoiseDamage * CalculateFalloffMultiplier();
	DamageInfo.KnockbackImpulse = ProjectileConfig.KnockbackImpulse;
	DamageInfo.DamageCategory = ProjectileConfig.DamageCategory;
	DamageInfo.WeaponType = ProjectileConfig.WeaponType;
	DamageInfo.DamageCauser = SourceWeapon.IsValid() ? static_cast<AActor*>(SourceWeapon.Get()) : this;
	DamageInfo.InstigatorWeapon = SourceWeapon.Get();
	DamageInfo.Normalize();

	if (UDamageHandlerComponent* Handler = TargetActor->FindComponentByClass<UDamageHandlerComponent>())
	{
		Handler->ApplyDamage(DamageInstigator.Get(), TargetActor, DamageInfo, Hit);
		return;
	}

	const float RawDamage = DamageInfo.GetCalculatedRawDamage();
	FPointDamageEvent DamageEvent(RawDamage, Hit, Hit.ImpactNormal, nullptr);
	APawn* InstigatorPawn = Cast<APawn>(DamageInstigator.Get());
	TargetActor->TakeDamage(RawDamage, DamageEvent, InstigatorPawn ? InstigatorPawn->GetController() : nullptr, SourceWeapon.IsValid() ? static_cast<AActor*>(SourceWeapon.Get()) : this);
}
