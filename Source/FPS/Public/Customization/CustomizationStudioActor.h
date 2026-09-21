#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/DataAsset.h"
#include "Customization/CustomizationProfile.h"
#include "Customization/CustomizationPreviewState.h"
#include "CustomizationStudioActor.generated.h"
class UCustomizableObject;
class UCustomizableObjectInstance;
class USkeletalMeshComponent;
class USkeletalMesh;
class UGroomAsset;
class UGroomBindingAsset;
class UGroomComponent;
class UCameraComponent;

USTRUCT(BlueprintType)
struct FStudioGroomChoice
{
 GENERATED_BODY()
 UPROPERTY(EditAnywhere,BlueprintReadWrite) FString BodyType;
 UPROPERTY(EditAnywhere,BlueprintReadWrite) FName Id;
 UPROPERTY(EditAnywhere,BlueprintReadWrite) FString Label;
 UPROPERTY(EditAnywhere,BlueprintReadWrite) TObjectPtr<UGroomAsset> Groom;
 UPROPERTY(EditAnywhere,BlueprintReadWrite) TObjectPtr<UGroomBindingAsset> Binding;
 UPROPERTY(EditAnywhere,BlueprintReadWrite) FName Component = TEXT("Hair");
};
UCLASS(BlueprintType)
class FPS_API UCustomizationStudioConfig : public UDataAsset
{
 GENERATED_BODY()
public:
 UPROPERTY(EditAnywhere,BlueprintReadWrite) TObjectPtr<UCustomizableObject> CustomizableObject;
 UPROPERTY(EditAnywhere,BlueprintReadWrite) TArray<FStudioGroomChoice> Grooms;
};

UCLASS(Blueprintable)
class FPS_API ACustomizationStudioActor : public AActor
{
 GENERATED_BODY()
public:
 ACustomizationStudioActor();
 virtual void BeginPlay() override;
 virtual void Tick(float Delta) override;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Studio") TObjectPtr<UCustomizationStudioConfig> Config;
 UPROPERTY(EditAnywhere,Category="Studio") bool bUseMutableLevelFraming=false;
 // Height in actor-root space. MutableLevel's pedestal top is +1 cm.
 // Used only by the static MutableLevel preview, never gameplay characters.
 UPROPERTY(EditAnywhere,Category="Studio") float PreviewFloorZ=1.f;
 // Bind grooms to a non-rendered original face in this static preview.
 // Mutable rebuilds vertex order, so authored bindings cannot target its output.
 UPROPERTY(EditAnywhere,Category="Studio") bool bUseReferencePoseGrooms=false;
 // 0 = automatic, 1 = highest detail. Reapplied after each Mutable generation.
 UPROPERTY(EditAnywhere,Category="Studio",meta=(ClampMin="0")) int32 PreviewFaceLOD=0;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Studio") TObjectPtr<USkeletalMeshComponent> Body;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Studio") TObjectPtr<USkeletalMeshComponent> Face;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Studio") TObjectPtr<USkeletalMeshComponent> Outfit;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Studio") TObjectPtr<UCameraComponent> Camera;
 UFUNCTION(BlueprintCallable) void RequestAppearance(const FCustomizationAppearance& Data);
 UFUNCTION(BlueprintCallable) void RotatePreview(float Degrees);
 UFUNCTION(BlueprintCallable) void ZoomPreview(float Amount);
 UFUNCTION(BlueprintCallable) void SetFaceView(bool bFace);
 bool IsUpdating() const {return bBusy||bQueued;}
 bool IsAppearanceReady() const {return IsPreviewRevisionReady(Revision,AppliedRevision,IsUpdating());}
 FString GetStatus() const {return Status;}
 TArray<FStudioGroomChoice> HairChoices(const FString& BodyType) const;
 bool IsHairValid(const FString& BodyType,FName Id) const;
 int32 GetAppliedRevision() const {return AppliedRevision;}
private:
 UPROPERTY() TObjectPtr<USceneComponent> ModelRoot;
 UPROPERTY() TArray<TObjectPtr<UGroomComponent>> GroomComponents;
 UPROPERTY() TObjectPtr<USkeletalMeshComponent> GroomBindingSurface;
 UPROPERTY(Transient) TObjectPtr<UCustomizableObjectInstance> LiveInstance;
 UPROPERTY(Transient) TObjectPtr<UCustomizableObjectInstance> PendingInstance;
 FCustomizationAppearance Desired;
 int32 Revision=0,AppliedRevision=0;
 bool bBusy=false,bQueued=false,bFaceCamera=false;
 float Debounce=0,Distance=310;
 FString Status;
 void StartUpdate();
 void ApplyGrooms(const FCustomizationAppearance& Data);
 bool ResolveGroundedPreview(const TArray<USkeletalMesh*>& Meshes,FTransform& OutTransform) const;
 void UpdateCamera();
};
