#include "Minimap/ShooterMinimapBounds.h"

#include "Components/BoxComponent.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Minimap/ShooterMinimapSubsystem.h"

#if WITH_EDITOR
#include "Async/ParallelFor.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "ImageUtils.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopedSlowTask.h"
#include "TextureResource.h"

namespace ShooterMinimapBoundsEditor
{
	void SavePng(const UWorld& World, const TArray<FColor>& Pixels, const int32 Resolution, const TCHAR* Suffix)
	{
		TArray64<uint8> PngData;
		FImageUtils::PNGCompressImageArray(Resolution, Resolution, Pixels, PngData);

		const FString FilePath = FPaths::ProjectSavedDir() / TEXT("Minimap") /
			FString::Printf(TEXT("%s_%s.png"), *World.GetMapName(), Suffix);
		if (!FFileHelper::SaveArrayToFile(PngData, *FilePath))
		{
			UE_LOG(LogTemp, Error, TEXT("[ShooterMinimapBounds] PNG 저장 실패: %s"), *FilePath);
			return;
		}

		UE_LOG(LogTemp, Log, TEXT("[ShooterMinimapBounds] PNG 저장: %s"), *FPaths::ConvertRelativePathToFull(FilePath));
	}

	// Out[i] = min(In[i-Radius .. i+Radius]). 단조 덱으로 반경과 무관하게 O(Count).
	void SlidingMin(const float* In, float* Out, const int32 Count, const int32 Stride, const int32 Radius)
	{
		TArray<int32> Deque;
		Deque.SetNumUninitialized(Count);
		int32 Head = 0;
		int32 Tail = 0;
		int32 Next = 0;

		for (int32 Index = 0; Index < Count; ++Index)
		{
			// 창의 오른쪽 끝까지 밀어 넣는다.
			const int32 WindowEnd = FMath::Min(Count - 1, Index + Radius);
			for (; Next <= WindowEnd; ++Next)
			{
				while (Tail > Head && In[Deque[Tail - 1] * Stride] >= In[Next * Stride])
				{
					--Tail;
				}
				Deque[Tail++] = Next;
			}

			// 창 왼쪽 밖으로 나간 값을 버린다.
			while (Deque[Head] < Index - Radius)
			{
				++Head;
			}

			Out[Index * Stride] = In[Deque[Head] * Stride];
		}
	}

	// 닫힌 윤곽선 하나. 작업 해상도 픽셀 좌표(픽셀 k는 [k, k+1] 구간, 중심 k+0.5). 첫 점을 끝에 반복하지 않는다.
	using FContour = TArray<FVector2f>;

