// Copyright 2026 Silvan Teufel. All Rights Reserved.

#include "FireLaneDemoDirector.h"

#include "Components/TextRenderComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "FireLaneSettings.h"
#include "FireLaneStatics.h"

namespace FireLaneDemoLocal
{
	static const TCHAR* VerdictText(EFireLaneVerdict V)
	{
		switch (V)
		{
		case EFireLaneVerdict::Clear:             return TEXT("CLEAR - FIRE");
		case EFireLaneVerdict::Grazing:           return TEXT("GRAZING - TOO CLOSE TO CALL");
		case EFireLaneVerdict::BlockedByFriendly: return TEXT("BLOCKED - ONE OF OURS IN THE LANE");
		case EFireLaneVerdict::BlockedByGeometry: return TEXT("BLOCKED - WALL");
		case EFireLaneVerdict::MuzzleBlocked:     return TEXT("BLOCKED - IN FRONT OF THE BARREL");
		default:                                  return TEXT("?");
		}
	}

	static FColor VerdictColour(EFireLaneVerdict V)
	{
		switch (V)
		{
		case EFireLaneVerdict::Clear:             return FColor(120, 230, 120);
		case EFireLaneVerdict::Grazing:           return FColor(235, 215, 110);
		case EFireLaneVerdict::BlockedByFriendly: return FColor(240, 110, 110);
		case EFireLaneVerdict::BlockedByGeometry: return FColor(150, 150, 150);
		case EFireLaneVerdict::MuzzleBlocked:     return FColor(230, 120, 230);
		default:                                  return FColor::White;
		}
	}

	/**
	 * How fast the team-mate is walking.
	 *
	 * The derivative of the same sine the position comes from, not a separately guessed number: the
	 * lookahead is only honest if the velocity it projects along is the velocity he actually has.
	 */
	static FVector AllyVelocity(float T, float Travel, float Period, const FVector& Axis)
	{
		const float Omega = 2.0f * PI / Period;
		return Axis.GetSafeNormal() * (Travel * Omega * FMath::Cos(Omega * T));
	}

	constexpr float WalkPeriod = 6.0f;
}

AFireLaneDemoDirector::AFireLaneDemoDirector()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	BoardText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("BoardText"));
	BoardText->SetupAttachment(Root);
	BoardText->SetHorizontalAlignment(EHTA_Center);
	BoardText->SetVerticalAlignment(EVRTA_TextBottom);
	BoardText->SetWorldSize(38.0f);
	BoardText->SetTextRenderColor(FColor::White);

	// Yaw 270, not 90: at 90 the text renders mirrored. Measured, not guessed.
	BoardText->SetRelativeRotation(FRotator(0.0f, 270.0f, 0.0f));
	BoardText->SetRelativeLocation(FVector(0.0f, 0.0f, 640.0f));

	VerdictText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("VerdictText"));
	VerdictText->SetupAttachment(Root);
	VerdictText->SetHorizontalAlignment(EHTA_Center);
	VerdictText->SetVerticalAlignment(EVRTA_TextBottom);
	VerdictText->SetWorldSize(52.0f);
	VerdictText->SetTextRenderColor(FColor(120, 230, 120));
	VerdictText->SetRelativeRotation(FRotator(0.0f, 270.0f, 0.0f));
	VerdictText->SetRelativeLocation(FVector(0.0f, 0.0f, 500.0f));
}

void AFireLaneDemoDirector::BeginPlay()
{
	Super::BeginPlay();
	CycleTime = 0.0f;
}

FVector AFireLaneDemoDirector::ShooterLocation() const
{
	if (const AActor* Actor = ShooterActor.Get())
	{
		return Actor->GetActorLocation() + FVector(0.0f, 0.0f, 60.0f);
	}
	return GetActorLocation() + FallbackShooter;
}

FVector AFireLaneDemoDirector::TargetLocation() const
{
	if (const AActor* Actor = TargetActor.Get())
	{
		return Actor->GetActorLocation() + FVector(0.0f, 0.0f, 60.0f);
	}
	return GetActorLocation() + FallbackTarget;
}

FVector AFireLaneDemoDirector::AllyLocation(float T) const
{
	// Halfway down the lane by default, walking across it. Halfway is the interesting place: close to the
	// muzzle the sidestep is trivial, close to the target it is impossible, and in the middle you can see
	// the gate actually solving something.
	const FVector Home = bAllyHomeCaptured
		? AllyHome
		: (ShooterLocation() + TargetLocation()) * 0.5f;

	const float Omega = 2.0f * PI / FireLaneDemoLocal::WalkPeriod;
	return Home + AllyAxis.GetSafeNormal() * (AllyTravel * FMath::Sin(Omega * T));
}

