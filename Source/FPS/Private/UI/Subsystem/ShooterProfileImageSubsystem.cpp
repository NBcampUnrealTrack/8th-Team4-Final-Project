// ShooterProfileImageSubsystem.cpp
#include "UI/Subsystem/ShooterProfileImageSubsystem.h"

#include "Engine/Texture2D.h"
#include "GameFramework/OnlineReplStructs.h"
#include "OnlineSubsystemNames.h"

THIRD_PARTY_INCLUDES_START
#include "steam/steam_api.h"
THIRD_PARTY_INCLUDES_END

namespace ShooterProfileImageParsing
{
	// 숫자만 받음. 부호/공백 등 받지않음
	bool IsDecimalDigits(const FString& Text)
	{
		if (Text.IsEmpty() || Text.Len() > 20)
		{
			return false;
		}

		for (const TCHAR Char : Text)
		{
			if (!FChar::IsDigit(Char))
			{
				return false;
			}
		}
		return true;
	}
}

uint64 FShooterProfileImageRules::ToSteamId(const FUniqueNetIdRepl& UniqueId)
{
	if (!UniqueId.IsV1() || !UniqueId.IsValid() || UniqueId.GetType() != STEAM_SUBSYSTEM)
	{
		return 0;
	}

	const FString IdText = UniqueId->ToString();
	if (!ShooterProfileImageParsing::IsDecimalDigits(IdText))
	{
		return 0;
	}

	const CSteamID SteamId(FCString::Strtoui64(*IdText, nullptr, 10));
	return (SteamId.IsValid() && SteamId.BIndividualAccount()) ? SteamId.ConvertToUint64() : 0;
}

UTexture2D* UShooterProfileImageSubsystem::FindOrRequestProfileImage(const FUniqueNetIdRepl& UniqueId)
{
	const uint64 SteamIdValue = FShooterProfileImageRules::ToSteamId(UniqueId);
	if (SteamIdValue == 0)
	{
		return nullptr;
	}

	if (const TObjectPtr<UTexture2D>* Cached = ImageBySteamId.Find(SteamIdValue))
	{
		return *Cached;
	}

	// ISteamFriends에서 인터페이스, 프로필 등가져오기
	ISteamFriends* Friends = SteamFriends();
	if (!Friends)
	{
		return nullptr;
	}

	const CSteamID SteamId(SteamIdValue);
	const int32 ImageHandle = Friends->GetMediumFriendAvatar(SteamId);
	if (ImageHandle <= 0)
	{
		// Steam에 받아오라고 재요청
		if (!RequestedSteamIds.Contains(SteamIdValue))
		{
			RequestedSteamIds.Add(SteamIdValue);
			Friends->RequestUserInformation(SteamId, false);
		}
		return nullptr;
	}

	// Steam 이미지 크기와 RGBA 픽셀 데이터 조회 
	ISteamUtils* Utils = SteamUtils();
	if (!Utils)
	{
		return nullptr;
	}

	// 이미지 크기 조회
	uint32 Width = 0;
	uint32 Height = 0;
	if (!Utils->GetImageSize(ImageHandle, &Width, &Height) || Width == 0 || Height == 0)
	{
		return nullptr;
	}

	// RGBA 픽셀 데이터를 담을 버퍼 생성
	TArray<uint8> Buffer;
	Buffer.SetNumUninitialized(static_cast<int32>(Width * Height * 4));
	// Steam 이미지 픽셀 데이터 조회
	if (!Utils->GetImageRGBA(ImageHandle, Buffer.GetData(), Buffer.Num()))
	{
		return nullptr;
	}

	// RGBA 픽셀 데이터로 런타임 Texture 생성
	UTexture2D* Texture = UTexture2D::CreateTransient(
		static_cast<int32>(Width), static_cast<int32>(Height), PF_R8G8B8A8, NAME_None, Buffer);
	if (!IsValid(Texture))
	{
		return nullptr;
	}

	ImageBySteamId.Add(SteamIdValue, Texture);
	return Texture;
}
