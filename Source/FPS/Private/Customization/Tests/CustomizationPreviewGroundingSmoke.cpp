#if WITH_EDITOR
#include "HAL/IConsoleManager.h"
#include "Customization/CustomizationStudioActor.h"
#include "Customization/CustomizationStudioController.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "TimerManager.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

// Opt-in runtime verification; no asset or appearance-save writes.
static FAutoConsoleCommandWithWorld PreviewGroundingProbe(TEXT("FPS.MutableLevel.GroundingProbe"),
    TEXT("Verify all three body soles and repeat generation in the live preview"),
    FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
    {
        if (!World || !World->GetMapName().Contains(TEXT("MutableLevel"))) return;
        struct FProbe
        {
            FTimerHandle Timer;
            FCustomizationAppearance Original;
            FString OriginalStatus;
            bool bOriginalDirty = false;
            bool bCapturedOriginal = false;
            bool bRestoring = false;
            bool bFailed = false;
            int32 Step = -1;
            int32 Seconds = 0;
            FTransform InitialRoot;
        };
        auto Probe = MakeShared<FProbe>();
        TWeakObjectPtr<UWorld> Weak(World);
        World->GetTimerManager().SetTimer(Probe->Timer, [Weak, Probe]()
        {
            auto* W = Weak.Get();
            if (!W) return;
            auto* Controller = Cast<ACustomizationStudioController>(W->GetFirstPlayerController());
            auto* Studio = Controller ? Controller->Studio.Get() : nullptr;
            auto Restore = [Controller, Studio, Probe]()
            {
                if (!Probe->bCapturedOriginal || !Controller || !Studio) return false;
                Controller->Draft = Probe->Original;
                Controller->bDirty = Probe->bOriginalDirty;
                Controller->Status = Probe->OriginalStatus;
                Studio->RequestAppearance(Probe->Original);
                Probe->bRestoring = true;
                Probe->Seconds = 0;
                return true;
            };
            auto Fail = [W, Probe, Restore](const TCHAR* Reason)
            {
                UE_LOG(LogTemp, Error, TEXT("PREVIEW_GROUNDING_FAILED %s"), Reason);
                Probe->bFailed = true;
                if (Probe->bRestoring || !Restore()) W->GetTimerManager().ClearTimer(Probe->Timer);
            };
            if (++Probe->Seconds > 180) { Fail(TEXT("timeout")); return; }
            if (!Studio || !Studio->IsAppearanceReady()) return;
            if (Probe->bRestoring)
            {
                UE_LOG(LogTemp, Display, TEXT("PREVIEW_GROUNDING_%s restored original appearance and editor state"), Probe->bFailed ? TEXT("RESTORED_AFTER_FAILURE") : TEXT("PASSED"));
                W->GetTimerManager().ClearTimer(Probe->Timer);
                return;
            }
            const TCHAR* Types[] = { TEXT("Seo"), TEXT("Custom"), TEXT("Fab"), TEXT("Seo") };
            if (Probe->Step == -1)
            {
                if (!Studio->bUseMutableLevelFraming) { Fail(TEXT("framing disabled")); return; }
                Probe->Original = Controller->Draft;
                Probe->OriginalStatus = Controller->Status;
                Probe->bOriginalDirty = Controller->bDirty;
                Probe->bCapturedOriginal = true;
                Probe->InitialRoot = Studio->Body->GetAttachParent()->GetRelativeTransform();
            }
            else if (Probe->Step < 4)
            {
                double MinZ = TNumericLimits<double>::Max();
                for (const auto* Component : { Studio->Body.Get(), Studio->Outfit.Get() })
                {
                    auto* Mesh = Component->GetSkeletalMeshAsset();
                    const auto* RenderData = Mesh ? Mesh->GetResourceForRendering() : nullptr;
                    if (!RenderData || RenderData->LODRenderData.IsEmpty()) { Fail(TEXT("missing render data")); return; }
                    const auto& Positions = RenderData->LODRenderData[0].StaticVertexBuffers.PositionVertexBuffer;
                    if (!Positions.GetVertexData()) { Fail(TEXT("missing CPU positions")); return; }
                    const FTransform ToRoot = Component->GetComponentTransform().GetRelativeTransform(Studio->GetRootComponent()->GetComponentTransform());
                    for (uint32 Vertex = 0; Vertex < Positions.GetNumVertices(); ++Vertex)
                        MinZ = FMath::Min(MinZ, ToRoot.TransformPosition(FVector(Positions.VertexPosition(Vertex))).Z);
                }
                const FTransform Root = Studio->Body->GetAttachParent()->GetRelativeTransform();
                if (!Root.GetScale3D().Equals(Probe->InitialRoot.GetScale3D()) || !Root.GetRotation().Equals(Probe->InitialRoot.GetRotation()))
                { Fail(TEXT("proportions or rotation changed")); return; }
                UE_LOG(LogTemp, Display, TEXT("PREVIEW_GROUNDING body=%s soleRootZ=%.6f modelZ=%.6f"), Types[Probe->Step], MinZ, Root.GetTranslation().Z);
                if (!FMath::IsNearlyEqual(MinZ, static_cast<double>(Studio->PreviewFloorZ), 0.01)) { Fail(TEXT("sole not on configured floor plane")); return; }
                FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("Screenshots") /
                    FString::Printf(TEXT("PreviewGrounding_%d_%s.png"), Probe->Step, Types[Probe->Step]), true, false);
            }
            ++Probe->Step;
            if (Probe->Step == 4) { Restore(); return; }
            Controller->Draft.BodyType = Types[Probe->Step];
            Controller->Draft.HairId = TEXT("Bald");
            Studio->RequestAppearance(Controller->Draft);
        }, 1.f, true);
    }));
#endif