	// 0/1 마스크의 경계를 marching squares로 닫힌 윤곽선들로 뽑는다.
	// 바깥을 0으로 한 칸 패딩해서 맵 가장자리에 닿은 영역도 반드시 닫힌다.
	// 3x3 박스 블러를 먼저 걸어 대각선 경계가 계단이 아니라 소수점 좌표로 나오게 한다.
	void ExtractContours(const TArray<uint8>& Mask, const int32 Resolution, TArray<FContour>& OutContours)
	{
		const int32 Padded = Resolution + 2;
		TArray<float> Field;
		TArray<float> Temp;
		Field.SetNumZeroed(Padded * Padded);
		Temp.SetNumZeroed(Padded * Padded);

		for (int32 Y = 0; Y < Resolution; ++Y)
		{
			for (int32 X = 0; X < Resolution; ++X)
			{
				Field[(Y + 1) * Padded + (X + 1)] = Mask[Y * Resolution + X] ? 1.f : 0.f;
			}
		}

		ParallelFor(Padded, [&](const int32 Y)
		{
			for (int32 X = 0; X < Padded; ++X)
			{
				const float Left = X > 0 ? Field[Y * Padded + X - 1] : 0.f;
				const float Right = X < Padded - 1 ? Field[Y * Padded + X + 1] : 0.f;
				Temp[Y * Padded + X] = (Left + Field[Y * Padded + X] + Right) / 3.f;
			}
		});
		ParallelFor(Padded, [&](const int32 Y)
		{
			for (int32 X = 0; X < Padded; ++X)
			{
				const float Up = Y > 0 ? Temp[(Y - 1) * Padded + X] : 0.f;
				const float Down = Y < Padded - 1 ? Temp[(Y + 1) * Padded + X] : 0.f;
				Field[Y * Padded + X] = (Up + Temp[Y * Padded + X] + Down) / 3.f;
			}
		});
		Temp.Empty();

		constexpr float Iso = 0.5f;
		auto Value = [&](const int32 I, const int32 J) { return Field[J * Padded + I]; };
		auto HorizontalEdge = [Padded](const int32 I, const int32 J) { return (J * Padded + I) * 2; };
		auto VerticalEdge = [Padded](const int32 I, const int32 J) { return (J * Padded + I) * 2 + 1; };

		// 격자 변마다 교차점 하나. 셀 두 개가 같은 변을 공유하므로 변 ID로 이어 붙인다.
		TMap<int32, FVector2f> EdgePoints;
		TMap<int32, FIntPoint> EdgeLinks;

		auto AddHorizontalPoint = [&](const int32 I, const int32 J)
		{
			const int32 EdgeId = HorizontalEdge(I, J);
			if (!EdgePoints.Contains(EdgeId))
			{
				const float A = Value(I, J);
				const float B = Value(I + 1, J);
				const float T = FMath::Clamp((Iso - A) / (B - A), 0.f, 1.f);
				// 패딩 인덱스 I는 픽셀 I-1의 중심(I-0.5)
				EdgePoints.Add(EdgeId, FVector2f(I + T - 0.5f, J - 0.5f));
			}
			return EdgeId;
		};
		auto AddVerticalPoint = [&](const int32 I, const int32 J)
		{
			const int32 EdgeId = VerticalEdge(I, J);
			if (!EdgePoints.Contains(EdgeId))
			{
				const float A = Value(I, J);
				const float B = Value(I, J + 1);
				const float T = FMath::Clamp((Iso - A) / (B - A), 0.f, 1.f);
				EdgePoints.Add(EdgeId, FVector2f(I - 0.5f, J + T - 0.5f));
			}
			return EdgeId;
		};
		auto Link = [&](const int32 A, const int32 B)
		{
			for (const TPair<int32, int32>& Pair : { TPair<int32, int32>(A, B), TPair<int32, int32>(B, A) })
			{
				FIntPoint& Slots = EdgeLinks.FindOrAdd(Pair.Key, FIntPoint(INDEX_NONE, INDEX_NONE));
				if (Slots.X == INDEX_NONE)
				{
					Slots.X = Pair.Value;
				}
				else
				{
					Slots.Y = Pair.Value;
				}
			}
		};

		for (int32 J = 0; J < Padded - 1; ++J)
		{
			for (int32 I = 0; I < Padded - 1; ++I)
			{
				// 코너: 0=좌상 1=우상 2=우하 3=좌하 / 변: 0=위 1=오른쪽 2=아래 3=왼쪽
				const bool bIn0 = Value(I, J) > Iso;
				const bool bIn1 = Value(I + 1, J) > Iso;
				const bool bIn2 = Value(I + 1, J + 1) > Iso;
				const bool bIn3 = Value(I, J + 1) > Iso;
				const int32 Case = (bIn0 ? 1 : 0) | (bIn1 ? 2 : 0) | (bIn2 ? 4 : 0) | (bIn3 ? 8 : 0);
				if (Case == 0 || Case == 15)
				{
					continue;
				}

				const bool bCross[4] = { bIn0 != bIn1, bIn1 != bIn2, bIn3 != bIn2, bIn0 != bIn3 };
				int32 Edges[4] = { INDEX_NONE, INDEX_NONE, INDEX_NONE, INDEX_NONE };
				if (bCross[0]) { Edges[0] = AddHorizontalPoint(I, J); }
				if (bCross[1]) { Edges[1] = AddVerticalPoint(I + 1, J); }
				if (bCross[2]) { Edges[2] = AddHorizontalPoint(I, J + 1); }
				if (bCross[3]) { Edges[3] = AddVerticalPoint(I, J); }

				if (Case == 5 || Case == 10)
				{
					// 대각선 두 코너만 안쪽인 안장점. 셀 중심값으로 어느 쪽이 이어져 있는지 정한다.
					const float Center = (Value(I, J) + Value(I + 1, J) + Value(I + 1, J + 1) + Value(I, J + 1)) * 0.25f;
					const bool bCutCorners1And3 = (Case == 5) == (Center > Iso);
					if (bCutCorners1And3)
					{
						Link(Edges[0], Edges[1]);
						Link(Edges[2], Edges[3]);
					}
					else
					{
						Link(Edges[3], Edges[0]);
						Link(Edges[1], Edges[2]);
					}
					continue;
				}

				int32 First = INDEX_NONE;
				for (const int32 EdgeId : Edges)
				{
					if (EdgeId == INDEX_NONE)
					{
						continue;
					}
					if (First == INDEX_NONE)
					{
						First = EdgeId;
					}
					else
					{
						Link(First, EdgeId);
						break;
					}
				}
			}
		}

		// 변 ID 링크를 따라 걸으며 닫힌 고리로 묶는다.
		TSet<int32> Visited;
		Visited.Reserve(EdgeLinks.Num());
		for (const TPair<int32, FIntPoint>& Pair : EdgeLinks)
		{
			if (Visited.Contains(Pair.Key))
			{
				continue;
			}

			FContour& Contour = OutContours.AddDefaulted_GetRef();
			int32 Previous = INDEX_NONE;
			int32 Current = Pair.Key;
			while (true)
			{
				Visited.Add(Current);
				Contour.Add(EdgePoints[Current]);

				const FIntPoint& Slots = EdgeLinks[Current];
				const int32 Next = (Slots.X != Previous) ? Slots.X : Slots.Y;
				if (Next == INDEX_NONE || Visited.Contains(Next))
				{
					break;
				}

				Previous = Current;
				Current = Next;
			}
		}
	}

