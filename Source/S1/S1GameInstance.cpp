// Fill out your copyright notice in the Description page of Project Settings.



#include "S1GameInstance.h"
#include "S1.h"
#include "Sockets.h"
#include "Common/TcpSocketBuilder.h"
#include "Serialization/ArrayWriter.h"
#include "SocketSubsystem.h"
#include "PacketSession.h"
#include "Protocol.pb.h"
#include "ServerPacketHandler.h"
#include "S1MyPlayer.h"

bool US1GameInstance::RequestLogin()
{
	if (Socket == nullptr || !GameServerSession.IsValid())
		return false;

	//TEMP : Lobby?êÏÑú Ï∫êÎ¶≠???†ÌÉùÏ∞?
	Protocol::C_LOGIN Pkt;
	SEND_PACKET(Pkt);

	return true;
}

bool US1GameInstance::RequestRefresh()
{
	if (Socket == nullptr || !GameServerSession.IsValid())
		return false;

	Protocol::C_ROOM_LIST Pkt;
	SEND_PACKET(Pkt);

	return true;
}

bool US1GameInstance::RequestCreateRoom(const FString& RoomName, int32 MaxPlayerCount)
{
	if (Socket == nullptr || !GameServerSession.IsValid())
		return false;

	const FString TrimmedName =
		RoomName.TrimStartAndEnd();

	if (TrimmedName.IsEmpty() || MaxPlayerCount <= 0)
		return false;

	Protocol::C_CREATE_ROOM Pkt;
	Pkt.set_room_name(TCHAR_TO_UTF8(*TrimmedName));
	Pkt.set_max_player_count(static_cast<uint32>(MaxPlayerCount));
	SEND_PACKET(Pkt);

	return true;
}

bool US1GameInstance::RequestEnterRoom(int64 RoomId)
{
	if (Socket == nullptr || !GameServerSession.IsValid())
		return false;

	if (RoomId <= 0)
		return false;

	Protocol::C_ENTER_ROOM Pkt;
	Pkt.set_room_id(static_cast<uint64>(RoomId));
	SEND_PACKET(Pkt);

	return true;
}

void US1GameInstance::HandleEnterRoom(const Protocol::S_ENTER_ROOM& Pkt)
{
	if (!Pkt.success() || !Pkt.has_room_info())
	{
		OnEnterRoomResult.Broadcast(false);
		return;
	}

	UpdateCurrentRoom(Pkt.room_info());

	OnEnterRoomResult.Broadcast(true);
}

void US1GameInstance::UpdateCurrentRoom(const Protocol::RoomInfo& Info)
{
	CurrentRoom.RoomId = static_cast<int64>(Info.room_id());
	CurrentRoom.RoomName = UTF8_TO_TCHAR(Info.room_name().c_str());
	CurrentRoom.CurrentPlayerCount = Info.players_size();
	CurrentRoom.MaxPlayerCount = static_cast<int32>(Info.max_player_count());
	CurrentRoom.HostObjectId = static_cast<int64>(Info.host_object_id());

	CurrentRoom.Players.Reset();

	for (const Protocol::RoomPlayerInfo& Player : Info.players())
	{
		FRoomPlayerItem Item;
		Item.ObjectId = static_cast<int64>(Player.object_id());
		Item.Ready = Player.ready();
		
		switch (Player.team())
		{
		case Protocol::TEAM_BLUE:
			Item.Team = ERoomTeam::Blue;
			break;

		case Protocol::TEAM_RED:
			Item.Team = ERoomTeam::Red;
			break;

		default:
			Item.Team = ERoomTeam::None;
			break;
		}
		CurrentRoom.Players.Add(Item);
	}
}

void US1GameInstance::HandleRoomState(const Protocol::S_ROOM_STATE& Pkt)
{
	if (!Pkt.has_room_info())
		return;

	UpdateCurrentRoom(Pkt.room_info());

	OnRoomStateUpdated.Broadcast();
}