float AFireLaneDemoDirector::DemoRadius() const
{
	return DemoLaneRadius >= 0.0f ? DemoLaneRadius : UFireLaneSettings::Get()->LaneRadius;
}

float AFireLaneDemoDirector::DemoMargin() const
{
	return DemoGrazeMargin >= 0.0f ? DemoGrazeMargin : UFireLaneSettings::Get()->GrazeMargin;
}

FFireLaneResult AFireLaneDemoDirector::Judge(float T, FVector& OutAlly, FVector& OutFuture) const
{
	const UFireLaneSettings* Settings = UFireLaneSettings::Get();

	const FVector Muzzle = ShooterLocation();
	const FVector Target = TargetLocation();
	OutAlly = AllyLocation(T);

	// Where he will be, not where he is. The same projection the component makes, from the same function.
	const FVector Velocity = FireLaneDemoLocal::AllyVelocity(T, AllyTravel, FireLaneDemoLocal::WalkPeriod,
		AllyAxis);
	OutFuture = UFireLaneStatics::ProjectPosition(OutAlly, Velocity, Settings->LookaheadSeconds);

	float Along = 0.0f;
	const float Approach = UFireLaneStatics::DistanceToLane(OutFuture, Muzzle, Target,
		Settings->TargetOvershoot, Along);

	FFireLaneResult Result;
	Result.ClosestApproach = Approach;
	Result.Verdict = UFireLaneStatics::ClassifyLane(Approach, DemoRadius(), DemoMargin());
	Result.Blocker = AllyActor.Get();

	if (Result.Verdict == EFireLaneVerdict::BlockedByFriendly)
	{
		Result.SuggestedSidestep = UFireLaneStatics::SidestepOffset(Muzzle, Target, OutFuture,
			DemoRadius(), Settings->SidestepClearance);
		const float MaxStep = DemoMaxSidestep >= 0.0f ? DemoMaxSidestep : Settings->MaxSidestep;
		if (MaxStep > 0.0f && Result.SuggestedSidestep.SizeSquared() > FMath::Square(MaxStep))
		{
			Result.SuggestedSidestep = FVector::ZeroVector;
		}
	}

	return Result;
}

void AFireLaneDemoDirector::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// The editor hands out one enormous delta after a recompile. Unclamped, the whole script runs inside
	// a single frame and the demo looks broken for reasons that have nothing to do with the plugin.
	DeltaSeconds = FMath::Clamp(DeltaSeconds, 0.0f, 0.1f);

	if (!bAllyHomeCaptured)
	{
		if (const AActor* Ally = AllyActor.Get())
		{
			AllyHome = Ally->GetActorLocation();
			bAllyHomeCaptured = true;
		}
	}

	CycleTime += DeltaSeconds;
	if (CycleTime > CycleSeconds)
	{
		CycleTime = 0.0f;
	}
	const float T = CycleTime;

	FVector Ally = FVector::ZeroVector;
	FVector Future = FVector::ZeroVector;
	const FFireLaneResult Result = Judge(T, Ally, Future);

	// Walk the team-mate, if the level gave us one to walk.
	if (AActor* AllyPawn = AllyActor.Get())
	{
		AllyPawn->SetActorLocation(Ally);
	}

	if (BoardText)
	{
		BoardText->SetText(FText::FromString(TeachingLine(T)));
	}
	if (VerdictText)
	{
		// The verdict AND the number it came from. A demo that only shows a word asks to be believed;
		// one that shows the measurement can be checked against the picture.
		const float Approach = Result.ClosestApproach;
		VerdictText->SetText(FText::FromString(
			Approach >= UFireLaneStatics::OutOfLaneDistance() * 0.5f
			? FString::Printf(TEXT("%s\nnot in the lane at all"),
				FireLaneDemoLocal::VerdictText(Result.Verdict))
			: FString::Printf(TEXT("%s\n%.0f cm from the centre line, lane radius %.0f"),
				FireLaneDemoLocal::VerdictText(Result.Verdict), Approach, DemoRadius())));
		VerdictText->SetTextRenderColor(FireLaneDemoLocal::VerdictColour(Result.Verdict));
	}

	if (bDrawDemo)
	{
		DrawScene(T, Result, Ally, Future);
	}
}

