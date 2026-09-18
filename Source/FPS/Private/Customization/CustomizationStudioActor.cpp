#include "Customization/CustomizationStudioActor.h"
#include "MuCO/CustomizableObject.h"
#include "MuCO/CustomizableObjectInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Camera/CameraComponent.h"
#include "GroomComponent.h"
#include "GroomBindingAsset.h"
#include "Engine/SkeletalMesh.h"
#include "Rendering/SkeletalMeshRenderData.h"

ACustomizationStudioActor::ACustomizationStudioActor()
{
 PrimaryActorTick.bCanEverTick=true;
 RootComponent=CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
 ModelRoot=CreateDefaultSubobject<USceneComponent>(TEXT("ModelRoot"));ModelRoot->SetupAttachment(RootComponent);
 Body=CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Body"));Body->SetupAttachment(ModelRoot);
 Face=CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Face"));Face->SetupAttachment(ModelRoot);
 Outfit=CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Outfit"));Outfit->SetupAttachment(ModelRoot);
 for(auto* M:{Body.Get(),Face.Get(),Outfit.Get()}){M->SetCollisionEnabled(ECollisionEnabled::NoCollision);M->bCastDynamicShadow=true;}
 Camera=CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));Camera->SetupAttachment(RootComponent);Camera->FieldOfView=38;
 for(const TCHAR* Name:{TEXT("Hair"),TEXT("Eyebrows"),TEXT("Eyelashes"),TEXT("Fuzz")}){auto* G=CreateDefaultSubobject<UGroomComponent>(Name);G->SetupAttachment(Face);GroomComponents.Add(G);}
 UpdateCamera();
}
void ACustomizationStudioActor::RequestAppearance(const FCustomizationAppearance& Data)
{
 if(!Data.Validate()){Status=TEXT("잘못된 외형 설정입니다.");return;}
 Desired=Data;++Revision;bQueued=true;Debounce=.12f;
}
void ACustomizationStudioActor::Tick(float Delta)
{Super::Tick(Delta);Debounce-=Delta;if(bQueued&&!bBusy&&Debounce<=0)StartUpdate();}
TArray<FStudioGroomChoice> ACustomizationStudioActor::HairChoices(const FString& Type) const
{
 TArray<FStudioGroomChoice> Out;if(Config)for(auto& H:Config->Grooms)if(H.BodyType==Type&&H.Component==TEXT("Hair")&&H.Groom&&H.Binding&&UGroomBindingAsset::IsCompatible(H.Groom,H.Binding,false))Out.Add(H);return Out;
}
bool ACustomizationStudioActor::IsHairValid(const FString& Type,FName Id) const
{if(Id==TEXT("Bald"))return true;for(auto& H:HairChoices(Type))if(H.Id==Id)return true;return false;}
void ACustomizationStudioActor::StartUpdate()
{
 bQueued=false;if(!Config||!Config->CustomizableObject){Status=TEXT("프리뷰 설정 또는 CO_Ranger가 없습니다.");return;}
 if(!IsHairValid(Desired.BodyType,Desired.HairId)){Status=TEXT("이 외형에 맞는 헤어 바인딩이 없습니다.");return;}
 PendingInstance=Config->CustomizableObject->CreateInstance();if(!PendingInstance){Status=TEXT("Mutable 인스턴스를 만들지 못했습니다.");return;}
 auto* I=PendingInstance.Get();const FCustomizationAppearance Snapshot=Desired;const int32 Request=Revision;
 I->SetIntParameterSelectedOption(TEXT("BodyType"),Snapshot.BodyType);I->SetColorParameterSelectedOption(TEXT("EyeColor"),Snapshot.EyeColor);I->SetColorParameterSelectedOption(TEXT("SkinColor"),Snapshot.SkinColor);I->SetFloatParameterSelectedOption(TEXT("SkinValue"),Snapshot.SkinValue);
 for(int K=0;K<8;++K){const auto& P=FCustomizationAppearance::Parts()[K];I->SetColorParameterSelectedOption(P+TEXT("Color"),Snapshot.Colors[K]);I->SetFloatParameterSelectedOption(P+TEXT("Value"),Snapshot.Values[K]);I->SetFloatParameterSelectedOption(P+TEXT("UseOriginal"),Snapshot.Originals[K]);}
 bBusy=true;Status=TEXT("외형 생성 중…");FInstanceUpdateNativeDelegate Callback;TWeakObjectPtr<ACustomizationStudioActor> Weak(this);
 Callback.AddLambda([Weak,Request,Snapshot](const FUpdateContext& Context){if(!Weak.IsValid())return;auto* Self=Weak.Get();Self->bBusy=false;
  bool Valid=Context.UpdateResult==EUpdateResult::Success||Context.UpdateResult==EUpdateResult::Warning;
  TArray<USkeletalMesh*> Meshes;for(const TCHAR* Name:{TEXT("Body"),TEXT("Face"),TEXT("Outfit")}){auto* M=Self->PendingInstance?Self->PendingInstance->GetSkeletalMeshComponentSkeletalMesh(Name):nullptr;Meshes.Add(M);auto* R=M?M->GetResourceForRendering():nullptr;Valid&=R&&R->LODRenderData.Num()>0;if(R)for(auto& L:R->LODRenderData)Valid&=L.GetNumVertices()>0&&L.RenderSections.Num()>0;}
  if(Request!=Self->Revision){Self->PendingInstance=nullptr;return;}
  if(!Valid){Self->Status=TEXT("외형 생성 실패 — 이전 미리보기를 유지합니다.");Self->PendingInstance=nullptr;return;}
  Self->Body->SetSkeletalMesh(Meshes[0]);Self->Face->SetSkeletalMesh(Meshes[1]);Self->Outfit->SetSkeletalMesh(Meshes[2]);Self->LiveInstance=Self->PendingInstance;Self->PendingInstance=nullptr;Self->ApplyGrooms(Snapshot);Self->AppliedRevision=Request;Self->Status=TEXT("미리보기 준비 완료");
 });I->UpdateSkeletalMeshAsyncResult(Callback,true,true);
}
void ACustomizationStudioActor::ApplyGrooms(const FCustomizationAppearance& Data)
{
 for(UGroomComponent* G:GroomComponents){G->SetVisibility(false);G->SetGroomAsset(nullptr);if(!Config)continue;
  for(auto& H:Config->Grooms){if(H.BodyType!=Data.BodyType||H.Component!=G->GetFName())continue;if(H.Component==TEXT("Hair")&&H.Id!=Data.HairId)continue;
   if(H.Groom&&H.Binding&&UGroomBindingAsset::IsCompatible(H.Groom,H.Binding,false)&&UGroomBindingAsset::IsCompatible(Face->GetSkeletalMeshAsset(),H.Binding,false)){G->SetGroomAsset(H.Groom,H.Binding,true);G->SetVisibility(true);}break;
  }
 }
}
void ACustomizationStudioActor::UpdateCamera(){const float Z=bFaceCamera?155.f:92.f;Camera->SetRelativeLocation(FVector(0,-Distance,Z));Camera->SetRelativeRotation(FRotator(0,90,0));}
void ACustomizationStudioActor::RotatePreview(float Degrees){ModelRoot->AddLocalRotation(FRotator(0,Degrees,0));}
void ACustomizationStudioActor::ZoomPreview(float Amount){Distance=FMath::Clamp(Distance+Amount,65.f,550.f);UpdateCamera();}
void ACustomizationStudioActor::SetFaceView(bool bFace){bFaceCamera=bFace;Distance=bFace?100.f:310.f;UpdateCamera();}
