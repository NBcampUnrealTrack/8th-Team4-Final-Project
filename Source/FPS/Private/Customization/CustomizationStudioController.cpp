#include "Customization/CustomizationStudioController.h"
#include "Customization/CustomizationStudioActor.h"
#include "Customization/CustomizationStudioWidget.h"
#include "GroomAsset.h"
#include "Engine/LocalPlayer.h"
#include "EngineUtils.h"
#include "UObject/ConstructorHelpers.h"

ACustomizationStudioController::ACustomizationStudioController()
{
 bShowMouseCursor=true;PrimaryActorTick.bCanEverTick=true;bAutoManageActiveCameraTarget=false;
 WidgetClass=UCustomizationStudioWidget::StaticClass();
}
void ACustomizationStudioController::BeginPlay()
{
 Super::BeginPlay();if(!IsLocalController())return;
 for(TActorIterator<ACustomizationStudioActor> It(GetWorld());It;++It){Studio=*It;break;}
 if(Studio)SetViewTarget(Studio);else Status=TEXT("커스터마이징 프리뷰 액터가 없습니다.");
 RefreshAccount();
 Screen=CreateWidget<UCustomizationStudioWidget>(this,WidgetClass);if(Screen){Screen->AddToViewport(100);FInputModeUIOnly Mode;Mode.SetWidgetToFocus(Screen->TakeWidget());Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);SetInputMode(Mode);}
}
void ACustomizationStudioController::Tick(float Delta){Super::Tick(Delta);if(!IsLocalController())return;AccountPoll+=Delta;if(AccountPoll>=1){AccountPoll=0;RefreshAccount();}}
void ACustomizationStudioController::EndPlay(const EEndPlayReason::Type Reason){if(Screen)Screen->RemoveFromParent();Super::EndPlay(Reason);}
void ACustomizationStudioController::RefreshAccount()
{
 FString NewService,NewAccount;
 if(auto* LP=GetLocalPlayer()){auto Id=LP->GetPreferredUniqueNetId();auto NetId=Id.GetUniqueNetId();if(NetId.IsValid()&&NetId->IsValid()&&NetId->GetType().ToString().Equals(TEXT("STEAM"),ESearchCase::IgnoreCase)){NewService=TEXT("STEAM");NewAccount=NetId->ToString();}}
#if WITH_EDITOR
 if(NewAccount.IsEmpty()){NewService=TEXT("LOCAL_TEST");NewAccount=TEXT("EditorPreview");}
#endif
 if(NewService!=Service||NewAccount!=Account||AccountLabel.IsEmpty()){
  Service=NewService;Account=NewAccount;AccountLabel=Service==TEXT("STEAM")?TEXT("Steam 계정 · PC 로컬 저장"):Service==TEXT("LOCAL_TEST")?TEXT("에디터 테스트 · Steam 저장과 분리됨"):TEXT("Steam 미연결 · 저장 불가");
  ColorEditPart=INDEX_NONE;Draft=FCustomizationAppearance();bDirty=false;LoadAppearance();
 }
}
void ACustomizationStudioController::Changed(){bDirty=true;Status=TEXT("저장하지 않은 변경사항");if(Studio)Studio->RequestAppearance(Draft);}
void ACustomizationStudioController::SelectBody(const FString& Type)
{
 if(IsColorEditOpen())return;
 // IDs such as Default are body-local; match the actual hairstyle before switching.
 FName PreviousHair=NAME_None;
 // The same hairstyle can be duplicated into different MetaHuman asset folders.
 if(Studio&&Draft.HairId!=TEXT("Bald"))for(const auto& H:Studio->HairChoices(Draft.BodyType))if(H.Id==Draft.HairId){PreviousHair=H.Groom->GetFName();break;}
 Draft.BodyType=Type;
 if(Studio&&Draft.HairId!=TEXT("Bald"))
 {
  const auto Choices=Studio->HairChoices(Type);
  const auto* Match=Choices.FindByPredicate([PreviousHair](const FStudioGroomChoice& H){return !PreviousHair.IsNone()&&H.Groom->GetFName()==PreviousHair;});
  Draft.HairId=Match?Match->Id:Studio->IsHairValid(Type,TEXT("Default"))?FName(TEXT("Default")):FName(TEXT("Bald"));
 }
 Changed();
}
void ACustomizationStudioController::ResetAppearance(){ColorEditPart=INDEX_NONE;Draft=FCustomizationAppearance();Changed();}
void ACustomizationStudioController::LoadAppearance()
{
 ColorEditPart=INDEX_NONE;
 if(Account.IsEmpty()){Status=TEXT("Steam 연결 후 저장할 수 있습니다.");if(Studio)Studio->RequestAppearance(Draft);return;}
 FCustomizationAppearance Loaded;auto Result=FCustomizationStorage::Load(Service,Account,TEXT("Ranger"),Loaded);
 if(Result==ECustomizationLoadResult::Invalid){Status=TEXT("저장 파일이 손상되었거나 버전/소유자가 다릅니다. 덮어쓰지 않습니다.");if(Studio)Studio->RequestAppearance(Draft);return;}
 if(Result==ECustomizationLoadResult::Loaded)Draft=Loaded;else Draft=FCustomizationAppearance();
 if(Studio&&!Studio->IsHairValid(Draft.BodyType,Draft.HairId)){Status=TEXT("저장된 헤어를 사용할 수 없습니다. 기본 헤어로 표시합니다.");Draft.HairId=Studio->IsHairValid(Draft.BodyType,TEXT("Default"))?FName(TEXT("Default")):FName(TEXT("Bald"));bDirty=true;}
 else{Status=Result==ECustomizationLoadResult::Loaded?TEXT("저장된 외형을 불러왔습니다."):TEXT("저장된 외형 없음 · 기본값으로 시작합니다.");bDirty=false;}
 if(Studio)Studio->RequestAppearance(Draft);
}
bool ACustomizationStudioController::CanSave() const
{
 return !IsColorEditOpen() && !Account.IsEmpty() && Studio && Studio->IsAppearanceReady() && Draft.Validate() && Studio->IsHairValid(Draft.BodyType,Draft.HairId);
}
void ACustomizationStudioController::SaveAppearance()
{
 if(IsColorEditOpen()){Status=TEXT("색상 선택을 확인하거나 취소한 후 저장하세요.");return;}
 const FString Previous=Service+TEXT(":")+Account;RefreshAccount();if(Previous!=Service+TEXT(":")+Account){Status=TEXT("계정이 변경되어 새 계정 설정을 불러왔습니다. 확인 후 저장하세요.");return;}
 if(!CanSave()){Status=Account.IsEmpty()?TEXT("유효한 계정이 필요합니다."):TEXT("최신 외형 미리보기가 준비된 후 저장할 수 있습니다.");return;}
 if(FCustomizationStorage::Save(Service,Account,TEXT("Ranger"),Draft)){bDirty=false;Status=TEXT("저장 완료 · 다음 입장 시 자동으로 불러옵니다.");}
 else Status=TEXT("저장 실패 · 파일/권한/버전을 확인하세요. 기존 파일은 자동 초기화하지 않습니다.");
}

