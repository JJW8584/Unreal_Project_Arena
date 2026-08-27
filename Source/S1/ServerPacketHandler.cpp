#include "ServerPacketHandler.h"
#include "BufferReader.h"
#include "S1.h"
#include "S1GameInstance.h"

PacketHandlerFunc GPacketHandler[UINT16_MAX];

// ÄÁÅÙÃ÷ ÀÛ¾÷
bool Handle_INVALID(PacketSessionRef& session, BYTE* buffer, int32 len)
{
	return false;
}

bool Handle_S_LOGIN(PacketSessionRef& session, Protocol::S_LOGIN& pkt)
{
	if (pkt.success())
	{
		if (GWorld == nullptr)
			return false;

		US1GameInstance* GameInstance = Cast<US1GameInstance>(GWorld->GetGameInstance());
		if (GameInstance == nullptr)
			return false;

		if (pkt.object_id() != 0)
			GameInstance->SetLocalObjectId(pkt.object_id());

		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, FString::Printf(TEXT("Login Success")));
		
		UGameplayStatics::OpenLevel(GWorld, FName(TEXT("Lobby")));

		return true;
	}
	else
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, FString::Printf(TEXT("Login Fail")));

		return false;
	}
}

bool Handle_S_ROOM_LIST(PacketSessionRef& session, Protocol::S_ROOM_LIST& pkt)
{
	if (GWorld == nullptr)
		return false;

	US1GameInstance* GameInstance = Cast<US1GameInstance>(GWorld->GetGameInstance());

	if (GameInstance == nullptr)
		return false;

	GameInstance->HandleRoomList(pkt);

	return true;
}

bool Handle_S_CREATE_ROOM(PacketSessionRef& session, Protocol::S_CREATE_ROOM& pkt)
{
	if (GWorld == nullptr)
		return false;

	US1GameInstance* GameInstance = Cast<US1GameInstance>(GWorld->GetGameInstance());

	if (GameInstance == nullptr)
		return false;

	if (pkt.success() && pkt.has_room_info())
	{
		GameInstance->bIsRoomHost = GameInstance->GetLocalObjectId() != 0 &&
			GameInstance->GetLocalObjectId() == pkt.room_info().host_object_id();
	}

	GameInstance->HandleCreateRoom(pkt);

	return true;
}

bool Handle_S_ENTER_ROOM(PacketSessionRef& session, Protocol::S_ENTER_ROOM& pkt)
{
	if (GWorld == nullptr)
		return false;

	US1GameInstance* GameInstance = Cast<US1GameInstance>(GWorld->GetGameInstance());

	if (GameInstance == nullptr)
		return false;

	if (pkt.success() && pkt.has_room_info())
	{
		GameInstance->bIsRoomHost = GameInstance->GetLocalObjectId() != 0 &&
			GameInstance->GetLocalObjectId() == pkt.room_info().host_object_id();
	}

	GameInstance->HandleEnterRoom(pkt);
	return true;
}

bool Handle_S_LEAVE_ROOM(PacketSessionRef& session, Protocol::S_LEAVE_ROOM& pkt)
{
	if (GWorld == nullptr)
		return false;

	US1GameInstance* GameInstance = Cast<US1GameInstance>(GWorld->GetGameInstance());

	if (GameInstance == nullptr)
		return false;

	if (pkt.success())
		GameInstance->bIsRoomHost = false;

	GameInstance->HandleLeaveRoom(pkt);

	return true;
}

bool Handle_S_ROOM_STATE(PacketSessionRef& session, Protocol::S_ROOM_STATE& pkt)
{
	if (GWorld == nullptr)
		return false;

	US1GameInstance* GameInstance = Cast<US1GameInstance>(GWorld->GetGameInstance());

	if (GameInstance == nullptr)
		return false;

	if (pkt.has_room_info())
	{
		GameInstance->bIsRoomHost = GameInstance->GetLocalObjectId() != 0 &&
			GameInstance->GetLocalObjectId() == pkt.room_info().host_object_id();
	}

	GameInstance->HandleRoomState(pkt);

	return true;
}

bool Handle_S_MATCH_PREPARE(PacketSessionRef& session, Protocol::S_MATCH_PREPARE& pkt)
{

	if (GWorld == nullptr)
		return false;

	US1GameInstance* GameInstance = Cast<US1GameInstance>(GWorld->GetGameInstance());

	if (GameInstance == nullptr)
		return false;
	
	GameInstance->HandlePrepareMatch(pkt);

	UGameplayStatics::OpenLevel(GWorld, FName(TEXT("GameMap")));
	return true;
}

bool Handle_S_MATCH_START(PacketSessionRef& session, Protocol::S_MATCH_START& pkt)
{
	return true;
}

bool Handle_S_MATCH_STATE(PacketSessionRef& session, Protocol::S_MATCH_STATE& pkt)
{
	return true;
}

bool Handle_S_PLAYER_STATE(PacketSessionRef& session, Protocol::S_PLAYER_STATE& pkt)
{
	return true;
}

bool Handle_S_PLAYER_RESPAWN(PacketSessionRef& session, Protocol::S_PLAYER_RESPAWN& pkt)
{
	return true;
}

bool Handle_S_MOVE(PacketSessionRef& session, Protocol::S_MOVE& pkt)
{
	if (auto* GameInstance = Cast<US1GameInstance>(GWorld->GetGameInstance()))
	{
		GameInstance->HandleMove(pkt);
	}

	return true;
}

bool Handle_S_FIRE(PacketSessionRef& session, Protocol::S_FIRE& pkt)
{
	if (auto* GameInstance = Cast<US1GameInstance>(GWorld->GetGameInstance()))
	{
		GameInstance->HandleFire(pkt);
	}

	return true;
}

bool Handle_S_PLAYER_DESPAWN(PacketSessionRef& session, Protocol::S_PLAYER_DESPAWN& pkt)
{
	return true;
}

bool Handle_S_MATCH_END(PacketSessionRef& session, Protocol::S_MATCH_END& pkt)
{
	return true;
}

bool Handle_S_RETURN_TO_ROOM(PacketSessionRef& session, Protocol::S_RETURN_TO_ROOM& pkt)
{
	return true;
}

bool Handle_S_CHAT(PacketSessionRef& session, Protocol::S_CHAT& pkt)
{
	return true;
}
