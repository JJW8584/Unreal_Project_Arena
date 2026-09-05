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
#include "GameFramework/PlayerController.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Components/PrimitiveComponent.h"
#include "HAL/PlatformTime.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"

bool US1GameInstance::RequestRegister(const FString& LoginId, const FString& Password, const FString& Nickname)
{
	if (Socket == nullptr || !GameServerSession.IsValid())
		return false;

	const FString TrimmedLoginId = LoginId.TrimStartAndEnd();

	if (TrimmedLoginId.Len() < 4 || TrimmedLoginId.Len() > 64)
		return false;

	const FString TrimmedNickname = Nickname.TrimStartAndEnd();

	if (TrimmedNickname.Len() < 2 || TrimmedNickname.Len() > 16)
		return false;

	if (Password.Len() < 8 || Password.Len() > 128)
		return false;

	for (const TCHAR Ch : TrimmedLoginId)
	{
		const bool bIsAsciiLetter = (Ch >= TEXT('a') && Ch <= TEXT('z')) || (Ch >= TEXT('A') && Ch <= TEXT('Z'));
		const bool bIsDigit = Ch >= TEXT('0') && Ch <= TEXT('9');

		if (!bIsAsciiLetter && !bIsDigit)
			return false;
	}

	FTCHARToUTF8 LoginIdUtf8(*TrimmedLoginId);
	FTCHARToUTF8 PasswordUtf8(*Password);
	FTCHARToUTF8 NicknameUtf8(*TrimmedNickname);

	Protocol::C_REGISTER Pkt;
	Pkt.set_login_id(LoginIdUtf8.Get(), static_cast<size_t>(LoginIdUtf8.Length()));
	Pkt.set_password(PasswordUtf8.Get(), static_cast<size_t>(PasswordUtf8.Length()));
	Pkt.set_nickname(NicknameUtf8.Get(), static_cast<size_t>(NicknameUtf8.Length()));

	SEND_PACKET(Pkt);

	return true;
}

void US1GameInstance::HandleRegister(const Protocol::S_REGISTER& Pkt)
{
	const bool bRegisterSucceeded = Pkt.success() && Pkt.result() == Protocol::AUTH_RESULT_SUCCESS;

	FString ResultMessage;

	if (bRegisterSucceeded)
	{
		ResultMessage = TEXT("회원가입에 성공했습니다.");
	}
	else
	{
		switch (Pkt.result())
		{
		case Protocol::AUTH_RESULT_INVALID_INPUT:
			ResultMessage =
				TEXT("입력한 회원정보의 형식이 올바르지 않습니다.");
			break;

		case Protocol::AUTH_RESULT_DUPLICATE_LOGIN_ID:
			ResultMessage =
				TEXT("이미 사용 중인 아이디입니다.");
			break;

		case Protocol::AUTH_RESULT_DUPLICATE_NICKNAME:
			ResultMessage =
				TEXT("이미 사용 중인 닉네임입니다.");
			break;

		case Protocol::AUTH_RESULT_SERVER_ERROR:
			ResultMessage =
				TEXT("서버 오류가 발생했습니다. 잠시 후 다시 시도해주세요.");
			break;

		default:
			ResultMessage =
				TEXT("회원가입에 실패했습니다.");
			break;
		}
	}

	// 블루프린트 위젯에 결과 전달
	OnRegisterResult.Broadcast(bRegisterSucceeded, ResultMessage);

	if (GEngine != nullptr)
	{
		const FColor MessageColor = bRegisterSucceeded ? FColor::Green : FColor::Red;

		GEngine->AddOnScreenDebugMessage(-1, 5.f, MessageColor, ResultMessage);
	}
}

