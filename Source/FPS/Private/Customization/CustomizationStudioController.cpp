#include "Customization/CustomizationStudioController.h"
#include "Customization/CustomizationStudioActor.h"
#include "Customization/CustomizationStudioWidget.h"
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
  Draft=FCustomizationAppearance();bDirty=false;LoadAppearance();
 }
}
void ACustomizationStudioController::Changed(){bDirty=true;Status=TEXT("저장하지 않은 변경사항");if(Studio)Studio->RequestAppearance(Draft);}
void ACustomizationStudioController::SelectBody(const FString& Type){Draft.BodyType=Type;if(Studio&&!Studio->IsHairValid(Type,Draft.HairId))Draft.HairId=Studio->IsHairValid(Type,TEXT("Default"))?FName(TEXT("Default")):FName(TEXT("Bald"));Changed();}
void ACustomizationStudioController::ResetAppearance(){Draft=FCustomizationAppearance();Changed();}
void ACustomizationStudioController::LoadAppearance()
{
 if(Account.IsEmpty()){Status=TEXT("Steam 연결 후 저장할 수 있습니다.");if(Studio)Studio->RequestAppearance(Draft);return;}
 FCustomizationAppearance Loaded;auto Result=FCustomizationStorage::Load(Service,Account,TEXT("Ranger"),Loaded);
 if(Result==ECustomizationLoadResult::Invalid){Status=TEXT("저장 파일이 손상되었거나 버전/소유자가 다릅니다. 덮어쓰지 않습니다.");if(Studio)Studio->RequestAppearance(Draft);return;}
 if(Result==ECustomizationLoadResult::Loaded)Draft=Loaded;else Draft=FCustomizationAppearance();
 if(Studio&&!Studio->IsHairValid(Draft.BodyType,Draft.HairId)){Status=TEXT("저장된 헤어를 사용할 수 없습니다. 기본 헤어로 표시합니다.");Draft.HairId=Studio->IsHairValid(Draft.BodyType,TEXT("Default"))?FName(TEXT("Default")):FName(TEXT("Bald"));bDirty=true;}
 else{Status=Result==ECustomizationLoadResult::Loaded?TEXT("저장된 외형을 불러왔습니다."):TEXT("저장된 외형 없음 · 기본값으로 시작합니다.");bDirty=false;}
 if(Studio)Studio->RequestAppearance(Draft);
}
void ACustomizationStudioController::SaveAppearance()
{
 const FString Previous=Service+TEXT(":")+Account;RefreshAccount();if(Previous!=Service+TEXT(":")+Account){Status=TEXT("계정이 변경되어 새 계정 설정을 불러왔습니다. 확인 후 저장하세요.");return;}
 if(!CanSave()||!Studio->IsHairValid(Draft.BodyType,Draft.HairId)){Status=TEXT("유효한 계정과 헤어 설정이 필요합니다.");return;}
 if(FCustomizationStorage::Save(Service,Account,TEXT("Ranger"),Draft)){bDirty=false;Status=TEXT("저장 완료 · 다음 입장 시 자동으로 불러옵니다.");}
 else Status=TEXT("저장 실패 · 파일/권한/버전을 확인하세요. 기존 파일은 자동 초기화하지 않습니다.");
}