FString AFireLaneDemoDirector::TeachingLine(float T) const
{
	// Every one of these is true in every frame, whatever the verdict happens to be. That is the whole
	// requirement: the lookahead moves the judgement ahead of the position, so any caption tied to where
	// the man currently IS will sooner or later contradict the verdict printed under it.
	const int32 Which = static_cast<int32>(T / 5.0f) % 3;
	switch (Which)
	{
	case 0:  return TEXT("the lane is a VOLUME - the capsule is the projectile's danger radius");
	case 1:  return TEXT("judged where he WILL be: the faint sphere is his position in 0.35 s");
	default: return TEXT("blocked is not stop - the cyan arrow is the step that opens the lane");
	}
}

void AFireLaneDemoDirector::DrawScene(float T, const FFireLaneResult& Result, const FVector& Ally,
	const FVector& Future) const
{
#if ENABLE_DRAW_DEBUG
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	UWorld* Mutable = const_cast<UWorld*>(World);

	const FVector Muzzle = ShooterLocation();
	const FVector Target = TargetLocation();
	const FColor Colour = FireLaneDemoLocal::VerdictColour(Result.Verdict);
	const float Radius = DemoRadius();
	const float Margin = DemoMargin();

	// A short lifetime rather than zero: in the editor viewport a zero-lifetime shape sometimes lands in
	// the frame after the one being captured, and a screenshot of an empty scene is hard to diagnose.
		// 0.35 s, not one frame. A debug shape lives on the line batcher and is dropped when its time is
	// up - and during a long screenshot run the editor renders many frames between two world ticks,
	// so anything drawn for a twelfth of a second is gone by the time the picture is taken. Measured
	// on 11.09.2026: a whole 240-frame capture came out with no lane, no spheres and no bars.
	constexpr float Life = 0.35f;

	// The lane, drawn as what it actually is - a capsule of the projectile's danger radius. Drawing it as
	// a line would illustrate the naive implementation this plugin exists to replace.
	const FVector Mid = (Muzzle + Target) * 0.5f;
	const FVector Axis = Target - Muzzle;
	const FQuat Along = FRotationMatrix::MakeFromZ(Axis.GetSafeNormal()).ToQuat();
	DrawDebugCapsule(Mutable, Mid, Axis.Size() * 0.5f, Radius, Along, Colour, false, Life, 0, 4.0f);

	// A ring every two metres down the lane, so the volume reads as a tube rather than as two lines.
	const int32 Rings = FMath::Clamp(FMath::FloorToInt(Axis.Size() / 200.0f), 2, 16);
	for (int32 i = 1; i < Rings; ++i)
	{
		const FVector At = Muzzle + Axis * (static_cast<float>(i) / Rings);
		DrawDebugCircle(Mutable, At, Radius, 24, Colour, false, Life, 0, 2.0f,
			FVector::CrossProduct(Axis.GetSafeNormal(), FVector::UpVector).GetSafeNormal(),
			FVector::UpVector, false);
	}

	// The graze band as a wider, fainter shell.
	DrawDebugCapsule(Mutable, Mid, Axis.Size() * 0.5f, Radius + Margin, Along,
		FColor(110, 110, 130), false, Life, 0, 1.5f);

	// The three parties. Spheres so the demo still reads when nobody assigned any meshes.
	DrawDebugSphere(Mutable, Muzzle, 55.0f, 14, FColor(120, 200, 255), false, Life, 0, 4.0f);
	DrawDebugSphere(Mutable, Target, 55.0f, 14, FColor(240, 140, 140), false, Life, 0, 4.0f);
	DrawDebugSphere(Mutable, Ally, 55.0f, 14, FColor(140, 240, 180), false, Life, 0, 4.0f);

	// Where he will be when the bullet gets there. This ghost is the whole argument for the lookahead:
	// the gate is judging the faint sphere, not the solid one.
	DrawDebugSphere(Mutable, Future, 55.0f, 14, FColor(90, 175, 130), false, Life, 0, 2.5f);
	DrawDebugDirectionalArrow(Mutable, Ally, Future, 45.0f, FColor(90, 175, 130), false, Life, 0, 3.0f);

	// The suggested step.
	if (!Result.SuggestedSidestep.IsNearlyZero())
	{
		DrawDebugDirectionalArrow(Mutable, Muzzle, Muzzle + Result.SuggestedSidestep, 80.0f,
			FColor(90, 220, 255), false, Life, 0, 7.0f);

		// ...and the lane that step would open, so the claim is visible rather than asserted.
		const FVector Stepped = Muzzle + Result.SuggestedSidestep;
		DrawDebugLine(Mutable, Stepped, Target, FColor(90, 220, 255), false, Life, 0, 3.0f);
	}
#endif
}