bool US1GameInstance::RequestLogin(const FString& LoginId, const FString& Password)
{
	if (Socket == nullptr || !GameServerSession.IsValid())
	{
		return false;
	}

	const FString TrimmedLoginId = LoginId.TrimStartAndEnd();

	if (TrimmedLoginId.Len() < 4 || TrimmedLoginId.Len() > 64)
	{
		OnLoginFailed.Broadcast(TEXT("아이디는 4자 이상 64자 이하로 입력해주세요."));
		return false;
	}

	if (Password.Len() < 8 || Password.Len() > 128)
	{
		OnLoginFailed.Broadcast(TEXT("비밀번호는 8자 이상 128자 이하로 입력해주세요."));
		return false;
	}

	for (const TCHAR Ch : TrimmedLoginId)
	{
		if (!FChar::IsAlnum(Ch))
		{
			OnLoginFailed.Broadcast(TEXT("아이디는 영문자와 숫자만 사용할 수 있습니다."));
			return false;
		}
	}

	FTCHARToUTF8 LoginIdUtf8(*TrimmedLoginId);
	FTCHARToUTF8 PasswordUtf8(*Password);

	Protocol::C_LOGIN Pkt;
	Pkt.set_login_id(LoginIdUtf8.Get(), static_cast<size_t>(LoginIdUtf8.Length()));
	Pkt.set_password(PasswordUtf8.Get(), static_cast<size_t>(PasswordUtf8.Length()));

	SEND_PACKET(Pkt);

	return true;
}

void US1GameInstance::HandleLogin(const Protocol::S_LOGIN& Pkt)
{
	const bool bLoginSucceeded = Pkt.success() && Pkt.result() == Protocol::AUTH_RESULT_SUCCESS;

	if (!bLoginSucceeded)
	{
		FString ErrorMessage;

		switch (Pkt.result())
		{
		case Protocol::AUTH_RESULT_INVALID_INPUT:
			ErrorMessage = TEXT("아이디 또는 비밀번호 형식이 올바르지 않습니다.");
			break;

		case Protocol::AUTH_RESULT_INVALID_CREDENTIALS:
			ErrorMessage = TEXT("아이디 또는 비밀번호가 올바르지 않습니다.");
			break;

		case Protocol::AUTH_RESULT_DUPLICATE_LOGIN_ID:
			ErrorMessage = TEXT("이미 접속 중인 계정입니다.");
			break;

		case Protocol::AUTH_RESULT_SERVER_ERROR:
			ErrorMessage = TEXT("서버 오류가 발생했습니다.");
			break;

		default:
			ErrorMessage = TEXT("로그인에 실패했습니다.");
			break;

		}

		OnLoginFailed.Broadcast(ErrorMessage);

		return;
	}

	// 성공 패킷 유효성 검사
	if (Pkt.account_id() == 0 || Pkt.object_id() == 0)
	{
		OnLoginFailed.Broadcast(TEXT("서버에서 잘못된 로그인 응답을 받았습니다."));

		if (GEngine != nullptr)
		{
			GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("잘못된 로그인 응답입니다."));
		}

		return;
	}

	LocalAccountId = Pkt.account_id();
	LocalObjectId = Pkt.object_id();
	LocalNickname = UTF8_TO_TCHAR(Pkt.nickname().c_str());

	if (GEngine != nullptr)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, FString::Printf(TEXT("%s 로그인 성공"), *LocalNickname));
	}

	UGameplayStatics::OpenLevel(this, FName(TEXT("Lobby")));
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
		Item.Nickname = UTF8_TO_TCHAR(Player.nickname().c_str());
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

void US1GameInstance::HandleLeaveRoom(Protocol::S_LEAVE_ROOM& Pkt)
{
	if (!Pkt.success())
		return;

	CurrentRoom = FCurrentRoomState();

	OnLeaveRoomResult.Broadcast(Pkt.success());
}

bool US1GameInstance::RequestChat(const FString& Message)
{
	if (Socket == nullptr || !GameServerSession.IsValid())
		return false;

	const FString TrimmedMessage = Message.TrimStartAndEnd();

	if (TrimmedMessage.IsEmpty())
		return false;

	Protocol::C_CHAT Pkt;
	Pkt.set_msg(TCHAR_TO_UTF8(*TrimmedMessage));

	SEND_PACKET(Pkt);
	return true;
}

