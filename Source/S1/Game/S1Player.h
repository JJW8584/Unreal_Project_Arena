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
	bool IsMyPlayer();

	Protocol::MoveState GetMoveState() { return PlayerInfo->state(); }
	void SetMoveState(Protocol::MoveState State);

	void UpdateMatchState(const Protocol::MatchPlayerState& State);

	UPROPERTY(BlueprintAssignable, Category = "Match")
	FOnPlayerStateUpdated OnPlayerStateUpdated;

public:
	void SetMoveInfo(const Protocol::MoveInfo& Info);
	void SetDestInfo(const Protocol::MoveInfo& Info);
	Protocol::MoveInfo* GetPlayerInfo() { return PlayerInfo; }

	void SetNickname(const FString& InNickname) { Nickname = InNickname; }
	UFUNCTION(BlueprintPure, Category = "Auth")
	FString GetNickname() const { return Nickname; }

protected:
	FString Nickname;

	FMatchPlayerStateData PlayerState;
	class Protocol::MoveInfo* PlayerInfo; // 현재 위치
	class Protocol::MoveInfo* DestInfo; // 목적지
};