bool US1GameInstance::RequestChangeTeam(ERoomTeam Team)
{
	if (Socket == nullptr || !GameServerSession.IsValid())
		return false;

	Protocol::C_CHANGE_TEAM RoomChangePkt;

	switch (Team)
	{
	case ERoomTeam::None:
		RoomChangePkt.set_team(Protocol::TEAM_NONE);
		break;
	case ERoomTeam::Red:
		RoomChangePkt.set_team(Protocol::TEAM_RED);
		break;
	case ERoomTeam::Blue:
		RoomChangePkt.set_team(Protocol::TEAM_BLUE);
		break;
	default:
		break;
	}

	SEND_PACKET(RoomChangePkt);

	return true;
}

bool US1GameInstance::RequestReady(bool ready)
{
	if (Socket == nullptr || !GameServerSession.IsValid())
		return false;

	Protocol::C_READY ReadyPkt;

	ReadyPkt.set_ready(ready);

	SEND_PACKET(ReadyPkt);

	return true;
}

bool US1GameInstance::ConnectToGameServer()
{
	if (Socket != nullptr)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, FString::Printf(TEXT("Already connected to server")));
		return true;
	}

	Socket = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateSocket(TEXT("Stream"), TEXT("Client Socket"));

	FIPv4Address Ip;
	FIPv4Address::Parse(IpAddress, Ip);

	TSharedRef<FInternetAddr> InternetAddr = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr();
	InternetAddr->SetIp(Ip.Value);
	InternetAddr->SetPort(Port);

	GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, FString::Printf(TEXT("Connecting To Server...")));

	bool Connected = Socket->Connect(*InternetAddr);

	if (Connected)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, FString::Printf(TEXT("Connection Success")));

		//Session
		GameServerSession = MakeShared<PacketSession>(Socket);
		GameServerSession->Run();

		return true;
	}
	else
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, FString::Printf(TEXT("Connection Failed")));

		ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(Socket);
		Socket = nullptr;

		return false;
	}
}

void US1GameInstance::DisconnectFromGameServer()
{
	if (Socket == nullptr || GameServerSession == nullptr)
		return;

	Protocol::C_LEAVE_ROOM LeavePkt;
	SEND_PACKET(LeavePkt);

	if (Socket)
	{
		ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get();
		SocketSubsystem->DestroySocket(Socket);
		Socket = nullptr;
	}
}

void US1GameInstance::HandleRecvPackets()
{
	if (Socket == nullptr || GameServerSession == nullptr)
		return;

	GameServerSession->HandleRecvPackets();
}

void US1GameInstance::SendPacket(SendBufferRef SendBuffer)
{
	if (Socket == nullptr || GameServerSession == nullptr)
		return;

	GameServerSession->SendPacket(SendBuffer);
}

void US1GameInstance::HandleRoomList(const Protocol::S_ROOM_LIST& Pkt)
{
	LobbyRooms.Reset();

	for (const Protocol::RoomInfo& Info : Pkt.rooms())
	{
		FRoomListItem Item;
		Item.RoomId = static_cast<int64>(Info.room_id());
		Item.RoomName =
			UTF8_TO_TCHAR(Info.room_name().c_str());
		Item.CurrentPlayerCount = Info.players_size();
		Item.MaxPlayerCount =
			static_cast<int32>(Info.max_player_count());
		Item.HostObjectId =
			static_cast<int64>(Info.host_object_id());

		LobbyRooms.Add(Item);
	}

	OnRoomListUpdated.Broadcast();
}

void US1GameInstance::HandleCreateRoom(const Protocol::S_CREATE_ROOM& Pkt)
{
	if (!Pkt.success() || !Pkt.has_room_info())
	{
		OnCreateRoomResult.Broadcast(false);
		return;
	}

	UpdateCurrentRoom(Pkt.room_info());

	OnCreateRoomResult.Broadcast(true);
}