void US1GameInstance::HandleChat(Protocol::S_CHAT& Pkt)
{
	const uint64 SenderId = Pkt.player_id();
	FString Nickname;

	// 인게임에서 자기 자신
	if (SenderId == LocalObjectId)
	{
		Nickname = LocalNickname;
	}

	// 인게임의 다른 플레이어
	if (Nickname.IsEmpty())
	{
		if (AS1Player* const* FoundPlayer = Players.Find(SenderId))
		{
			if (IsValid(*FoundPlayer))
			{
				Nickname = (*FoundPlayer)->GetNickname();
			}
		}
	}

	// Room에 있는 플레이어
	if (Nickname.IsEmpty())
	{
		for (const FRoomPlayerItem& Player : CurrentRoom.Players)
		{
			if (static_cast<uint64>(Player.ObjectId) == SenderId)
			{
				Nickname = Player.Nickname;
				break;
			}
		}
	}

	if (Nickname.IsEmpty())
	{
		Nickname = FString::Printf(TEXT("Player_%s"), *LexToString(SenderId));
	}

	const FString Message = UTF8_TO_TCHAR(Pkt.msg().c_str());
	OnChatReceived.Broadcast(FString::Printf(TEXT("%s: %s"), *Nickname, *Message));
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

	for (const Protocol::MatchPlayerInfo& PlayerInfo : MatchPlayerInfo)
	{
		HandleSpawn(PlayerInfo.player_info());
	}

	Protocol::C_MATCH_PREPARE Pkt;

	SEND_PACKET(Pkt);
}

void US1GameInstance::HandleMatchStart(Protocol::S_MATCH_START& Pkt)
{
	if (MatchId != Pkt.match_id())
		return;

	OnMatchStarted.Broadcast();
}

void US1GameInstance::HandleMatchState(Protocol::S_MATCH_STATE& Pkt)
{
	if (MatchId != Pkt.match_state().match_id())
		return;

	RemainSeconds = Pkt.match_state().remaining_time_seconds();
	RedScore = Pkt.match_state().red_score();
	BlueScore = Pkt.match_state().blue_score();

	if (RemainSeconds <= 0)
	{
		//TODO: 경기 종료
	}

	OnMatchStateUpdated.Broadcast();
}

bool US1GameInstance::RequestReturnRoom()
{
	if (Socket == nullptr || !GameServerSession.IsValid())
		return false;

	Protocol::C_RETURN_TO_ROOM Pkt;

	SEND_PACKET(Pkt);

	return true;
}

void US1GameInstance::HandleReturnRoom(Protocol::S_RETURN_TO_ROOM& Pkt)
{
	if (!Pkt.has_room_info())
	{
		return;
	}

	UpdateCurrentRoom(Pkt.room_info());

	Players.Reset();
	MyPlayer = nullptr;

	MatchPlayerInfo.Reset();
	MatchPlayerState.Reset();

	UGameplayStatics::OpenLevel(this, FName(TEXT("Lobby")));
}

void US1GameInstance::HandlePrepareMatch(const Protocol::S_MATCH_PREPARE& Pkt)
{
	MatchPlayerInfo.Reset();
	MatchPlayerState.Reset();

	MatchId = Pkt.match_info().match_id();
	RemainSeconds = Pkt.match_info().duration_seconds();
	RedScore = Pkt.match_info().red_score();
	BlueScore = Pkt.match_info().blue_score();

	if (Pkt.match_info().match_players_info().size() != Pkt.match_info().match_players_state().size())
	{
		//TODO: 잘못된 패킷
		return;
	}

	for (int32 i = 0; i < Pkt.match_info().match_players_info().size(); i++)
	{
		MatchPlayerInfo.Add(Pkt.match_info().match_players_info(i));
	}
	for (int32 i = 0; i < Pkt.match_info().match_players_state().size(); i++)
	{
		MatchPlayerState.Add({ Pkt.match_info().match_players_info(i).player_info().object_id(), Pkt.match_info().match_players_state(i) });
	}
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

		Player->SetNickname(LocalNickname);
		Player->SetMoveInfo(PlayerInfo.move_info());
		Player->SetDestInfo(PlayerInfo.move_info());

		if (const Protocol::MatchPlayerState* State = MatchPlayerState.Find(PlayerInfo.object_id()))
		{
			Player->UpdateMatchState(*State);
		}

		MyPlayer = Player;
		Players.Add(PlayerInfo.object_id(), Player);
	}
	else
	{
		AS1Player* Player = Cast<AS1Player>(GWorld->SpawnActor(OtherPlayerClass, &SpawnLocation));
		
		if (Player == nullptr)
			return;

		Player->SetNickname(UTF8_TO_TCHAR(PlayerInfo.nickname().c_str()));
		Player->SetMoveInfo(PlayerInfo.move_info());
		Player->SetDestInfo(PlayerInfo.move_info());

		if (const Protocol::MatchPlayerState* State = MatchPlayerState.Find(PlayerInfo.object_id()))
		{
			Player->UpdateMatchState(*State);
		}

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
	if (FindActor == nullptr || !IsValid(*FindActor))
		return;

	AS1Player* Player = *FindActor;
	Player->SetActorHiddenInGame(true);
	Player->SetActorEnableCollision(false);
	Player->SetActorTickEnabled(false);

	if (UCharacterMovementComponent* Movement = Player->GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
		Movement->DisableMovement();
	}

	if (Player->IsMyPlayer())
	{
		if (APlayerController* PC = Cast<APlayerController>(Player->GetController()))
		{
			Player->DisableInput(PC);
		}
	}
}

