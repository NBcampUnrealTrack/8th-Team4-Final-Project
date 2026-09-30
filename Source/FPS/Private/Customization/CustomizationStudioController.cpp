#include "Customization/CustomizationStudioController.h"
#include "Customization/CustomizationStudioActor.h"
#include "Customization/CustomizationStudioWidget.h"
#include "GroomAsset.h"
#include "Engine/LocalPlayer.h"
#include "EngineUtils.h"
#include "UObject/ConstructorHelpers.h"
#include "Customization/AppearanceLocalPlayerSubsystem.h"
#include "Engine/GameInstance.h"
#include "Session/SessionSubSystem.h"
#include "UI/Transition/ScreenTransitionSubsystem.h"

ACustomizationStudioController::ACustomizationStudioController()
{
 bShowMouseCursor=true;PrimaryActorTick.bCanEverTick=true;bAutoManageActiveCameraTarget=false;
 WidgetClass=UCustomizationStudioWidget::StaticClass();
}
void ACustomizationStudioController::BeginPlay()
{
 Super::BeginPlay();if(!IsLocalController())return;
 for(TActorIterator<ACustomizationStudioActor> It(GetWorld());It;++It){Studio=*It;break;}
 if(Studio){SetViewTarget(Studio);if(Studio->Config){ActiveClass=FName(*Studio->Config->ClassId);Studio->ClassConfigs.FindOrAdd(ActiveClass)=Studio->Config;}}else Status=TEXT("커스터마이징 프리뷰 액터가 없습니다.");
 RefreshAccount();
 Screen=CreateWidget<UCustomizationStudioWidget>(this,WidgetClass);if(Screen){Screen->AddToViewport(100);FInputModeUIOnly Mode;Mode.SetWidgetToFocus(Screen->TakeWidget());Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);SetInputMode(Mode);}
 // 진입 트랜지션
 if(auto* Transition=GetGameInstance()?GetGameInstance()->GetSubsystem<UScreenTransitionSubsystem>():nullptr)Transition->EndTransition();
}
void ACustomizationStudioController::Tick(float Delta){Super::Tick(Delta);if(!IsLocalController())return;AccountPoll+=Delta;if(AccountPoll>=1){AccountPoll=0;RefreshAccount();}}
void ACustomizationStudioController::EndPlay(const EEndPlayReason::Type Reason){if(Screen)Screen->RemoveFromParent();Super::EndPlay(Reason);}
void ACustomizationStudioController::RefreshAccount()
{
 if(auto* LP=GetLocalPlayer())if(auto* Storage=LP->GetSubsystem<UAppearanceLocalPlayerSubsystem>())
 {
  Storage->RefreshIdentity();
  const bool Changed=Service!=Storage->GetService()||Account!=Storage->GetAccount();
  SetAccountIdentity(Storage->GetService(),Storage->GetAccount());
  if(Changed)Storage->RefreshCloud();
  return;
 }
 SetAccountIdentity({},{});
}
void ACustomizationStudioController::SetAccountIdentity(const FString& NewService,const FString& NewAccount)
{
 if(NewService!=Service||NewAccount!=Account||AccountLabel.IsEmpty()){
  Service=NewService;Account=NewAccount;AccountLabel=Service==TEXT("STEAM")?TEXT("Steam 계정 · PC 저장 + Cloud 동기화"):Service==TEXT("LOCAL_TEST")?TEXT("에디터 테스트 · Steam 저장과 분리됨"):TEXT("Steam 미연결 · 저장 불가");
  ColorEditPart=INDEX_NONE;ClassDrafts.Reset();Draft=DefaultAppearance();bDirty=false;LoadAppearance();
 }
}
FCustomizationAppearance ACustomizationStudioController::DefaultAppearance() const
{
 FCustomizationAppearance D;if(Studio&&Studio->Config){D.BodyType=Studio->Config->DefaultBodyType;D.HairId=Studio->Config->DefaultHairId;}return D;
}
TArray<FString> ACustomizationStudioController::BodyChoices() const
{return Studio&&Studio->Config?Studio->Config->BodyTypes:TArray<FString>();}
bool ACustomizationStudioController::SelectClass(FName Class)
{
 if(IsColorEditOpen()||!Studio)return false;
 auto* Entry=Studio->ClassConfigs.Find(Class);auto* Candidate=Entry?Entry->Get():nullptr;
 if(!Candidate||Candidate->ClassId!=Class.ToString()||!Candidate->CustomizableObject||!Candidate->BodyTypes.Contains(Candidate->DefaultBodyType))return false;
 FCustomizationAppearance Default;Default.BodyType=Candidate->DefaultBodyType;Default.HairId=Candidate->DefaultHairId;if(!Default.ValidateForClass(Candidate->ClassId))return false;
 if(Class==ActiveClass&&Studio->Config==Candidate)return true;
 if(!Studio->SetClassConfig(Candidate))return false;
 ClassDrafts.Add(ActiveClass,{Draft,bDirty,Status});ActiveClass=Class;
 if(const auto* Cached=ClassDrafts.Find(Class)){Draft=Cached->Data;bDirty=Cached->bDirty;Status=Cached->Status;Studio->RequestAppearance(Draft);}
 else{Draft=DefaultAppearance();bDirty=false;LoadAppearance();}
 return true;
}
bool ACustomizationStudioController::CanEditPart(int32 Part) const
{
 if(Part<0||Part>10)return false;
 if(Part==10)return Draft.HairId!=TEXT("Bald");
 if(Part>=8)return true;
 return !Studio||!Studio->Config||Studio->Config->EnabledClothingParts.Contains(FName(*FCustomizationAppearance::Parts()[Part]));
}
void ACustomizationStudioController::Changed(){bDirty=true;Status=TEXT("저장하지 않은 변경사항");if(Studio)Studio->RequestAppearance(Draft);}
void ACustomizationStudioController::SelectBody(const FString& Type)
{
 if(IsColorEditOpen()||!Studio||!Studio->IsBodyValid(Type))return;
 // IDs such as Default are body-local; match the actual hairstyle before switching.
 FName PreviousHair=NAME_None;
 // The same hairstyle can be duplicated into different MetaHuman asset folders.
 if(Studio&&Draft.HairId!=TEXT("Bald"))for(const auto& H:Studio->HairChoices(Draft.BodyType))if(H.Id==Draft.HairId){PreviousHair=H.Groom->GetFName();break;}
 Draft.BodyType=Type;
 if(Studio&&Draft.HairId!=TEXT("Bald"))
 {
  const auto Choices=Studio->HairChoices(Type);
  const auto* Match=Choices.FindByPredicate([PreviousHair](const FStudioGroomChoice& H){return !PreviousHair.IsNone()&&H.Groom->GetFName()==PreviousHair;});
  // Keep an unresolved selection visible as an error. Never silently replace
  // a hairstyle merely because its destination binding is missing.
  if(Match)Draft.HairId=Match->Id;
 }
 Changed();
}
void ACustomizationStudioController::ResetAppearance(){ColorEditPart=INDEX_NONE;Draft=DefaultAppearance();Changed();}

