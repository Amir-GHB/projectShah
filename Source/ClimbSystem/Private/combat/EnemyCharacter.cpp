// Copyright Epic Games / Modular Combat System. All Rights Reserved.

#include "combat/EnemyCharacter.h"

#include "combat/CombatComponent.h"
#include "combat/DamageHandlerComponent.h"
#include "combat/DamageReactionComponent.h"
#include "AI/EnemyAIController.h"
#include "Engine/DamageEvents.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "combat/CombatProjectile.h"
#include "Kismet/GameplayStatics.h"

AEnemyCharacter::AEnemyCharacter()
{
	PrimaryActorTick.bCanEverTick = false;

	CombatComponent = CreateDefaultSubobject<UCombatComponent>(TEXT("CombatComponent"));
	DamageHandlerComponent = CreateDefaultSubobject<UDamageHandlerComponent>(TEXT("DamageHandlerComponent"));
	DamageReactionComponent = CreateDefaultSubobject<UDamageReactionComponent>(TEXT("DamageReactionComponent"));

	AIControllerClass = AEnemyAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;
	bUseControllerRotationYaw = false;

	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->bOrientRotationToMovement = true;
		Movement->RotationRate = FRotator(0.0f, 540.0f, 0.0f);
		Movement->MaxWalkSpeed = 300.0f;
		Movement->NavAgentProps.bCanCrouch = true;
	}
}

void AEnemyCharacter::BeginPlay()
{
	Super::BeginPlay();

	// Register real AI enemies with the player's encounter tracker so last-enemy
	// finishers work without Level Blueprint/manual dummy-only setup.
	if (ACharacter* PlayerCharacter = UGameplayStatics::GetPlayerCharacter(this, 0))
	{
		if (UCombatComponent* PlayerCombat = PlayerCharacter->FindComponentByClass<UCombatComponent>())
		{
			PlayerCombat->RegisterEncounterEnemy(this);
		}
	}
}

void AEnemyCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ACharacter* PlayerCharacter = UGameplayStatics::GetPlayerCharacter(this, 0))
	{
		if (UCombatComponent* PlayerCombat = PlayerCharacter->FindComponentByClass<UCombatComponent>())
		{
			PlayerCombat->UnregisterEncounterEnemy(this);
		}
	}
	Super::EndPlay(EndPlayReason);
}

void AEnemyCharacter::PerformAICombatAction(AActor* Target)
{
	if (CombatComponent) CombatComponent->HandleAttackInput();
}

void AMagicEnemyCharacter::PerformAICombatAction(AActor* Target)
{
	if (!IsValid(Target) || !IsValid(MagicProjectileClass) || !GetWorld()) return;
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now < NextMagicAttackTime || PendingMagicTarget.IsValid()) return;
	PendingMagicTarget = Target;
	bNeedsRelocationAfterAttack = false;

	UAnimInstance* Anim = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	if (IsValid(MagicAttackMontage) && Anim)
	{
		if (Anim->Montage_Play(MagicAttackMontage) > 0.0f) return;
	}
	// Fallback for prototypes without an authored attack montage.
	ReleaseMagicProjectile();
}

void AMagicEnemyCharacter::ReleaseMagicProjectile()
{
	AActor* Target = PendingMagicTarget.Get();
	PendingMagicTarget.Reset();
	if (!IsValid(Target) || !IsValid(MagicProjectileClass) || !GetWorld()) return;

	const FVector SpawnLocation = GetMesh() && GetMesh()->DoesSocketExist(ProjectileSpawnSocket)
		? GetMesh()->GetSocketLocation(ProjectileSpawnSocket)
		: GetActorLocation() + GetActorTransform().TransformVectorNoScale(ProjectileSpawnOffset);
	const FVector AimLocation = Target->GetActorLocation() + FVector(0.0f, 0.0f, 55.0f);
	const FRotator AimRotation = (AimLocation - SpawnLocation).Rotation();
	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.Instigator = this;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	ACombatProjectile* Projectile = GetWorld()->SpawnActor<ACombatProjectile>(MagicProjectileClass, SpawnLocation, AimRotation, Params);
	if (Projectile)
	{
		FProjectileConfig Config = MagicProjectileConfig;
		Config.GravityScale = 0.0f;
		Projectile->InitializeProjectile(Config, (AimLocation - SpawnLocation).GetSafeNormal(), this, nullptr, Target);
	}
	NextMagicAttackTime = GetWorld()->GetTimeSeconds() + FMath::Max(0.1f, MagicAttackCooldown);
	bNeedsRelocationAfterAttack = true;
}

float AEnemyCharacter::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent,
	AController* EventInstigator, AActor* DamageCauser)
{
	const float ActualDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);

	if (!DamageHandlerComponent || DamageHandlerComponent->bIsDead || ActualDamage <= 0.0f)
	{
		return ActualDamage;
	}

	FDamageInfo DamageInfo;
	DamageInfo.BaseDamage = ActualDamage;
	DamageInfo.DamageMultiplier = 1.0f;
	DamageInfo.DamageCauser = DamageCauser;
	if (IsValid(DamageCauser))
	{
		DamageInfo.DamageDirection =
			(GetActorLocation() - DamageCauser->GetActorLocation()).GetSafeNormal();
	}
	DamageInfo.Normalize();

	FHitResult HitResult;
	if (DamageEvent.IsOfType(FPointDamageEvent::ClassID))
	{
		HitResult = static_cast<const FPointDamageEvent&>(DamageEvent).HitInfo;
	}
	DamageHandlerComponent->ApplyDamage(DamageCauser, this, DamageInfo, HitResult);
	return ActualDamage;
}