void US1GameInstance::HandleRespawn(const Protocol::PlayerInfo& PlayerInfo)
{
	if (Socket == nullptr || !GameServerSession.IsValid())
		return;

	AS1Player** FindActor = Players.Find(PlayerInfo.object_id());
	if (FindActor == nullptr || !IsValid(*FindActor))
		return;

	AS1Player* Player = *FindActor;
	Player->SetActorHiddenInGame(false);
	Player->SetActorEnableCollision(true);
	Player->SetActorTickEnabled(true);

	if (UCharacterMovementComponent* Movement = Player->GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
		Movement->SetMovementMode(MOVE_Walking);
	}

	Player->SetMoveInfo(PlayerInfo.move_info());
	Player->SetDestInfo(PlayerInfo.move_info());


	if (Player->IsMyPlayer())
	{
		if (APlayerController* PC = Cast<APlayerController>(Player->GetController()))
		{
			Player->EnableInput(PC);
		}
	}
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

void US1GameInstance::SendFireRequest(const FVector& SpawnLocation, const FVector& FireDirection)
{
	if (Socket == nullptr || !GameServerSession.IsValid())
		return;

	Protocol::C_FIRE Pkt;
	Pkt.set_object_id(LocalObjectId);

	Pkt.set_spawn_x(SpawnLocation.X);
	Pkt.set_spawn_y(SpawnLocation.Y);
	Pkt.set_spawn_z(50.f);

	Pkt.set_direction_x(FireDirection.X);
	Pkt.set_direction_y(FireDirection.Y);
	Pkt.set_direction_z(0.f);

	SEND_PACKET(Pkt);
}

void US1GameInstance::RequestHit(AS1Player* TargetPlayer)
{
	if (!IsValid(TargetPlayer) || !GameServerSession.IsValid())
		return;	

	for (const auto iter : Players)
	{
		if (iter.Value != TargetPlayer)
			continue;

		Protocol::C_HIT HitPkt;
		HitPkt.set_target_object_id(iter.Key);
		SEND_PACKET(HitPkt);
		return;
	}

}

void US1GameInstance::HandleFire(const Protocol::S_FIRE& FirePkt)
{
	auto World = GetWorld();

	if (World == nullptr || BulletClass == nullptr)
		return;

	AS1Player** FoundPlayer = Players.Find(FirePkt.object_id());

	if (FoundPlayer == nullptr || !IsValid(*FoundPlayer))
		return;

	AS1Player* Player = *FoundPlayer;

	FVector SpawnLocation(FirePkt.spawn_x(), FirePkt.spawn_y(), FirePkt.spawn_z());
	FVector FireDirection(FirePkt.direction_x(), FirePkt.direction_y(), FirePkt.direction_z());

	const FRotator SpawnRotation = FireDirection.Rotation();

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = Player;
	SpawnParams.Instigator = Cast<APawn>(Player);
	// 발사 위치가 플레이어 캡슐 내부일 수 있음
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AActor* Bullet = World->SpawnActor<AActor>(BulletClass, SpawnLocation, SpawnRotation, SpawnParams);

	if (!IsValid(Bullet))
		return;

	UProjectileMovementComponent* ProjectileMovement = Bullet->FindComponentByClass<UProjectileMovementComponent>();

	if (ProjectileMovement == nullptr)
	{
		Bullet->Destroy();
		return;
	}

	UPrimitiveComponent* CollisionComponent = Cast<UPrimitiveComponent>(ProjectileMovement->UpdatedComponent);

	if (CollisionComponent == nullptr)
	{
		Bullet->Destroy();
		return;
	}

	// Bullet이 이동할 때 발사자와의 충돌 무시
	CollisionComponent->IgnoreActorWhenMoving(Player, true);
}

void US1GameInstance::HandlePlayerState(const Protocol::S_PLAYER_STATE& PlayerStatePkt)
{
	const uint64 ObjectId = PlayerStatePkt.object_id();

	Protocol::MatchPlayerState* State = MatchPlayerState.Find(ObjectId);

	if (State == nullptr)
		return;

	State->CopyFrom(PlayerStatePkt.player_state());

	AS1Player** FoundPlayer = Players.Find(ObjectId);

	if (FoundPlayer == nullptr || !IsValid(*FoundPlayer))
		return;


	(*FoundPlayer)->UpdateMatchState(*State);
	OnMatchPlayerScoreUpdated.Broadcast(static_cast<int64>(ObjectId), static_cast<int32>(State->kill_count()), static_cast<int32>(State->death_count()));
}

void US1GameInstance::HandlePlayerState(const uint64 ObjectId, const Protocol::MatchPlayerState& PlayerState)
{
	Protocol::MatchPlayerState* State = MatchPlayerState.Find(ObjectId);

	if (State == nullptr)
		return;

	State->CopyFrom(PlayerState);

	AS1Player** FoundPlayer = Players.Find(ObjectId);

	if (FoundPlayer == nullptr || !IsValid(*FoundPlayer))
		return;


	(*FoundPlayer)->UpdateMatchState(*State);
	OnMatchPlayerScoreUpdated.Broadcast(static_cast<int64>(ObjectId), static_cast<int32>(State->kill_count()), static_cast<int32>(State->death_count()));
}

void US1GameInstance::HandleMatchEnd(const Protocol::S_MATCH_END& EndPkt)
{
	UWorld* World = GetWorld();
	if (World == nullptr)
		return;

	const Protocol::MatchResult& Result = EndPkt.result();

	RedScore = Result.red_score();
	BlueScore = Result.blue_score();
	RemainSeconds = 0;

	ERoomTeam WinnerTeam = ERoomTeam::None;

	switch (Result.winner_team())
	{
	case Protocol::TEAM_RED:
		WinnerTeam = ERoomTeam::Red;
		break;

	case Protocol::TEAM_BLUE:
		WinnerTeam = ERoomTeam::Blue;
		break;

	case Protocol::TEAM_NONE:
	default:
		WinnerTeam = ERoomTeam::None;
		break;
	}

	// 캐릭터, 투사체, 액터 Tick과 물리 등을 정지
	UGameplayStatics::SetGamePaused(World, true);

	// 기존 상단 점수 HUD도 마지막 점수로 갱신
	OnMatchStateUpdated.Broadcast();

	// 결과창 생성은 블루프린트에서 처리
	OnMatchEnded.Broadcast(WinnerTeam, static_cast<int32>(RedScore), static_cast<int32>(BlueScore));
}

bool US1GameInstance::GetMatchPlayerScore(int64 ObjectId, int32& KillCount, int32& DeathCount) const
{
	KillCount = 0;
	DeathCount = 0;

	const Protocol::MatchPlayerState* State = MatchPlayerState.Find(static_cast<uint64>(ObjectId));
	if (State == nullptr)
		return false;

	KillCount = static_cast<int32>(State->kill_count());
	DeathCount = static_cast<int32>(State->death_count());
	return true;
}
