// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "S1.h"
#include "Protocol.pb.h"
#include "S1GameInstance.generated.h"

class AS1Player;

UENUM(BlueprintType)
enum class ERoomTeam : uint8
{
	None,
	Red,
	Blue
};

USTRUCT(BlueprintType)
struct FRoomPlayerItem
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	int64 ObjectId = 0;

	UPROPERTY(BlueprintReadOnly)
	ERoomTeam Team = ERoomTeam::None;

	UPROPERTY(BlueprintReadOnly)
	bool Ready = false;
};

USTRUCT(BlueprintType)
struct FRoomListItem
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Lobby")
	int64 RoomId = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Lobby")
	FString RoomName;

	UPROPERTY(BlueprintReadOnly, Category = "Lobby")
	int32 CurrentPlayerCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Lobby")
	int32 MaxPlayerCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Lobby")
	int64 HostObjectId = 0;
};

USTRUCT(BlueprintType)
struct FCurrentRoomState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Lobby")
	int64 RoomId = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Lobby")
	FString RoomName;

	UPROPERTY(BlueprintReadOnly, Category = "Lobby")
	int32 CurrentPlayerCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Lobby")
	int32 MaxPlayerCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Lobby")
	int64 HostObjectId = 0;

	UPROPERTY(BlueprintReadOnly)
	TArray<FRoomPlayerItem> Players;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnRoomListUpdated);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCreateRoomResult, bool, bSuccess);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEnterRoomResult, bool, bSuccess);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnRoomStateUpdated);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLeaveRoomResult, bool, bSuccess);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnMatchStateUpdated);

UCLASS()
class S1_API US1GameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable)
	bool ConnectToGameServer();

	UFUNCTION(BlueprintCallable)
	void DisconnectFromGameServer();

	UFUNCTION(BlueprintCallable)
	void HandleRecvPackets();

	void SendPacket(SendBufferRef SendBuffer);

	uint64 GetLocalObjectId() const
	{
		return LocalObjectId;
	}

	void SetLocalObjectId(uint64 InObjectId)
	{
		LocalObjectId = InObjectId;
	}

public:
	// Login
	UFUNCTION(BlueprintCallable)
	bool RequestLogin();


	//---------
	//	Lobby
	//---------
	
	// 새로고침
	UFUNCTION(BlueprintCallable)
	bool RequestRefresh();

	void HandleRoomList(const Protocol::S_ROOM_LIST& Pkt);

	UPROPERTY(BlueprintReadOnly, Category = "Lobby")
	TArray<FRoomListItem> LobbyRooms;

	UPROPERTY(BlueprintAssignable, Category = "Lobby")
	FOnRoomListUpdated OnRoomListUpdated;

	// 방 생성
	UFUNCTION(BlueprintCallable)
	bool RequestCreateRoom(const FString& RoomName, int32 MaxPlayerCount);
	
	void HandleCreateRoom(const Protocol::S_CREATE_ROOM& Pkt);

	UPROPERTY(BlueprintReadOnly, Category = "Lobby")
	FCurrentRoomState CurrentRoom;

	UPROPERTY(BlueprintAssignable, Category = "Lobby")
	FOnCreateRoomResult OnCreateRoomResult;

	// 방 입장
	UFUNCTION(BlueprintCallable)
	bool RequestEnterRoom(int64 RoomId);

	void HandleEnterRoom(const Protocol::S_ENTER_ROOM& Pkt);

	UPROPERTY(BlueprintAssignable, Category = "Lobby")
	FOnEnterRoomResult OnEnterRoomResult;

	//--------
	//	Room
	//--------
	void UpdateCurrentRoom(const Protocol::RoomInfo& Info);

	void HandleRoomState(const Protocol::S_ROOM_STATE& Pkt);

	UFUNCTION(BlueprintCallable)
	bool RequestChangeTeam(ERoomTeam Team);

	UFUNCTION(BlueprintCallable)
	bool RequestReady(bool ready);

	UPROPERTY(BlueprintAssignable, Category = "Room")
	FOnRoomStateUpdated OnRoomStateUpdated;

	UPROPERTY(BlueprintReadOnly, Category = "Room")
	bool bIsRoomHost = false;

	// 방 나가기
	UFUNCTION(BlueprintCallable)
	bool RequestLeaveRoom();

	void HandleLeaveRoom(Protocol::S_LEAVE_ROOM& Pkt);

	UPROPERTY(BlueprintAssignable, Category = "Room")
	FOnLeaveRoomResult OnLeaveRoomResult;

	//----------
	//	인게임
	//----------

	// 게임 시작
	UFUNCTION(BlueprintCallable)
	bool RequestGameStart();

	UFUNCTION(BlueprintCallable)
	void PrepareMatch();

	void HandleMatchStart(Protocol::S_MATCH_START& Pkt);
	void HandleMatchState(Protocol::S_MATCH_STATE& Pkt);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bullet")
	TSubclassOf<AActor> BulletClass;

public:
	void HandlePrepareMatch(const Protocol::S_MATCH_PREPARE& Pkt);
	void HandleSpawn(const Protocol::PlayerInfo& PlayerInfo);

	void HandleDespawn(uint64 ObjectId);

	void HandleMove(const Protocol::S_MOVE& MovePkt);
	void HandleFire(const Protocol::S_FIRE& FirePkt);
	void HandlePlayerState(const Protocol::S_PLAYER_STATE& PlayerStatePkt);

	UFUNCTION(BlueprintCallable, Category = "Bullet")
	void SendFireRequest(const FVector& SpawnLocation, const FVector& FireDirection);

	UFUNCTION(BlueprintCallable, Category = "Bullet")
	void RequestHit(AS1Player* TargetPlayer);

	UFUNCTION(BlueprintPure, Category = "Match")
	int32 GetRemainSeconds() const
	{
		return static_cast<int32>(RemainSeconds);
	}

	UFUNCTION(BlueprintPure, Category = "Match")
	int32 GetRedScore() const
	{
		return static_cast<int32>(RedScore);
	}

	UFUNCTION(BlueprintPure, Category = "Match")
	int32 GetBlueScore() const
	{
		return static_cast<int32>(BlueScore);
	}

	UFUNCTION(BlueprintPure, Category = "Match")
	bool GetMatchPlayerScore(int64 ObjectId, int32& KillCount, int32& DeathCount) const;

	UPROPERTY(BlueprintAssignable, Category = "Match")
	FOnMatchStateUpdated OnMatchStateUpdated;

public:
	// GameServer
	class FSocket* Socket = nullptr;
	FString IpAddress = TEXT("127.0.0.1");
	int16 Port = 7777;
	TSharedPtr<class PacketSession> GameServerSession;

public:
	// 인게임
	uint64 MatchId;
	uint32 RemainSeconds;
	uint32 RedScore;
	uint32 BlueScore;
	TArray<Protocol::MatchPlayerInfo> MatchPlayerInfo;
	TMap<uint64, Protocol::MatchPlayerState> MatchPlayerState;

public:
	UPROPERTY(EditAnywhere)
	TSubclassOf<AS1Player> OtherPlayerClass;

	AS1Player* MyPlayer;
	TMap<uint64, AS1Player*> Players;

private:
	uint64 LocalObjectId = 0;
};