namespace
{
FLinearColor PartColor(const FCustomizationAppearance& Data,int32 Part)
{
 if(Part==8)return Data.SkinColor;
 if(Part==9)return Data.EyeColor;
 if(Part==10)return Data.HairColor;
 return Data.Colors.IsValidIndex(Part)?Data.Colors[Part]:FLinearColor::White;
}
}
void ACustomizationStudioController::BeginColorEdit(int32 Part)
{
 if(IsColorEditOpen()||Part<0||Part>10||(Part==10&&Draft.HairId==TEXT("Bald")))return;
 BeforeColorEdit=Draft;bDirtyBeforeColorEdit=bDirty;StatusBeforeColorEdit=Status;ColorEditPart=Part;
}
void ACustomizationStudioController::EndColorEdit(bool bAccept)
{
 if(!IsColorEditOpen())return;
 if(!bAccept){Draft=BeforeColorEdit;bDirty=bDirtyBeforeColorEdit;Status=StatusBeforeColorEdit;if(Studio)Studio->RequestAppearance(Draft);}
 ColorEditPart=INDEX_NONE;
}
FLinearColor ACustomizationStudioController::GetEditingColor() const{return PartColor(Draft,ColorEditPart);}
FLinearColor ACustomizationStudioController::GetPreviousColor() const{return PartColor(BeforeColorEdit,ColorEditPart);}
void ACustomizationStudioController::SetEditingColor(FLinearColor Color)
{
 if(!IsColorEditOpen()||!FMath::IsFinite(Color.R)||!FMath::IsFinite(Color.G)||!FMath::IsFinite(Color.B))return;
 Color=Color.GetClamped(0.f,1.f);Color.A=1.f;
 if(ColorEditPart==8)Draft.SkinColor=Color;
 else if(ColorEditPart==9)Draft.EyeColor=Color;
 else if(ColorEditPart==10){Draft.HairColor=Color;Draft.bUseOriginalHairColor=false;}
 else {Draft.Colors[ColorEditPart]=Color;Draft.Originals[ColorEditPart]=0.f;}
 Changed();
}
void ACustomizationStudioController::RestoreEditingPart()
{
 if(!IsColorEditOpen())return;
 if(ColorEditPart==8){Draft.SkinColor=FLinearColor::White;Draft.SkinValue=1.f;}
 else if(ColorEditPart==9)Draft.EyeColor=FLinearColor::White;
 else if(ColorEditPart==10){Draft.HairColor=FLinearColor::White;Draft.bUseOriginalHairColor=true;}
 else {Draft.Colors[ColorEditPart]=FLinearColor::White;Draft.Values[ColorEditPart]=1.f;Draft.Originals[ColorEditPart]=1.f;}
 Changed();
}