void ACustomizationStudioController::RequestReturnToFrontend()
{
 auto* GI= GetGameInstance();
 auto* Sessions=GI?GI->GetSubsystem<USessionSubSystem>():nullptr;
 auto* Transition=GI?GI->GetSubsystem<UScreenTransitionSubsystem>():nullptr;
 
 if(!Sessions||(Transition&&Transition->IsTransitionActive()))
 {
  return;
 }
 
 const FSimpleDelegate Request=FSimpleDelegate::CreateWeakLambda(Sessions,[Sessions]()
 {
  Sessions->LeaveRoomAndReturnToFrontend();
 });
 
 if(!Transition||!Transition->BeginTransition(Request))
 {
  Request.ExecuteIfBound();
 }
}

void ACustomizationStudioController::LoadAppearance()
{
 ColorEditPart=INDEX_NONE;
 if(Account.IsEmpty()){Status=TEXT("Steam 연결 후 저장할 수 있습니다.");if(Studio)Studio->RequestAppearance(Draft);return;}
 auto* Storage=GetLocalPlayer()?GetLocalPlayer()->GetSubsystem<UAppearanceLocalPlayerSubsystem>():nullptr;
 FCustomizationAppearance Loaded;auto Result=Storage?Storage->LoadSavedAppearance(ActiveClass,Loaded):ECustomizationLoadResult::Invalid;
 if(Result==ECustomizationLoadResult::Invalid){Status=TEXT("저장 파일이 손상되었거나 버전/소유자가 다릅니다. 덮어쓰지 않습니다.");if(Studio)Studio->RequestAppearance(Draft);return;}
 if(Result==ECustomizationLoadResult::Loaded)Draft=Loaded;else Draft=DefaultAppearance();
 if(Studio&&!Studio->IsHairValid(Draft.BodyType,Draft.HairId)){Status=TEXT("저장된 헤어의 바인딩이 없습니다. 유효한 헤어 또는 X를 선택해야 저장할 수 있습니다.");bDirty=false;}
 else{Status=Result==ECustomizationLoadResult::Loaded?TEXT("저장된 외형을 불러왔습니다."):TEXT("저장된 외형 없음 · 기본값으로 시작합니다.");bDirty=false;}
 if(Studio)Studio->RequestAppearance(Draft);
}
bool ACustomizationStudioController::CanSave() const
{
 return !IsColorEditOpen() && !Account.IsEmpty() && Studio && Studio->Config && Studio->Config->ClassId==ActiveClass.ToString() && Studio->IsAppearanceReady() && Draft.ValidateForClass(ActiveClass.ToString()) && Studio->IsHairValid(Draft.BodyType,Draft.HairId);
}
void ACustomizationStudioController::SaveAppearance()
{
 if(IsColorEditOpen()){Status=TEXT("색상 선택을 확인하거나 취소한 후 저장하세요.");return;}
 const FString Previous=Service+TEXT(":")+Account;RefreshAccount();if(Previous!=Service+TEXT(":")+Account){Status=TEXT("계정이 변경되어 새 계정 설정을 불러왔습니다. 확인 후 저장하세요.");return;}
 if(!CanSave()){Status=Account.IsEmpty()?TEXT("유효한 계정이 필요합니다."):TEXT("최신 외형 미리보기가 준비된 후 저장할 수 있습니다.");return;}
 auto* Storage=GetLocalPlayer()?GetLocalPlayer()->GetSubsystem<UAppearanceLocalPlayerSubsystem>():nullptr;
 if(Storage&&Storage->SaveAppearance(ActiveClass,Draft)){bDirty=false;Status=TEXT("PC 저장 완료 · 다음 입장/리스폰 시 적용");}
 else Status=TEXT("저장 실패 · 파일/권한/버전을 확인하세요. 기존 파일은 자동 초기화하지 않습니다.");
}

