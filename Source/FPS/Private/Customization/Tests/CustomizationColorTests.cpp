#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Customization/CustomizationProfile.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/UnrealType.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Customization/CustomizationStudioWidget.h"
#include "Customization/CustomizationStudioController.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameInstance.h"
#include "Engine/Engine.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Types/SlateAttributeMetaData.h"
#include "Widgets/Input/SEditableText.h"

namespace ColorTests
{
TSharedPtr<SWidget> Find(const TSharedRef<SWidget>& Root,TFunctionRef<bool(const SWidget&)> Match)
{
 // This synchronous test has no intervening Slate frame after a click.
 FSlateAttributeMetaData::UpdateAllAttributes(Root.Get(),FSlateAttributeMetaData::EInvalidationPermission::AllowInvalidationIfConstructed);
 if(!Root->GetVisibility().IsVisible())return nullptr;
 if(Match(Root.Get()))return Root;
 auto* Children=Root->GetChildren();
 for(int32 I=0;I<Children->Num();++I)if(auto W=Find(Children->GetChildAt(I),Match))return W;
 return nullptr;
}
TSharedPtr<SButton> Button(const TSharedRef<SWidget>& Root,const FString& Text)
{
 auto Found=Find(Root,[&](const SWidget& W){
  if(W.GetType()!=FName(TEXT("SButton")))return false;
  return Find(ConstCastSharedRef<SWidget>(W.AsShared()),[&](const SWidget& Child){
   return Child.GetType()==FName(TEXT("STextBlock"))&&static_cast<const STextBlock&>(Child).GetText().ToString()==Text;
  }).IsValid();
 });
 return StaticCastSharedPtr<SButton>(Found);
}
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCustomizationPopupTest,
 "FPS.Customization.ColorPopup", EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCustomizationPopupTest::RunTest(const FString&)
{
 auto* World=UWorld::CreateWorld(EWorldType::Game,false);
 auto* Controller=World->SpawnActor<ACustomizationStudioController>();
 auto* GameInstance=NewObject<UGameInstance>();World->SetGameInstance(GameInstance);
 auto* Player=NewObject<ULocalPlayer>(GEngine);Player->PlayerController=Controller;Controller->Player=Player;
 World->AddController(Controller);
 auto* Widget=NewObject<UCustomizationStudioWidget>(Controller);Widget->Initialize();Widget->SetOwningPlayer(Controller);Widget->bUseMutableLevelLayout=true;
 TestEqual(TEXT("Widget has a real local owner"),Widget->GetOwningPlayer(),static_cast<APlayerController*>(Controller));
 const auto Root=Widget->TakeWidget();Root->SlatePrepass();
 auto Top=ColorTests::Button(Root,TEXT("상의"));
 if(TestTrue(TEXT("Part button exists"),Top.IsValid()))Top->SimulateClick();
 TestTrue(TEXT("Part click opens visible color wheel"),ColorTests::Find(Root,[](const SWidget& W){return W.GetType()==FName(TEXT("SColorWheel"));}).IsValid());
 TestTrue(TEXT("Click starts an edit on owning controller"),Controller->IsColorEditOpen());
 auto UnchangedHex=ColorTests::Find(Root,[](const SWidget& W){return W.GetType()==FName(TEXT("SEditableTextBox"));});
 if(UnchangedHex)
 {
  auto Text=ColorTests::Find(UnchangedHex.ToSharedRef(),[](const SWidget& W){return W.GetType()==FName(TEXT("SEditableText"));});
  if(Text)Text->OnFocusLost(FFocusEvent(EFocusCause::Mouse,0));
 }
 TestEqual(TEXT("Viewing HEX preserves original texture"),Controller->Draft.Originals[0],1.f);
 TestFalse(TEXT("Viewing HEX does not mark dirty"),Controller->bDirty);
 Controller->SetEditingColor(FLinearColor(.1f,.2f,.3f,1.f));
 TestEqual(TEXT("Live draft changes"),Controller->Draft.Colors[0],FLinearColor(.1f,.2f,.3f,1.f));
 auto Cancel=ColorTests::Button(Root,TEXT("취소"));
 if(TestTrue(TEXT("Cancel button exists"),Cancel.IsValid()))Cancel->SimulateClick();
 TestEqual(TEXT("Cancel restores original color"),Controller->Draft.Colors[0],FLinearColor::White);
 TestEqual(TEXT("Cancel restores original texture mode"),Controller->Draft.Originals[0],1.f);
 TestFalse(TEXT("Cancel restores clean state"),Controller->bDirty);
 TestFalse(TEXT("Cancel closes popup"),Controller->IsColorEditOpen());
 Top->SimulateClick();Controller->SetEditingColor(FLinearColor(.4f,.3f,.2f,1.f));
 auto Confirm=ColorTests::Button(Root,TEXT("확인"));
 if(TestTrue(TEXT("Confirm button exists"),Confirm.IsValid()))Confirm->SimulateClick();
 TestEqual(TEXT("Confirm keeps selected color"),Controller->Draft.Colors[0],FLinearColor(.4f,.3f,.2f,1.f));
 TestTrue(TEXT("Confirm leaves unsaved changes"),Controller->bDirty);
 auto Hair=ColorTests::Button(Root,TEXT("헤어 색상"));
 if(TestTrue(TEXT("Hair color button exists"),Hair.IsValid()))Hair->SimulateClick();
 auto HairHex=ColorTests::Find(Root,[](const SWidget& W){return W.GetType()==FName(TEXT("SEditableTextBox"));});
 if(HairHex)
 {
  auto Text=ColorTests::Find(HairHex.ToSharedRef(),[](const SWidget& W){return W.GetType()==FName(TEXT("SEditableText"));});
  if(Text)Text->OnFocusLost(FFocusEvent(EFocusCause::Mouse,0));
 }
 TestTrue(TEXT("Viewing hair HEX does not turn authored hair white"),Controller->Draft.bUseOriginalHairColor);
 Controller->SetEditingColor(FLinearColor::Red);
 TestFalse(TEXT("Dye disables authored hair mode"),Controller->Draft.bUseOriginalHairColor);
 auto Restore=ColorTests::Button(Root,TEXT("원본 색 복원"));
 if(TestTrue(TEXT("Restore exists"),Restore.IsValid()))Restore->SimulateClick();
 TestTrue(TEXT("Restore returns authored hair mode"),Controller->Draft.bUseOriginalHairColor);
 Controller->EndColorEdit(false);
 TestTrue(TEXT("Cancel preserves earlier unsaved work"),Controller->bDirty);
 Top->SimulateClick();
 auto HexBox=ColorTests::Find(Root,[](const SWidget& W){return W.GetType()==FName(TEXT("SEditableTextBox"));});
 if(TestTrue(TEXT("HEX field exists"),HexBox.IsValid()))
 {
  auto Text=StaticCastSharedPtr<SEditableText>(ColorTests::Find(HexBox.ToSharedRef(),[](const SWidget& W){return W.GetType()==FName(TEXT("SEditableText"));}));
  if(TestTrue(TEXT("HEX text editor exists"),Text.IsValid()))
  {
   Text->SetText(FText::FromString(TEXT("#00FF00")));
   StaticCastSharedPtr<SWidget>(Text)->OnFocusLost(FFocusEvent(EFocusCause::Mouse,0));
   TestEqual(TEXT("HEX applies when clicking away without Enter"),Controller->Draft.Colors[0],FLinearColor::Green);
   Text->SetText(FText::FromString(TEXT("not-hex")));StaticCastSharedPtr<SWidget>(Text)->OnFocusLost(FFocusEvent(EFocusCause::Mouse,0));
   Confirm=ColorTests::Button(Root,TEXT("확인"));if(Confirm)Confirm->SimulateClick();
   TestTrue(TEXT("Invalid HEX cannot silently confirm"),Controller->IsColorEditOpen());
  }
 }
 auto Popup=ColorTests::Find(Root,[](const SWidget& W){return W.GetType()==FName(TEXT("SMutableColorPanel"));});
 if(TestTrue(TEXT("Popup panel exists"),Popup.IsValid()))Popup->OnPreviewKeyDown(FGeometry(),FKeyEvent(EKeys::Escape,FModifierKeysState(),0,false,0,0));
 TestFalse(TEXT("Preview Escape closes before text can consume key"),Controller->IsColorEditOpen());
 TestEqual(TEXT("Escape restores pre-popup color"),Controller->Draft.Colors[0],FLinearColor(.4f,.3f,.2f,1.f));
 Controller->Draft.HairId=TEXT("Bald");Controller->BeginColorEdit(10);
 TestFalse(TEXT("Bald blocks hair color popup"),Controller->IsColorEditOpen());
 Controller->BeginColorEdit(-1);Controller->BeginColorEdit(11);
 TestFalse(TEXT("Invalid part is ignored"),Controller->IsColorEditOpen());
 World->DestroyWorld(false);
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCustomizationHairColorStorageTest,
 "FPS.Customization.HairColor.Storage", EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCustomizationHairColorStorageTest::RunTest(const FString&)
{
 // Missing hair fields must not silently discard a user's dye on save/load.
 auto* Color=FindFProperty<FStructProperty>(FCustomizationAppearance::StaticStruct(),TEXT("HairColor"));
 auto* Original=FindFProperty<FBoolProperty>(FCustomizationAppearance::StaticStruct(),TEXT("bUseOriginalHairColor"));
 if(!TestNotNull(TEXT("Hair color persists in appearance"),Color)||!TestNotNull(TEXT("Original hair mode persists"),Original))return false;
 FCustomizationAppearance Draft;
 *Color->ContainerPtrToValuePtr<FLinearColor>(&Draft)=FLinearColor(.2f,.4f,.8f,1.f);
 Original->SetPropertyValue_InContainer(&Draft,false);
 const FString Account=FGuid::NewGuid().ToString();
 TestTrue(TEXT("Save dye"),FCustomizationStorage::Save(TEXT("COLOR_TEST"),Account,TEXT("Ranger"),Draft));
 FCustomizationAppearance Loaded;
 TestEqual(TEXT("Load dye"),FCustomizationStorage::Load(TEXT("COLOR_TEST"),Account,TEXT("Ranger"),Loaded),ECustomizationLoadResult::Loaded);
 TestEqual(TEXT("Dye survives roundtrip"),*Color->ContainerPtrToValuePtr<FLinearColor>(&Loaded),FLinearColor(.2f,.4f,.8f,1.f));
 TestFalse(TEXT("Dye mode survives roundtrip"),Original->GetPropertyValue_InContainer(&Loaded));
 UGameplayStatics::DeleteGameInSlot(FCustomizationStorage::SlotFor(TEXT("COLOR_TEST"),Account,TEXT("Ranger")),0);
 Color->ContainerPtrToValuePtr<FLinearColor>(&Draft)->R=-.1f;
 TestFalse(TEXT("Reject invalid dye"),Draft.Validate());
 return true;
}

// Captured with the old schema before adding hair fields; never regenerate this
// with current code, which would no longer test a real backwards-compatible load.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCustomizationLegacyColorFixture,
 "FPS.Customization.HairColor.Legacy", EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCustomizationLegacyColorFixture::RunTest(const FString&)
{
 const FString Path=FPaths::ProjectDir()/TEXT("Source/FPS/Private/Customization/Tests/Fixtures/AppearanceV1.sav");
 TArray<uint8> Bytes;
 TestTrue(TEXT("Read legacy fixture"),FFileHelper::LoadFileToArray(Bytes,*Path));
 auto* Loaded=Cast<UCustomizationProfile>(UGameplayStatics::LoadGameFromMemory(Bytes));
 if(!TestNotNull(TEXT("Legacy loads"),Loaded))return false;
 TestTrue(TEXT("Legacy still accepted"),FCustomizationStorage::Matches(Loaded,TEXT("COLOR_TEST"),TEXT("LegacyFixture"),TEXT("Ranger")));
 TestEqual(TEXT("Legacy body unchanged"),Loaded->Appearance.BodyType,FString(TEXT("Fab")));
 TestEqual(TEXT("Legacy skin unchanged"),Loaded->Appearance.SkinValue,.7f);
 auto* Original=FindFProperty<FBoolProperty>(FCustomizationAppearance::StaticStruct(),TEXT("bUseOriginalHairColor"));
 if(Original)TestTrue(TEXT("Legacy retains authored hair"),Original->GetPropertyValue_InContainer(&Loaded->Appearance));
 return true;
}
#endif
