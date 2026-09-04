// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/S1Player.h"
#include "Engine/LocalPlayer.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/Controller.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "S1MyPlayer.h"

AS1Player::AS1Player()
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);

	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true; // Character moves in the direction of input...	
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f); // ...at this rotation rate

	// Note: For faster iteration times these variables, and many more, can be tweaked in the Character Blueprint
	// instead of recompiling to adjust them
	GetCharacterMovement()->JumpZVelocity = 700.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;

	GetCharacterMovement()->bRunPhysicsWithNoController = true;
}

AS1Player::~AS1Player()
{
}

void AS1Player::BeginPlay()
{
	Super::BeginPlay();

	
	FVector Location = GetActorLocation();

	ServerMoveTarget = FVector2D(Location.X, Location.Y);

	if (!IsMyPlayer())
	{
		UCharacterMovementComponent* Movement = GetCharacterMovement();

		Movement->StopMovementImmediately();
		Movement->DisableMovement();
	}	
}

void AS1Player::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (IsMyPlayer())
		return;

	const FVector PreviousLocation = GetActorLocation();

	const FVector TargetLocation(ServerMoveTarget.X, ServerMoveTarget.Y, PreviousLocation.Z);

	const FVector DesiredLocation = FMath::VInterpConstantTo(PreviousLocation, TargetLocation, DeltaSeconds, 500.f);

	FHitResult Hit;

	SetActorLocation(DesiredLocation, true, &Hit, ETeleportType::None);

	// Sweep 적용 후 실제 이동한 위치
	const FVector ActualLocation = GetActorLocation();

	UCharacterMovementComponent* Movement = GetCharacterMovement();

	if (DeltaSeconds > UE_SMALL_NUMBER)
	{
		Movement->Velocity = (ActualLocation - PreviousLocation) / DeltaSeconds;
	}
	else
	{
		Movement->Velocity = FVector::ZeroVector;
	}

	//const float Distance = FVector::Dist(Location, DestLocation);

	//// 오차가 큰 경우 강제 이동
	//if (Distance >= 100.f)
	//{
	//	SetActorLocation(DestLocation);
	//}

	//const Protocol::MoveState State = PlayerInfo->state();
	//if (State == Protocol::MOVE_STATE_MOVE)
	//{
	//	SetActorRotation(FMath::RInterpTo(GetActorRotation(), FRotator(0, DestInfo->yaw(), 0), DeltaSeconds, 15.f));
	//	AddMovementInput(GetActorForwardVector());
	//}
}

bool AS1Player::IsMyPlayer() const
{
	return Cast<AS1MyPlayer>(this) != nullptr;
}	

void AS1Player::UpdateMatchState(const Protocol::MatchPlayerState& State)
{
	PlayerState.Hp = State.hp();
	PlayerState.MaxHp = State.max_hp();
	PlayerState.bIsAlive = State.is_alive();
	PlayerState.KillCount = State.kill_count();
	PlayerState.DeathCount = State.death_count();

	OnPlayerStateUpdated.Broadcast(PlayerState);
}

void AS1Player::TeleportToServerPosition(float X, float Y)
{
	FVector Location = GetActorLocation();

	Location.X = X;
	Location.Y = Y;

	FHitResult Hit;

	SetActorLocation(Location, true, &Hit, ETeleportType::None);

	const FVector ActualLocation = GetActorLocation();

	ServerMoveTarget = FVector2D(ActualLocation.X, ActualLocation.Y);
}

void AS1Player::SetServerMoveTarget(float X, float Y)
{
	const FVector2D NewTarget(X, Y);

	const FVector2D MoveDirection = NewTarget - ServerMoveTarget;

	if (!MoveDirection.IsNearlyZero())
	{
		const FVector Direction3D(MoveDirection.X, MoveDirection.Y, 0.f);

		SetActorRotation(Direction3D.Rotation());
	}

	ServerMoveTarget = NewTarget;
}