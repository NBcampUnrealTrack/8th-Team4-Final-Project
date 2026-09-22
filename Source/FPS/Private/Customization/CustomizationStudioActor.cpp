#include "Customization/CustomizationStudioActor.h"
#include "Customization/CustomizationPreviewGrounding.h"
#include "MuCO/CustomizableObject.h"
#include "MuCO/CustomizableObjectInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Camera/CameraComponent.h"
#include "GroomComponent.h"
#include "GroomBindingAsset.h"
#include "GroomAsset.h"
#include "Engine/SkeletalMesh.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Materials/MaterialInstanceDynamic.h"

ACustomizationStudioActor::ACustomizationStudioActor()
{
 PrimaryActorTick.bCanEverTick=true;
 RootComponent=CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
 ModelRoot=CreateDefaultSubobject<USceneComponent>(TEXT("ModelRoot"));ModelRoot->SetupAttachment(RootComponent);
 Body=CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Body"));Body->SetupAttachment(ModelRoot);
 Face=CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Face"));Face->SetupAttachment(ModelRoot);
 Outfit=CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Outfit"));Outfit->SetupAttachment(ModelRoot);
 GroomBindingSurface=CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("GroomBindingSurface"));GroomBindingSurface->SetupAttachment(ModelRoot);
 GroomBindingSurface->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 GroomBindingSurface->SetRenderInMainPass(false);GroomBindingSurface->SetRenderInDepthPass(false);GroomBindingSurface->SetCastShadow(false);
 GroomBindingSurface->bVisibleInRayTracing=false;
 GroomBindingSurface->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
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
void ACustomizationStudioActor::BeginPlay()
{
 Super::BeginPlay();
 if(bUseMutableLevelFraming)SetFaceView(false);
}
void ACustomizationStudioActor::Tick(float Delta)
{Super::Tick(Delta);Debounce-=Delta;if(bQueued&&!bBusy&&Debounce<=0)StartUpdate();}
TArray<FStudioGroomChoice> ACustomizationStudioActor::HairChoices(const FString& Type) const
{
 TArray<FStudioGroomChoice> Out;if(Config)for(auto& H:Config->Grooms)if(H.BodyType==Type&&H.Component==TEXT("Hair")&&H.Groom&&H.Binding&&UGroomBindingAsset::IsCompatible(H.Groom,H.Binding,false))Out.Add(H);
 const TArray<FString> Order={TEXT("Hair_S_UpdoBuns"),TEXT("Hair_M_BobStraight"),TEXT("Hair_M_SlickPonytail")};
 Out.StableSort([&Order](const FStudioGroomChoice& A,const FStudioGroomChoice& B){
  auto Rank=[&Order](const FStudioGroomChoice& H){const int32 Index=Order.IndexOfByKey(H.Groom->GetName());return Index==INDEX_NONE?MAX_int32:Index;};
  return Rank(A)<Rank(B);
 });return Out;
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
  FTransform GroundedTransform=Self->ModelRoot->GetRelativeTransform();
  if(Self->bUseMutableLevelFraming&&!Self->ResolveGroundedPreview(Meshes,GroundedTransform))
  {Self->Status=TEXT("발 위치 확인 실패 — 이전 미리보기를 유지합니다.");Self->PendingInstance=nullptr;return;}
  Self->Body->SetSkeletalMesh(Meshes[0]);Self->Face->SetSkeletalMesh(Meshes[1]);Self->Face->SetForcedLOD(Self->PreviewFaceLOD);Self->Outfit->SetSkeletalMesh(Meshes[2]);Self->LiveInstance=Self->PendingInstance;Self->PendingInstance=nullptr;Self->ApplyGrooms(Snapshot);Self->AppliedRevision=Request;Self->Status=TEXT("미리보기 준비 완료");
  if(Self->bUseMutableLevelFraming)Self->ModelRoot->SetRelativeTransform(GroundedTransform);
 });I->UpdateSkeletalMeshAsyncResult(Callback,true,true);
}
bool ACustomizationStudioActor::ResolveGroundedPreview(const TArray<USkeletalMesh*>& Meshes,FTransform& OutTransform) const
{
 // These components show a static reference pose. Imported mesh bounds may be
 // padded or inherited from a different body, so measure the generated vertices.
 // Include the outfit because its boots can extend below the body's bare feet.
 TArray<FVector> Vertices;
 for(const int32 MeshIndex:{0,2})
 {
  const auto* RenderData=Meshes.IsValidIndex(MeshIndex)&&Meshes[MeshIndex]?Meshes[MeshIndex]->GetResourceForRendering():nullptr;
  if(!RenderData||RenderData->LODRenderData.IsEmpty())return false;
  const auto& Positions=RenderData->LODRenderData[0].StaticVertexBuffers.PositionVertexBuffer;
  // CPU data may be stripped in cooked content. Never access a released buffer
  // or guess from bounds; retain the preceding valid preview in that case.
  if(!Positions.GetVertexData()||Positions.GetNumVertices()==0)return false;
  const FTransform ComponentToModel=(MeshIndex==0?Body.Get():Outfit.Get())->GetRelativeTransform();
  Vertices.Reserve(Vertices.Num()+Positions.GetNumVertices());
  for(uint32 Index=0;Index<Positions.GetNumVertices();++Index)
   Vertices.Add(ComponentToModel.TransformPosition(FVector(Positions.VertexPosition(Index))));
 }
 return CalculatePreviewGroundedTransform(Vertices,ModelRoot->GetRelativeTransform(),PreviewFloorZ,OutTransform);
}
void ACustomizationStudioActor::ApplyGrooms(const FCustomizationAppearance& Data)
{
 // The authored target retains the vertex layout AND the head shape used to fit
 // cross-body hairstyles. It supplies deformation only, never a second visible face.
 USkeletalMesh* BindingTarget=nullptr;
 if(bUseReferencePoseGrooms&&Config)for(const auto& H:Config->Grooms)
  if(H.BodyType==Data.BodyType&&H.Binding){BindingTarget=H.Binding->GetTargetSkeletalMesh();if(BindingTarget)break;}
 GroomBindingSurface->SetSkeletalMesh(BindingTarget);
 GroomBindingSurface->SetForcedLOD(1);
 GroomBindingSurface->SetRelativeTransform(Face->GetRelativeTransform());
 for(UGroomComponent* G:GroomComponents){G->SetVisibility(false);G->SetGroomAsset(nullptr);
  if(G->GetFName()==TEXT("Hair"))G->EmptyOverrideMaterials();
  if(!Config)continue;
  for(auto& H:Config->Grooms){if(H.BodyType!=Data.BodyType||H.Component!=G->GetFName())continue;if(H.Component==TEXT("Hair")&&H.Id!=Data.HairId)continue;
   auto* Surface=bUseReferencePoseGrooms?GroomBindingSurface.Get():Face.Get();
   if(H.Groom&&H.Binding&&Surface->GetSkeletalMeshAsset()&&UGroomBindingAsset::IsCompatible(H.Groom,H.Binding,false)&&UGroomBindingAsset::IsCompatible(Surface->GetSkeletalMeshAsset(),H.Binding,false))
   {
    G->AttachToComponent(Surface,FAttachmentTransformRules::KeepRelativeTransform);
    G->SetGroomAsset(H.Groom,H.Binding,true);G->SetVisibility(true);
    if(H.Component==TEXT("Hair")&&!Data.bUseOriginalHairColor)
    {
     // Keep each authored cards/strands/helmet shader; tint only this component.
     const auto& Materials=H.Groom->GetHairGroupsMaterials();
     for(int32 Slot=0;Slot<Materials.Num();++Slot)if(Materials[Slot].Material)
     {
      auto* Dye=UMaterialInstanceDynamic::Create(Materials[Slot].Material,G);
      Dye->SetVectorParameterValue(TEXT("hairDye"),Data.HairColor);
      // Neutralize the dark melanin base so light colors are selectable as well.
      for(const TCHAR* Parameter:{TEXT("hairMelanin"),TEXT("hairRedness"),TEXT("MelaninVariationFine"),TEXT("MelaninVariationRough"),TEXT("RedVariation"),TEXT("Ombre"),TEXT("Highlights"),TEXT("Region"),TEXT("WhiteAmount")})
       Dye->SetScalarParameterValue(Parameter,0.f);
      G->SetMaterial(Slot,Dye);
     }
    }
   }break;
  }
 }
}
void ACustomizationStudioActor::UpdateCamera(){const float Z=bFaceCamera?155.f:92.f;Camera->SetRelativeLocation(FVector(bUseMutableLevelFraming?-Distance*.13f:0.f,-Distance,Z));Camera->SetRelativeRotation(FRotator(0,90,0));}
void ACustomizationStudioActor::RotatePreview(float Degrees){ModelRoot->AddLocalRotation(FRotator(0,Degrees,0));}
void ACustomizationStudioActor::ZoomPreview(float Amount){Distance=FMath::Clamp(Distance+Amount,65.f,bUseMutableLevelFraming?800.f:550.f);UpdateCamera();}
void ACustomizationStudioActor::SetFaceView(bool bFace){bFaceCamera=bFace;Distance=bFace?(bUseMutableLevelFraming?150.f:100.f):(bUseMutableLevelFraming?560.f:310.f);UpdateCamera();}
