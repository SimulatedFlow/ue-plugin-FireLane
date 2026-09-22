// Copyright 2026 Silvan Teufel. All Rights Reserved.

#include "FireLaneComponent.h"

#include "CollisionQueryParams.h"
#include "Components/SceneComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "FireLaneLog.h"
#include "FireLaneSettings.h"
#include "FireLaneStatics.h"
#include "FireLaneSubsystem.h"
#include "GameFramework/Actor.h"

UFireLaneComponent::UFireLaneComponent()
{
	// Nothing to do per frame. The gate answers when it is asked; a component that ticked in order to
	// pre-compute an answer nobody requested would be spending a frame budget on speculation.
	PrimaryComponentTick.bCanEverTick = false;
}

void UFireLaneComponent::BeginPlay()
{
	Super::BeginPlay();

	if (bRegisterOwnerAsFriendly)
	{
		if (UFireLaneSubsystem* Lane = UFireLaneStatics::GetFireLane(this))
		{
			Lane->RegisterFriendly(GetOwner(), TeamId);
		}
	}
}

void UFireLaneComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	if (UFireLaneSubsystem* Lane = UFireLaneStatics::GetFireLane(this))
	{
		Lane->UnregisterFriendly(GetOwner());
	}
	Super::EndPlay(Reason);
}

void UFireLaneComponent::SetMuzzle(USceneComponent* Component, FName SocketName)
{
	MuzzleComponent = Component;
	MuzzleSocket = SocketName;
}

FVector UFireLaneComponent::GetMuzzleLocation() const
{
	if (const USceneComponent* Muzzle = MuzzleComponent.Get())
	{
		return (MuzzleSocket != NAME_None && Muzzle->DoesSocketExist(MuzzleSocket))
			? Muzzle->GetSocketLocation(MuzzleSocket)
			: Muzzle->GetComponentLocation();
	}

	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return FVector::ZeroVector;
	}

	// Owner space, not world space: a shooter facing the other way should not have its muzzle behind it.
	return Owner->GetActorLocation() + Owner->GetActorRotation().RotateVector(MuzzleOffset);
}

void UFireLaneComponent::SetTeamId(int32 NewTeamId)
{
	TeamId = NewTeamId;
	if (bRegisterOwnerAsFriendly)
	{
		if (UFireLaneSubsystem* Lane = UFireLaneStatics::GetFireLane(this))
		{
			Lane->RegisterFriendly(GetOwner(), TeamId);
		}
	}
}

float UFireLaneComponent::EffectiveLaneRadius() const
{
	return LaneRadiusOverride >= 0.0f ? LaneRadiusOverride : UFireLaneSettings::Get()->LaneRadius;
}

float UFireLaneComponent::EffectiveGrazeMargin() const
{
	return GrazeMarginOverride >= 0.0f ? GrazeMarginOverride : UFireLaneSettings::Get()->GrazeMargin;
}

float UFireLaneComponent::EffectiveLookahead() const
{
	return LookaheadSecondsOverride >= 0.0f
		? LookaheadSecondsOverride
		: UFireLaneSettings::Get()->LookaheadSeconds;
}

float UFireLaneComponent::EffectiveOvershoot() const
{
	return TargetOvershootOverride >= 0.0f
		? TargetOvershootOverride
		: UFireLaneSettings::Get()->TargetOvershoot;
}

FFireLaneResult UFireLaneComponent::CheckLane(AActor* TargetActor)
{
	if (!IsValid(TargetActor))
	{
		return FFireLaneResult();
	}
	return Evaluate(TargetActor, TargetActor->GetActorLocation());
}

FFireLaneResult UFireLaneComponent::CheckLaneToLocation(const FVector& TargetLocation)
{
	return Evaluate(nullptr, TargetLocation);
}

bool UFireLaneComponent::CanFire(AActor* TargetActor)
{
	const FFireLaneResult Result = CheckLane(TargetActor);
	return Result.Verdict == EFireLaneVerdict::Clear
		|| (bAllowGrazingShots && Result.Verdict == EFireLaneVerdict::Grazing);
}

