#include "Customization/CustomizationStudioActor.h"
#include "Customization/AppearanceParameters.h"
#include "Customization/CustomizationPreviewGrounding.h"
#include "Customization/AppearanceVisualSafety.h"
#include "Customization/CustomizationClassRules.h"
#include "HAL/PlatformProperties.h"
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
 for(const TCHAR* Name:{TEXT("Hair"),TEXT("Eyebrows"),TEXT("Eyelashes"),TEXT("Fuzz"),TEXT("Mustache"),TEXT("Beard")}){auto* G=CreateDefaultSubobject<UGroomComponent>(Name);G->SetupAttachment(Face);GroomComponents.Add(G);}
 UpdateCamera();
}
void ACustomizationStudioActor::RequestAppearance(const FCustomizationAppearance& Data)
{
 FCustomizationAppearance Normalized=Data;
 if(!Config||!Normalized.NormalizeBodyForClass(Config->ClassId)||!IsBodyValid(Normalized.BodyType)){++Revision;bQueued=false;Status=TEXT("잘못된 외형 설정입니다.");return;}
 Desired=Normalized;++Revision;bQueued=true;Debounce=.12f;
}
void ACustomizationStudioActor::BeginPlay()
{
 Super::BeginPlay();
 if(bUseMutableLevelFraming)SetFaceView(false);
}
void ACustomizationStudioActor::Tick(float Delta)
{Super::Tick(Delta);Debounce-=Delta;if(bQueued&&!bBusy&&Debounce<=0)StartUpdate();}
TArray<FStudioGroomChoice> ACustomizationStudioActor::HairChoices(const FString& Type) const
{return HairChoicesForConfig(Config,Type);}
TArray<FStudioGroomChoice> ACustomizationStudioActor::HairChoicesForConfig(const UCustomizationStudioConfig* Config,const FString& Type)
{
 TArray<FStudioGroomChoice> Out;if(Config)for(auto& H:Config->Grooms)if(H.BodyType==Type&&H.Component==TEXT("Hair")&&H.Groom&&H.Binding&&UGroomBindingAsset::IsCompatible(H.Groom,H.Binding,false))Out.Add(H);
 const TArray<FString> Order={TEXT("Hair_S_UpdoBuns"),TEXT("Hair_M_BobStraight"),TEXT("Hair_M_SlickPonytail")};
 Out.StableSort([&Order](const FStudioGroomChoice& A,const FStudioGroomChoice& B){
  auto Rank=[&Order](const FStudioGroomChoice& H){const int32 Index=Order.IndexOfByKey(H.Groom->GetName());return Index==INDEX_NONE?MAX_int32:Index;};
  return Rank(A)<Rank(B);
 });return Out;
}
bool ACustomizationStudioActor::IsHairValid(const FString& Type,FName Id) const
{if(!IsBodyValid(Type))return false;if(Id==TEXT("Bald"))return true;for(auto& H:HairChoices(Type))if(H.Id==Id)return true;return false;}
bool ACustomizationStudioActor::IsBodyValid(const FString& Type) const
{return Config&&Type==FixedCustomizationBodyForClass(Config->ClassId)&&Config->BodyTypes.Contains(Type);}
bool ACustomizationStudioActor::SetClassConfig(UCustomizationStudioConfig* NewConfig)
{
 if(!NewConfig||!NewConfig->CustomizableObject||!NewConfig->BodyTypes.Contains(NewConfig->DefaultBodyType))return false;
 if(NewConfig->DefaultBodyType!=FixedCustomizationBodyForClass(NewConfig->ClassId))return false;
 if(Config==NewConfig)return true;
 Config=NewConfig;++Revision;bQueued=false;
 for(auto* M:{Body.Get(),Face.Get(),Outfit.Get()})M->SetVisibility(false);
 for(UGroomComponent* G:GroomComponents)G->SetVisibility(false);
 Status=TEXT("클래스 미리보기 준비 중…");return true;
}
void ACustomizationStudioActor::StartUpdate()
{
 bQueued=false;if(!Config||!Config->CustomizableObject){Status=TEXT("프리뷰 설정 또는 CO_Ranger가 없습니다.");return;}
 if(!IsHairValid(Desired.BodyType,Desired.HairId)){Status=TEXT("이 외형에 맞는 헤어 바인딩이 없습니다.");return;}
#if WITH_EDITOR
 // Standalone editor play does not automatically compile every class CO.
 // Prepare a newly selected object before creating its parameter descriptor.
 if(!Config->CustomizableObject->IsCompiled())
 {FCompileParams Params;Params.bAsync=false;Params.bSkipIfNotOutOfDate=false;Config->CustomizableObject->Compile(Params);}
#endif
 if(!Config->CustomizableObject->IsCompiled()){Status=TEXT("클래스 CO가 준비되지 않았습니다. 컴파일 상태를 확인하세요.");return;}
 PendingInstance=Config->CustomizableObject->CreateInstance();if(!PendingInstance){Status=TEXT("Mutable 인스턴스를 만들지 못했습니다.");return;}
 auto* I=PendingInstance.Get();const FCustomizationAppearance Snapshot=Desired;const int32 Request=Revision;
 if(!ApplyAppearanceParameters(I,Config,Snapshot)){PendingInstance=nullptr;Status=TEXT("잘못된 외형 설정입니다.");return;}
 bBusy=true;Status=TEXT("외형 생성 중…");FInstanceUpdateNativeDelegate Callback;TWeakObjectPtr<ACustomizationStudioActor> Weak(this);
 TWeakObjectPtr<UCustomizationStudioConfig> RequestConfig(Config);
 Callback.AddLambda([Weak,Request,Snapshot,I,RequestConfig](const FUpdateContext& Context){if(!Weak.IsValid())return;auto* Self=Weak.Get();Self->bBusy=false;
  bool Valid=Context.UpdateResult==EUpdateResult::Success||Context.UpdateResult==EUpdateResult::Warning;
  if(Request!=Self->Revision||RequestConfig.Get()!=Self->Config){Self->PendingInstance=nullptr;return;}
  TArray<USkeletalMesh*> Meshes;for(const TCHAR* Name:{TEXT("Body"),TEXT("Face"),TEXT("Outfit")}){auto* M=I->GetSkeletalMeshComponentSkeletalMesh(Name);Meshes.Add(M);Valid&=AppearanceVisualSafety::HasRenderableGeometry(M);}
  if(!Valid){Self->Status=TEXT("외형 생성 실패 — 이전 미리보기를 유지합니다.");Self->PendingInstance=nullptr;return;}
  FTransform GroundedTransform=Self->ModelRoot->GetRelativeTransform();
  // Floor placement is optional: cooked meshes may have no CPU-readable vertices.
  // Keep the existing placement without blocking valid mesh/material/color updates.
  const bool bGroundingResolved=!Self->bUseMutableLevelFraming||Self->ResolveGroundedPreview(Meshes,GroundedTransform);
  const TArray<USkeletalMeshComponent*> Components={Self->Body.Get(),Self->Face.Get(),Self->Outfit.Get()};
  // This studio displays reference poses. The generated face inherits the CO
  // reference face's post-process setup, which can deform a different identity.
  // Disable it only for the static reference-pose preview, never on source assets.
  Self->Face->SetDisablePostProcessBlueprint(Self->bUseReferencePoseGrooms);
  const FName ComponentNames[]={TEXT("Body"),TEXT("Face"),TEXT("Outfit")};
  for(int32 Index=0;Index<Components.Num();++Index)
  {
   auto* Component=Components[Index];
   // MetaHuman post-process animation creates component material overrides.
   // Their slot order belongs to the previous face, not the next class/body.
   Component->EmptyOverrideMaterials();
   Component->SetSkeletalMesh(Meshes[Index]);
   // Cached Mutable meshes can retain older asset materials: use this instance's
   // generated materials instead, without modifying the shared mesh asset.
   const auto Materials=I->GetSkeletalMeshComponentOverrideMaterials(ComponentNames[Index]);
   for(int32 Slot=0;Slot<Materials.Num();++Slot)Component->SetMaterial(Slot,Materials[Slot]);
  }
  Self->Face->SetForcedLOD(Self->PreviewFaceLOD);Self->LiveInstance=Self->PendingInstance;Self->PendingInstance=nullptr;Self->ApplyGrooms(Snapshot);Self->AppliedRevision=Request;Self->Status=TEXT("미리보기 준비 완료");
  if(!bGroundingResolved)Self->Status=TEXT("미리보기 준비 완료 · 발 높이는 기존 배치 유지");
  for(auto* M:{Self->Body.Get(),Self->Face.Get(),Self->Outfit.Get()})M->SetVisibility(true);
  if(Self->bUseMutableLevelFraming&&bGroundingResolved)Self->ModelRoot->SetRelativeTransform(GroundedTransform);
  Self->UpdateCamera();
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
  // or guess from bounds; retain only the preceding floor placement in that case.
  if(!CanReadPreviewVertices(FPlatformProperties::RequiresCookedData(), Positions.GetAllowCPUAccess(),
      Positions.GetVertexData()!=nullptr&&Positions.GetNumVertices()>0))return false;
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
    // MutableLevel was authored with cards forced for Ranger. Do not carry
    // that override into strand-only styles (for example Frey's Middle_part).
    // Keep the existing card preview where the selected asset supplies cards.
    const bool bHasCards=H.Groom->GetHairGroupsCards().ContainsByPredicate([](const FHairGroupsCardsSourceDescription& Cards){return Cards.GetMesh()!=nullptr&&Cards.LODIndex>=0;});
    G->SetUseCards(bHasCards);
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
void ACustomizationStudioActor::UpdateCamera()
{
 FVector Focus(0,0,bFaceCamera?155.f:92.f);
 if(bFaceCamera&&Face&&Face->GetSkeletalMeshAsset())
 {
  // Use the displayed identity's eye landmarks, including grounding and rotation.
  // Face bounds include the neck/chest and are not a reliable facial center.
  if(Face->DoesSocketExist(TEXT("FACIAL_L_Eye"))&&Face->DoesSocketExist(TEXT("FACIAL_R_Eye")))
   Focus=GetActorTransform().InverseTransformPosition((Face->GetSocketLocation(TEXT("FACIAL_L_Eye"))+Face->GetSocketLocation(TEXT("FACIAL_R_Eye")))*.5)-FVector(0,0,2);
  else if(Face->DoesSocketExist(TEXT("head")))
   Focus=GetActorTransform().InverseTransformPosition(Face->GetSocketLocation(TEXT("head")))+FVector(0,0,10);
 }
 Camera->SetRelativeLocation(Focus+FVector((bUseMutableLevelFraming?-Distance*.13f:0.f)+CameraPan.X,-Distance,CameraPan.Y));
 Camera->SetRelativeRotation(FRotator(0,90,0));
}
void ACustomizationStudioActor::RotatePreview(float Degrees){ModelRoot->AddLocalRotation(FRotator(0,Degrees,0));UpdateCamera();}
void ACustomizationStudioActor::ZoomPreview(float Amount){Distance=FMath::Clamp(Distance+Amount,65.f,bUseMutableLevelFraming?800.f:550.f);UpdateCamera();}
void ACustomizationStudioActor::PanPreview(FVector2D Delta)
{
 if(!FMath::IsFinite(Delta.X)||!FMath::IsFinite(Delta.Y))return;
 CameraPan+=Delta*(Distance*.7f);
 CameraPan.X=FMath::Clamp(CameraPan.X,-250.,250.);CameraPan.Y=FMath::Clamp(CameraPan.Y,-250.,250.);
 UpdateCamera();
}
void ACustomizationStudioActor::SetFaceView(bool bFace){bFaceCamera=bFace;CameraPan=FVector2D::ZeroVector;Distance=bFace?(bUseMutableLevelFraming?150.f:100.f):(bUseMutableLevelFraming?700.f:310.f);UpdateCamera();}