bool US1GameInstance::RequestLeaveRoom()
{
	if (Socket == nullptr || !GameServerSession.IsValid())
		return false;

	Protocol::C_LEAVE_ROOM Pkt;

	SEND_PACKET(Pkt);

	return true;
}

void US1GameInstance::HandleLeaveRoom(Protocol::S_LEAVE_ROOM& pkt)
{
	if (!pkt.success())
		return;

	CurrentRoom = FCurrentRoomState();

	OnLeaveRoomResult.Broadcast(pkt.success());
}

bool US1GameInstance::RequestGameStart()
{
	if (Socket == nullptr || !GameServerSession.IsValid())
		return false;

	Protocol::C_START_MATCH Pkt;

	SEND_PACKET(Pkt);

	return true;
}

void US1GameInstance::PrepareMatch()
{
	Players.Reset();
	MyPlayer = nullptr;

	for (const Protocol::MatchPlayerInfo& MatchPlayerInfo : MatchInfo.match_players_info())
	{
		HandleSpawn(MatchPlayerInfo.player_info());
	}
}

void US1GameInstance::HandlePrepareMatch(const Protocol::S_MATCH_PREPARE& Pkt)
{
	MatchId = Pkt.match_info().match_id();
	DurationSeconds = Pkt.match_info().duration_seconds();
	MatchInfo = Pkt.match_info();
}

void US1GameInstance::HandleSpawn(const Protocol::PlayerInfo& PlayerInfo)
{
	if (Socket == nullptr || !GameServerSession.IsValid())
		return;

	bool IsMine = LocalObjectId == PlayerInfo.object_id();

	FVector SpawnLocation(PlayerInfo.move_info().x(), PlayerInfo.move_info().y(), PlayerInfo.move_info().z());

	if (IsMine)
	{
		auto* PC = UGameplayStatics::GetPlayerController(this, 0);
		AS1Player* Player = Cast<AS1Player>(PC->GetPawn());
		if (Player == nullptr)
			return;

		Player->SetMoveInfo(PlayerInfo.move_info());
		Player->SetDestInfo(PlayerInfo.move_info());

		MyPlayer = Player;
		Players.Add(PlayerInfo.object_id(), Player);
	}
	else
	{
		AS1Player* Player = Cast<AS1Player>(GWorld->SpawnActor(OtherPlayerClass, &SpawnLocation));
		
		if (Player == nullptr)
			return;

		Player->SetMoveInfo(PlayerInfo.move_info());
		Player->SetDestInfo(PlayerInfo.move_info());
		Players.Add(PlayerInfo.object_id(), Player);
	}
}

void US1GameInstance::HandleDespawn(uint64 ObjectId)
{
	if (Socket == nullptr || GameServerSession == nullptr)
		return;

	auto* World = GetWorld();
	if (World == nullptr)
		return;

	AS1Player** FindActor = Players.Find(ObjectId);
	if (FindActor == nullptr)
		return;

	World->DestroyActor(*FindActor);
}

void US1GameInstance::HandleMove(const Protocol::S_MOVE& MovePkt)
{
	if (Socket == nullptr || GameServerSession == nullptr)
		return;

	auto* World = GetWorld();
	if (World == nullptr)
		return;

	const uint64 ObjectId = MovePkt.player_info().object_id();
	AS1Player** FindActor = Players.Find(ObjectId);
	if (FindActor == nullptr)
		return;

	AS1Player* Player = (*FindActor);
	if (Player->IsMyPlayer())
		return;

	const Protocol::MoveInfo& Info = MovePkt.player_info().move_info();
	//Player->SetPlayerInfo(Info);
	Player->SetDestInfo(Info);
}

void US1GameInstance::Fire()
{
	if (Socket == nullptr || !GameServerSession.IsValid())
		return;

	Protocol::C_FIRE Pkt;
	Pkt.set_client_fire_id(LocalObjectId);

	SEND_PACKET(Pkt);
}