	float DistanceToSegment(const FVector2f& Point, const FVector2f& A, const FVector2f& B)
	{
		const FVector2f AB = B - A;
		const float LengthSq = AB.SizeSquared();
		if (LengthSq <= KINDA_SMALL_NUMBER)
		{
			return FVector2f::Distance(Point, A);
		}

		const float T = FMath::Clamp(FVector2f::DotProduct(Point - A, AB) / LengthSq, 0.f, 1.f);
		return FVector2f::Distance(Point, A + AB * T);
	}

	// Douglas–Peucker로 거의 일직선인 점들을 한 직선으로 합친다. 곧은 벽이 자로 그은 듯 나온다.
	void SimplifyClosedContour(const FContour& In, const float Tolerance, FContour& Out)
	{
		const int32 Count = In.Num();
		if (Count < 4 || Tolerance <= 0.f)
		{
			Out = In;
			return;
		}

		// 닫힌 고리는 0번과 가장 먼 점 두 곳을 고정하고 양쪽 반을 각각 단순화한다.
		int32 Farthest = 0;
		float FarthestDistSq = 0.f;
		for (int32 Index = 1; Index < Count; ++Index)
		{
			const float DistSq = FVector2f::DistSquared(In[0], In[Index]);
			if (DistSq > FarthestDistSq)
			{
				FarthestDistSq = DistSq;
				Farthest = Index;
			}
		}

		TArray<bool> Keep;
		Keep.Init(false, Count);
		Keep[0] = true;
		Keep[Farthest] = true;

		// 인덱스 Count는 0번으로 감긴다.
		TArray<FIntPoint> Stack = { FIntPoint(0, Farthest), FIntPoint(Farthest, Count) };
		while (Stack.Num() > 0)
		{
			const FIntPoint Range = Stack.Pop(EAllowShrinking::No);
			const FVector2f& A = In[Range.X % Count];
			const FVector2f& B = In[Range.Y % Count];

			int32 MaxIndex = INDEX_NONE;
			float MaxDistance = Tolerance;
			for (int32 Index = Range.X + 1; Index < Range.Y; ++Index)
			{
				const float Distance = DistanceToSegment(In[Index % Count], A, B);
				if (Distance > MaxDistance)
				{
					MaxDistance = Distance;
					MaxIndex = Index;
				}
			}

			if (MaxIndex != INDEX_NONE)
			{
				Keep[MaxIndex % Count] = true;
				Stack.Add(FIntPoint(Range.X, MaxIndex));
				Stack.Add(FIntPoint(MaxIndex, Range.Y));
			}
		}

		Out.Reset();
		for (int32 Index = 0; Index < Count; ++Index)
		{
			if (Keep[Index])
			{
				Out.Add(In[Index]);
			}
		}
	}

	float GetContourPerimeter(const FContour& Contour)
	{
		float Perimeter = 0.f;
		for (int32 Index = 0; Index < Contour.Num(); ++Index)
		{
			Perimeter += FVector2f::Distance(Contour[Index], Contour[(Index + 1) % Contour.Num()]);
		}
		return Perimeter;
	}

	// 마스크 → 윤곽선 추출 → 직선화 → 짧은 고리(소품·노이즈) 제거
	void BuildVectorContours(
		const TArray<uint8>& Mask, const int32 Resolution, const float Tolerance, const float MinPerimeter,
		TArray<FContour>& OutContours)
	{
		TArray<FContour> RawContours;
		ExtractContours(Mask, Resolution, RawContours);

		OutContours.Reset();
		for (const FContour& Raw : RawContours)
		{
			if (Raw.Num() < 3 || GetContourPerimeter(Raw) < MinPerimeter)
			{
				continue;
			}

			FContour Simplified;
			SimplifyClosedContour(Raw, Tolerance, Simplified);
			if (Simplified.Num() >= 3)
			{
				OutContours.Add(MoveTemp(Simplified));
			}
		}
	}