FFireLaneResult UFireLaneComponent::Evaluate(AActor* TargetActor, const FVector& TargetLocation)
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return FFireLaneResult();
	}

	// A verdict that is a few hundredths of a second old is the same verdict. Re-checking per call would
	// mean a behaviour tree that asks three times in one frame pays three times for one answer - but only
	// while the question is the same one, which is why the target is part of the cache key.
	const double Now = World->GetTimeSeconds();
	const float StaleAfter = UFireLaneSettings::Get()->VerdictStaleAfter;
	const bool bSameQuestion = (LastCheckTarget.Get() == TargetActor)
		&& (TargetActor != nullptr || LastCheckLocation.Equals(TargetLocation, 1.0f));
	if (StaleAfter > 0.0f && LastCheckTime >= 0.0 && bSameQuestion
		&& (Now - LastCheckTime) < StaleAfter)
	{
		return LastResult;
	}

	const FVector Muzzle = GetMuzzleLocation();
	const float LaneRadius = EffectiveLaneRadius();
	const float GrazeMargin = EffectiveGrazeMargin();
	const float Lookahead = EffectiveLookahead();
	const float Overshoot = EffectiveOvershoot();

	FFireLaneResult Result;
	Result.ClosestApproach = UFireLaneStatics::OutOfLaneDistance();

	UFireLaneSubsystem* Lane = UFireLaneStatics::GetFireLane(this);

	// --------------------------------------------------------------------------------- our own people

	TArray<FFireLaneFriendly> Friendlies;
	if (Lane)
	{
		Lane->GatherFriendlies(TeamId, GetOwner(), Friendlies);
	}

	const AActor* Owner = GetOwner();
	float WorstApproach = UFireLaneStatics::OutOfLaneDistance();
	AActor* WorstActor = nullptr;
	FVector WorstFuture = FVector::ZeroVector;
	float WorstApproachNow = UFireLaneStatics::OutOfLaneDistance();

	for (const FFireLaneFriendly& Entry : Friendlies)
	{
		AActor* Friendly = Entry.Actor.Get();
		if (!Friendly || Friendly == TargetActor)
		{
			continue;
		}

		// Straight in front of the barrel beats every other consideration: at that range the lane is not
		// what hurts him, the muzzle is.
		const float ToMuzzle = FVector::Dist(Friendly->GetActorLocation(), Muzzle) - Entry.Radius;
		if (UFireLaneStatics::IsWithinMuzzleClearance(ToMuzzle, UFireLaneSettings::Get()->MuzzleClearance))
		{
			Result.Verdict = EFireLaneVerdict::MuzzleBlocked;
			Result.Blocker = Friendly;
			Result.ClosestApproach = FMath::Max(ToMuzzle, 0.0f);
			Result.SecondsUntilClear = -1.0f;
			Result.bGeometryChecked = false;
			if (Lane)
			{
				Lane->NoteCheck(true, false);
			}
			LastResult = Result;
			LastCheckTime = Now;
			LastCheckTarget = TargetActor;
			LastCheckLocation = TargetLocation;
			OnBlocked.Broadcast(Friendly, FVector::ZeroVector, -1.0f);
			return Result;
		}

		// Judged where he WILL be. Checking where he is now hands out permission to fire at a man who is
		// mid-stride into the lane, and the bullet and the stride arrive together.
		const FVector Future = UFireLaneStatics::ProjectPosition(
			Friendly->GetActorLocation(), Friendly->GetVelocity(), Lookahead);

		float Along = 0.0f;
		const float Raw = UFireLaneStatics::DistanceToLane(Future, Muzzle, TargetLocation, Overshoot, Along);
		const float Approach = Raw - Entry.Radius;
		if (Approach < WorstApproach)
		{
			WorstApproach = Approach;
			WorstActor = Friendly;
			WorstFuture = Future;

			float AlongNow = 0.0f;
			WorstApproachNow = UFireLaneStatics::DistanceToLane(
				Friendly->GetActorLocation(), Muzzle, TargetLocation, Overshoot, AlongNow) - Entry.Radius;
		}
	}

	Result.ClosestApproach = WorstApproach;
	Result.Verdict = UFireLaneStatics::ClassifyLane(WorstApproach, LaneRadius, GrazeMargin);

	const bool bFriendlyBlock = (Result.Verdict == EFireLaneVerdict::BlockedByFriendly);
	if (bFriendlyBlock || Result.Verdict == EFireLaneVerdict::Grazing)
	{
		Result.Blocker = WorstActor;
	}

	if (bFriendlyBlock && WorstActor)
	{
		Result.SuggestedSidestep = UFireLaneStatics::SidestepOffset(Muzzle, TargetLocation, WorstFuture,
			LaneRadius, UFireLaneSettings::Get()->SidestepClearance);

		// A step longer than the shooter is allowed to take is not an answer, it is a different plan. Say
		// so by returning nothing rather than a number the caller will walk a fraction of and still be
		// blocked.
		const float MaxStep = UFireLaneSettings::Get()->MaxSidestep;
		if (MaxStep > 0.0f && Result.SuggestedSidestep.SizeSquared() > FMath::Square(MaxStep))
		{
			Result.SuggestedSidestep = FVector::ZeroVector;
		}

		// Is he walking out of it by himself? The rate is measured between now and the lookahead, which is
		// the same projection the verdict was based on - two different estimates would contradict.
		const float Rate = (Lookahead > KINDA_SMALL_NUMBER)
			? (WorstApproach - WorstApproachNow) / Lookahead
			: 0.0f;
		Result.SecondsUntilClear = UFireLaneStatics::SecondsUntilLaneClears(WorstApproach, Rate, LaneRadius);
	}

	// -------------------------------------------------------------------------------------- the world

	bool bTraced = false;
	if (UFireLaneSettings::Get()->bCheckGeometry && Result.Verdict != EFireLaneVerdict::BlockedByFriendly)
	{
		if (Lane && Lane->ClaimTrace())
		{
			bTraced = true;

			FCollisionQueryParams Params(SCENE_QUERY_STAT(FireLane), false, Owner);
			if (TargetActor)
			{
				Params.AddIgnoredActor(TargetActor);
			}

			FHitResult Hit;
			const bool bHit = World->LineTraceSingleByChannel(Hit, Muzzle, TargetLocation,
				UFireLaneSettings::Get()->GeometryChannel, Params);
			if (bHit)
			{
				Result.Verdict = EFireLaneVerdict::BlockedByGeometry;
				Result.Blocker = Hit.GetActor();
				Result.SuggestedSidestep = FVector::ZeroVector;
				Result.SecondsUntilClear = -1.0f;
			}
		}
	}
	Result.bGeometryChecked = bTraced;

	if (Lane)
	{
		Lane->NoteCheck(bFriendlyBlock, bTraced);

		if (Lane->bDebugDraw || UFireLaneSettings::Get()->bDrawDebugLanes)
		{
			FColor Colour = FColor::Green;
			switch (Result.Verdict)
			{
			case EFireLaneVerdict::Grazing:           Colour = FColor::Yellow; break;
			case EFireLaneVerdict::BlockedByFriendly: Colour = FColor::Red; break;
			case EFireLaneVerdict::BlockedByGeometry: Colour = FColor(120, 120, 120); break;
			case EFireLaneVerdict::MuzzleBlocked:     Colour = FColor::Magenta; break;
			default: break;
			}
			DrawDebugLine(World, Muzzle, TargetLocation, Colour, false, 0.1f, 0, 2.0f);
			if (!Result.SuggestedSidestep.IsNearlyZero())
			{
				DrawDebugDirectionalArrow(World, Muzzle, Muzzle + Result.SuggestedSidestep,
					40.0f, FColor::Cyan, false, 0.1f, 0, 3.0f);
			}
		}
	}

	if (bFriendlyBlock)
	{
		OnBlocked.Broadcast(Result.Blocker, Result.SuggestedSidestep, Result.SecondsUntilClear);
	}

	LastResult = Result;
	LastCheckTime = Now;
	LastCheckTarget = TargetActor;
	LastCheckLocation = TargetLocation;
	return Result;
}
