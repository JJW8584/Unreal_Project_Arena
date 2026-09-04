// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InputActionValue.h"
#include "Protocol.pb.h"
#include "S1Player.generated.h"

USTRUCT(BlueprintType)
struct FMatchPlayerStateData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Match")
	int32 Hp = 100;

	UPROPERTY(BlueprintReadOnly, Category = "Match")
	int32 MaxHp = 100;

	UPROPERTY(BlueprintReadOnly, Category = "Match")
	bool bIsAlive = true;

	UPROPERTY(BlueprintReadOnly, Category = "Match")
	int32 KillCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Match")
	int32 DeathCount = 0;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPlayerStateUpdated, FMatchPlayerStateData, PlayerState);

UCLASS()
class S1_API AS1Player : public ACharacter
{
	GENERATED_BODY()

public:
	AS1Player();
	~AS1Player();

protected:
	virtual void BeginPlay();
	virtual void Tick(float DeltaSeconds) override;

public:
	bool IsMyPlayer() const;

	void UpdateMatchState(const Protocol::MatchPlayerState& State);

	// 스폰, 리스폰, 로컬 최종 보정
	void TeleportToServerPosition(float X, float Y);

	// 원격 플레이어 이동 패킷
	void SetServerMoveTarget(float X, float Y);

	UPROPERTY(BlueprintAssignable, Category = "Match")
	FOnPlayerStateUpdated OnPlayerStateUpdated;

public:

	void SetNickname(const FString& InNickname) { Nickname = InNickname; }
	UFUNCTION(BlueprintPure, Category = "Auth")
	FString GetNickname() const { return Nickname; }

protected:
	FString Nickname;
	FMatchPlayerStateData PlayerState;

	// 원격 캐릭터가 따라갈 서버 위치
	FVector2D ServerMoveTarget = FVector2D::ZeroVector;
};