	// even-odd 규칙으로 윤곽선 내부를 채운다. 바깥 고리 안의 구멍은 자동으로 비워진다.
	void FillContours(const TArray<FContour>& Contours, const int32 Resolution, TArray<uint8>& Classes, const uint8 Value)
	{
		struct FPolyEdge
		{
			float YMin;
			float YMax;
			float XAtYMin;
			float SlopeX;
		};

		TArray<FPolyEdge> Edges;
		for (const FContour& Contour : Contours)
		{
			for (int32 Index = 0; Index < Contour.Num(); ++Index)
			{
				FVector2f A = Contour[Index];
				FVector2f B = Contour[(Index + 1) % Contour.Num()];
				if (FMath::IsNearlyEqual(A.Y, B.Y))
				{
					continue;
				}
				if (A.Y > B.Y)
				{
					Swap(A, B);
				}
				Edges.Add({ A.Y, B.Y, A.X, (B.X - A.X) / (B.Y - A.Y) });
			}
		}

		Edges.Sort([](const FPolyEdge& L, const FPolyEdge& R) { return L.YMin < R.YMin; });

		TArray<int32> Active;
		TArray<float> Crossings;
		int32 NextEdge = 0;
		for (int32 Y = 0; Y < Resolution; ++Y)
		{
			const float SampleY = Y + 0.5f;
			while (NextEdge < Edges.Num() && Edges[NextEdge].YMin <= SampleY)
			{
				Active.Add(NextEdge++);
			}
			Active.RemoveAllSwap([&](const int32 EdgeIndex) { return Edges[EdgeIndex].YMax <= SampleY; }, EAllowShrinking::No);

			Crossings.Reset();
			for (const int32 EdgeIndex : Active)
			{
				const FPolyEdge& Edge = Edges[EdgeIndex];
				if (SampleY >= Edge.YMin)
				{
					Crossings.Add(Edge.XAtYMin + (SampleY - Edge.YMin) * Edge.SlopeX);
				}
			}
			Crossings.Sort();

			for (int32 Pair = 0; Pair + 1 < Crossings.Num(); Pair += 2)
			{
				const int32 StartX = FMath::Max(0, FMath::CeilToInt(Crossings[Pair] - 0.5f));
				const int32 EndX = FMath::Min(Resolution - 1, FMath::FloorToInt(Crossings[Pair + 1] - 0.5f));
				for (int32 X = StartX; X <= EndX; ++X)
				{
					Classes[Y * Resolution + X] = Value;
				}
			}
		}
	}

	// 윤곽선을 HalfWidth 반경으로 칠한다(픽셀 중심과 선분 사이 거리 기준).
	void StrokeContours(const TArray<FContour>& Contours, const int32 Resolution, const float HalfWidth, TArray<uint8>& Classes, const uint8 Value)
	{
		for (const FContour& Contour : Contours)
		{
			for (int32 Index = 0; Index < Contour.Num(); ++Index)
			{
				const FVector2f& A = Contour[Index];
				const FVector2f& B = Contour[(Index + 1) % Contour.Num()];

				const int32 MinX = FMath::Max(0, FMath::FloorToInt(FMath::Min(A.X, B.X) - HalfWidth));
				const int32 MaxX = FMath::Min(Resolution - 1, FMath::CeilToInt(FMath::Max(A.X, B.X) + HalfWidth));
				const int32 MinY = FMath::Max(0, FMath::FloorToInt(FMath::Min(A.Y, B.Y) - HalfWidth));
				const int32 MaxY = FMath::Min(Resolution - 1, FMath::CeilToInt(FMath::Max(A.Y, B.Y) + HalfWidth));

				for (int32 Y = MinY; Y <= MaxY; ++Y)
				{
					for (int32 X = MinX; X <= MaxX; ++X)
					{
						if (DistanceToSegment(FVector2f(X + 0.5f, Y + 0.5f), A, B) <= HalfWidth)
						{
							Classes[Y * Resolution + X] = Value;
						}
					}
				}
			}
		}
	}

	FString ToSvgPathData(const TArray<FContour>& Contours, const float Scale)
	{
		FString PathData;
		for (const FContour& Contour : Contours)
		{
			for (int32 Index = 0; Index < Contour.Num(); ++Index)
			{
				PathData += FString::Printf(TEXT("%s%.2f %.2f"), Index == 0 ? TEXT("M") : TEXT("L"),
					Contour[Index].X * Scale, Contour[Index].Y * Scale);
			}
			PathData += TEXT("Z");
		}
		return PathData;
	}