FString ACustomizationStudioController::CloudStatus() const
{
 auto* Storage=GetLocalPlayer()?GetLocalPlayer()->GetSubsystem<UAppearanceLocalPlayerSubsystem>():nullptr;
 return Storage?Storage->GetStatusText(ActiveClass):FString();
}
bool ACustomizationStudioController::HasCloudConflict() const
{
 auto* Storage=GetLocalPlayer()?GetLocalPlayer()->GetSubsystem<UAppearanceLocalPlayerSubsystem>():nullptr;
 return Storage&&Storage->GetSaveStatus(ActiveClass)==EAppearanceSaveStatus::Conflict;
}
void ACustomizationStudioController::ResolveCloudConflict(bool UseLocal)
{
 if(IsColorEditOpen()||bDirty){Status=TEXT("편집을 먼저 저장하거나 불러오기로 되돌린 뒤 사본을 선택하세요.");return;}
 RefreshAccount();
 if(auto* LP=GetLocalPlayer())if(auto* Storage=LP->GetSubsystem<UAppearanceLocalPlayerSubsystem>())
 {
  Storage->ResolveConflict(ActiveClass,UseLocal);
  Status=TEXT("사본 선택 처리 중 · 동기화 완료 후 불러오기를 눌러 확인하세요.");
 }
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
 if(IsColorEditOpen()||!CanEditPart(Part))return;
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