	FString ToSvgColor(const FLinearColor& Color)
	{
		const FColor SRGB = Color.ToFColor(true);
		return FString::Printf(TEXT("#%02x%02x%02x"), SRGB.R, SRGB.G, SRGB.B);
	}

	void SaveSvg(const UWorld& World, const FString& SvgText, const TCHAR* Suffix)
	{
		const FString FilePath = FPaths::ProjectSavedDir() / TEXT("Minimap") /
			FString::Printf(TEXT("%s_%s.svg"), *World.GetMapName(), Suffix);
		if (!FFileHelper::SaveStringToFile(SvgText, *FilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
		{
			UE_LOG(LogTemp, Error, TEXT("[ShooterMinimapBounds] SVG 저장 실패: %s"), *FilePath);
			return;
		}

		UE_LOG(LogTemp, Log, TEXT("[ShooterMinimapBounds] SVG 저장: %s"), *FPaths::ConvertRelativePathToFull(FilePath));
	}
}
#endif

AShooterMinimapBounds::AShooterMinimapBounds()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;

#if WITH_EDITORONLY_DATA
	// World Partition 맵에서 셀 언로드로 사라지면 맵 기준을 잃으므로 항상 로드되게 한다.
	bIsSpatiallyLoaded = false;
#endif

	BoundsBox = CreateDefaultSubobject<UBoxComponent>(TEXT("BoundsBox"));
	BoundsBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BoundsBox->SetCanEverAffectNavigation(false);
	BoundsBox->SetHiddenInGame(true);
	BoundsBox->ShapeColor = FColor::Cyan;
	BoundsBox->SetBoxExtent(FVector(MapWorldHalfSize, MapWorldHalfSize, MapWorldHalfHeight));
	SetRootComponent(BoundsBox);
}

void AShooterMinimapBounds::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// 스케일로 늘리면 정사각형이 깨질 수 있어 크기는 HalfSize 값으로만 조절한다.
	SetActorScale3D(FVector::OneVector);
	BoundsBox->SetBoxExtent(FVector(MapWorldHalfSize, MapWorldHalfSize, MapWorldHalfHeight));
}

void AShooterMinimapBounds::BeginPlay()
{
	Super::BeginPlay();

	if (UShooterMinimapSubsystem* Subsystem = UShooterMinimapSubsystem::Get(this))
	{
		Subsystem->RegisterMapBounds(this);
	}
}

void AShooterMinimapBounds::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UShooterMinimapSubsystem* Subsystem = UShooterMinimapSubsystem::Get(this))
	{
		Subsystem->UnregisterMapBounds(this);
	}

	Super::EndPlay(EndPlayReason);
}

FVector2D AShooterMinimapBounds::WorldToMapUV(const FVector& WorldLocation) const
{
	const FVector Local = GetActorTransform().InverseTransformPositionNoScale(WorldLocation);
	const float FullSize = MapWorldHalfSize * 2.f;

	// 텍스처 위쪽 = 로컬 +X, 오른쪽 = 로컬 +Y. 화면 Y는 아래로 증가하므로 X는 뒤집는다.
	return FVector2D(
		(Local.Y + MapWorldHalfSize) / FullSize,
		(MapWorldHalfSize - Local.X) / FullSize);
}

float AShooterMinimapBounds::WorldYawToMapAngle(const float WorldYaw) const
{
	// 로컬 Yaw 90(+Y, 맵 오른쪽)이 화면 시계방향 90도와 일치한다.
	return FRotator::NormalizeAxis(WorldYaw - GetActorRotation().Yaw);
}

#if WITH_EDITOR
void AShooterMinimapBounds::ExportTopDownCapture()
{
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	// BaseColor는 리니어 값이라 sRGB 타겟에 받아야 눈으로 본 색과 비슷하게 저장된다.
	UTextureRenderTarget2D* RenderTarget = UKismetRenderingLibrary::CreateRenderTarget2D(
		this, CaptureResolution, CaptureResolution, RTF_RGBA8_SRGB, FLinearColor::Black);
	if (RenderTarget == nullptr)
	{
		return;
	}

	USceneCaptureComponent2D* Capture = NewObject<USceneCaptureComponent2D>(this, NAME_None, RF_Transient);
	Capture->ProjectionType = ECameraProjectionMode::Orthographic;
	Capture->OrthoWidth = MapWorldHalfSize * 2.f;
	// 니어 플레인을 카메라 위치에 고정해서 박스 윗면보다 높은 물체(지붕 등)는 찍히지 않게 한다.
	Capture->bAutoCalculateOrthoPlanes = false;
	Capture->bUpdateOrthoPlanes = false;
	Capture->CaptureSource = CaptureSource;
	Capture->bCaptureEveryFrame = false;
	Capture->bCaptureOnMovement = false;
	Capture->TextureTarget = RenderTarget;
	Capture->RegisterComponentWithWorld(World);

	// Pitch -90으로 내려다보면 카메라 Up 벡터가 액터 Forward가 되어 WorldToMapUV와 방향이 일치한다.
	Capture->SetWorldLocationAndRotation(
		GetActorLocation() + FVector(0.f, 0.f, MapWorldHalfHeight),
		FRotator(-90.f, GetActorRotation().Yaw, 0.f));
	Capture->CaptureScene();

	// 씬 캡처 셰이더는 BaseColor/SceneColor 모드에서 알파를 0으로 쓴다. ExportRenderTarget으로 그대로
	// 내보내면 완전 투명 PNG(뷰어에서 검정)가 되므로 직접 읽어서 알파를 채운 뒤 저장한다.
	TArray<FColor> Pixels;
	FTextureRenderTargetResource* RenderTargetResource = RenderTarget->GameThread_GetRenderTargetResource();
	const bool bRead = RenderTargetResource && RenderTargetResource->ReadPixels(Pixels);

	Capture->DestroyComponent();

	if (!bRead || Pixels.Num() != CaptureResolution * CaptureResolution)
	{
		UE_LOG(LogTemp, Error, TEXT("[ShooterMinimapBounds] 탑다운 캡처 픽셀을 읽지 못했습니다."));
		return;
	}

	for (FColor& Pixel : Pixels)
	{
		Pixel.A = 255;
	}

	ShooterMinimapBoundsEditor::SavePng(*World, Pixels, CaptureResolution, TEXT("TopDown"));
}

void AShooterMinimapBounds::ExportLineArt()
{
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	// 작업은 출력보다 Supersample배 큰 해상도에서 하고, 마지막에 평균 내서 줄인다.
	const int32 OutputResolution = LineArtResolution;
	const int32 Supersample = FMath::Clamp(LineArtSupersample, 1, 4);
	const int32 Resolution = OutputResolution * Supersample;
	const int32 PixelCount = Resolution * Resolution;
	const float PixelWorldSize = MapWorldHalfSize * 2.f / Resolution;
	const FTransform BoundsTransform = GetActorTransform();

	// 1. 높이맵: 픽셀 중심마다 박스 윗면 → 바닥면으로 트레이스. 박스 위(지붕 등)는 애초에 안 맞는다.
	//    씬 쿼리는 읽기 전용이라 행 단위로 병렬 처리한다.
	constexpr float NoHit = TNumericLimits<float>::Max();
	TArray<float> Heights;
	Heights.Init(NoHit, PixelCount);

	const FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(MinimapLineArt), bLineArtTraceComplex, this);
	const ECollisionChannel TraceChannel = LineArtTraceChannel;
	{
		FScopedSlowTask SlowTask(Resolution, FText::FromString(TEXT("미니맵 라인아트: 높이 측정 중...")));
		SlowTask.MakeDialog(true);

		constexpr int32 RowsPerBatch = 64;
		for (int32 BatchStart = 0; BatchStart < Resolution; BatchStart += RowsPerBatch)
		{
			if (SlowTask.ShouldCancel())
			{
				return;
			}

			const int32 BatchRows = FMath::Min(RowsPerBatch, Resolution - BatchStart);
			ParallelFor(BatchRows, [&](const int32 RowOffset)
			{
				const int32 Y = BatchStart + RowOffset;

				// 픽셀 위쪽 = 로컬 +X, 오른쪽 = 로컬 +Y (WorldToMapUV와 동일)
				const float LocalX = MapWorldHalfSize - (Y + 0.5f) * PixelWorldSize;
				for (int32 X = 0; X < Resolution; ++X)
				{
					const float LocalY = -MapWorldHalfSize + (X + 0.5f) * PixelWorldSize;
					const FVector Start = BoundsTransform.TransformPositionNoScale(FVector(LocalX, LocalY, MapWorldHalfHeight));
					const FVector End = BoundsTransform.TransformPositionNoScale(FVector(LocalX, LocalY, -MapWorldHalfHeight));

					FHitResult Hit;
					if (World->LineTraceSingleByChannel(Hit, Start, End, TraceChannel, QueryParams))
					{
						Heights[Y * Resolution + X] = BoundsTransform.InverseTransformPositionNoScale(Hit.ImpactPoint).Z;
					}
				}
			});

			SlowTask.EnterProgressFrame(BatchRows);
		}
	}

	if (!Heights.ContainsByPredicate([NoHit](const float Height) { return Height != NoHit; }))
	{
		UE_LOG(LogTemp, Error, TEXT("[ShooterMinimapBounds] 트레이스가 하나도 맞지 않았습니다. 박스 위치/높이와 World Partition 영역 로드를 확인하세요."));
		return;
	}

	// 2. 주변 최저 높이(= 근처 바닥). 가로·세로 슬라이딩 최소값으로 O(N²)에 구한다. NoHit은 Max라 자연히 무시된다.
	const int32 SearchRadiusPixels = FMath::Max(0, FMath::RoundToInt(ObstacleSearchRadius / PixelWorldSize));
	TArray<float> RowMin;
	TArray<float> LocalMin;
	RowMin.SetNumUninitialized(PixelCount);
	LocalMin.SetNumUninitialized(PixelCount);
	ParallelFor(Resolution, [&](const int32 Y)
	{
		ShooterMinimapBoundsEditor::SlidingMin(&Heights[Y * Resolution], &RowMin[Y * Resolution], Resolution, 1, SearchRadiusPixels);
	});
	ParallelFor(Resolution, [&](const int32 X)
	{
		ShooterMinimapBoundsEditor::SlidingMin(&RowMin[X], &LocalMin[X], Resolution, Resolution, SearchRadiusPixels);
	});
	RowMin.Empty();

	// 3. 픽셀 분류: 빈 곳 / 바닥 / 장애물 / 선. 고해상도라 색 대신 1바이트 분류값만 들고 있는다.
	enum ELineArtClass : uint8 { Background, Floor, Obstacle, Line, ClassCount };

	TArray<uint8> Classes;
	Classes.SetNumUninitialized(PixelCount);
	for (int32 Index = 0; Index < PixelCount; ++Index)
	{
		const float Height = Heights[Index];
		if (Height == NoHit)
		{
			Classes[Index] = Background;
		}
		else
		{
			Classes[Index] = (Height - LocalMin[Index] > WallHeightThreshold) ? Obstacle : Floor;
		}
	}
	LocalMin.Empty();

	// 4. 벡터화: 분류 마스크의 경계를 선(좌표 목록)으로 뽑고 직선화한 뒤 다시 그린다.
	//    - 맵 영역(트레이스가 맞은 곳) 경계 → 장애물색으로 채움(선 없음)
	//    - 바닥 경계(벽·엄폐물·낭떠러지) → 바닥색으로 채우고 선을 그음
	//    픽셀 격자 대신 소수점 좌표의 곧은 선으로 다시 칠하므로 벽이 삐뚤빼뚤하지 않다.
	using ShooterMinimapBoundsEditor::FContour;

	const float SimplifyTolerancePixels = LineArtSimplifyTolerance / PixelWorldSize;
	const float MinPerimeterPixels = LineArtMinContourLength / PixelWorldSize;

	TArray<uint8> MapAreaMask;
	TArray<uint8> FloorMask;
	MapAreaMask.SetNumUninitialized(PixelCount);
	FloorMask.SetNumUninitialized(PixelCount);
	for (int32 Index = 0; Index < PixelCount; ++Index)
	{
		MapAreaMask[Index] = Classes[Index] != Background ? 1 : 0;
		FloorMask[Index] = Classes[Index] == Floor ? 1 : 0;
	}

	TArray<FContour> MapAreaContours;
	TArray<FContour> FloorContours;
	{
		FScopedSlowTask SlowTask(2.f, FText::FromString(TEXT("미니맵 라인아트: 선 추출/직선화 중...")));
		SlowTask.MakeDialog();

		SlowTask.EnterProgressFrame(1.f);
		ShooterMinimapBoundsEditor::BuildVectorContours(MapAreaMask, Resolution, SimplifyTolerancePixels, MinPerimeterPixels, MapAreaContours);
		MapAreaMask.Empty();

		SlowTask.EnterProgressFrame(1.f);
		ShooterMinimapBoundsEditor::BuildVectorContours(FloorMask, Resolution, SimplifyTolerancePixels, MinPerimeterPixels, FloorContours);
		FloorMask.Empty();
	}

	// 선 두께는 작업 해상도 기준 반지름으로 환산한다(출력 1px = Supersample px).
	const float HalfLineWidth = FMath::Max(0.5f, LineWidth * Supersample * 0.5f);

	FMemory::Memset(Classes.GetData(), Background, PixelCount);
	ShooterMinimapBoundsEditor::FillContours(MapAreaContours, Resolution, Classes, Obstacle);
	ShooterMinimapBoundsEditor::FillContours(FloorContours, Resolution, Classes, Floor);
	ShooterMinimapBoundsEditor::StrokeContours(FloorContours, Resolution, HalfLineWidth, Classes, Line);

	// 5. 다운샘플: 출력 1px에 들어가는 Supersample² 개 분류를 세서 색을 섞는다.
	//    알파가 다른 색(투명 배경 + 흰 선)을 섞으므로 프리멀티플라이드 리니어 공간에서 평균 낸다.
	FLinearColor Palette[ClassCount];
	Palette[Background] = LineArtBackgroundColor;
	Palette[Floor] = LineArtFloorColor;
	Palette[Obstacle] = LineArtObstacleColor;
	Palette[Line] = LineArtLineColor;

	const float SampleWeight = 1.f / (Supersample * Supersample);
	TArray<FColor> Output;
	Output.SetNumUninitialized(OutputResolution * OutputResolution);
	ParallelFor(OutputResolution, [&](const int32 OutY)
	{
		for (int32 OutX = 0; OutX < OutputResolution; ++OutX)
		{
			FLinearColor Premultiplied(0.f, 0.f, 0.f, 0.f);
			for (int32 SY = 0; SY < Supersample; ++SY)
			{
				for (int32 SX = 0; SX < Supersample; ++SX)
				{
					const int32 WorkIndex = (OutY * Supersample + SY) * Resolution + (OutX * Supersample + SX);
					const FLinearColor& Sample = Palette[Classes[WorkIndex]];
					Premultiplied += FLinearColor(Sample.R * Sample.A, Sample.G * Sample.A, Sample.B * Sample.A, Sample.A);
				}
			}

			Premultiplied *= SampleWeight;
			const float Alpha = Premultiplied.A;
			const FLinearColor Straight = Alpha > KINDA_SMALL_NUMBER
				? FLinearColor(Premultiplied.R / Alpha, Premultiplied.G / Alpha, Premultiplied.B / Alpha, Alpha)
				: FLinearColor(0.f, 0.f, 0.f, 0.f);

			Output[OutY * OutputResolution + OutX] = Straight.ToFColor(true);
		}
	});

	ShooterMinimapBoundsEditor::SavePng(*World, Output, OutputResolution, TEXT("Lines"));

	// 6. SVG: 같은 윤곽선을 출력 PNG 픽셀 좌표로 저장한다. 피그마/일러스트레이터에서 손보고 원하는 해상도로 다시 뽑을 수 있다.
	const float SvgScale = 1.f / Supersample;
	FString Svg = FString::Printf(
		TEXT("<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"%d\" height=\"%d\" viewBox=\"0 0 %d %d\">\n"),
		OutputResolution, OutputResolution, OutputResolution, OutputResolution);
	if (LineArtBackgroundColor.A > 0.f)
	{
		Svg += FString::Printf(TEXT("<rect id=\"Background\" width=\"100%%\" height=\"100%%\" fill=\"%s\" fill-opacity=\"%.3f\"/>\n"),
			*ShooterMinimapBoundsEditor::ToSvgColor(LineArtBackgroundColor), LineArtBackgroundColor.A);
	}
	Svg += FString::Printf(TEXT("<path id=\"MapArea\" fill=\"%s\" fill-opacity=\"%.3f\" fill-rule=\"evenodd\" stroke=\"none\" d=\"%s\"/>\n"),
		*ShooterMinimapBoundsEditor::ToSvgColor(LineArtObstacleColor), LineArtObstacleColor.A,
		*ShooterMinimapBoundsEditor::ToSvgPathData(MapAreaContours, SvgScale));
	Svg += FString::Printf(TEXT("<path id=\"Floor\" fill=\"%s\" fill-opacity=\"%.3f\" fill-rule=\"evenodd\" stroke=\"%s\" stroke-opacity=\"%.3f\" stroke-width=\"%.2f\" stroke-linejoin=\"round\" d=\"%s\"/>\n"),
		*ShooterMinimapBoundsEditor::ToSvgColor(LineArtFloorColor), LineArtFloorColor.A,
		*ShooterMinimapBoundsEditor::ToSvgColor(LineArtLineColor), LineArtLineColor.A, LineWidth,
		*ShooterMinimapBoundsEditor::ToSvgPathData(FloorContours, SvgScale));
	Svg += TEXT("</svg>\n");

	ShooterMinimapBoundsEditor::SaveSvg(*World, Svg, TEXT("Lines"));
}
#endif
