// Copyright 2026 Silvan Teufel. All Rights Reserved.

#include "FireLaneStatics.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "FireLaneSubsystem.h"

namespace
{
	/**
	 * The "not in the lane" sentinel.
	 *
	 * Ten kilometres. Large enough that no level puts a real friendly there, small enough that arithmetic
	 * on it stays finite - a caller that subtracts a collision radius from FLT_MAX and compares the result
	 * gets a number, not an infinity.
	 */
	constexpr float FireLaneOutOfLane = 1.0e6f;
}

UFireLaneSubsystem* UFireLaneStatics::GetFireLane(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine
		? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull)
		: nullptr;
	return World ? World->GetSubsystem<UFireLaneSubsystem>() : nullptr;
}

void UFireLaneStatics::RegisterFriendly(const UObject* WorldContextObject, AActor* Actor, int32 TeamId)
{
	if (UFireLaneSubsystem* Lane = GetFireLane(WorldContextObject))
	{
		Lane->RegisterFriendly(Actor, TeamId);
	}
}

void UFireLaneStatics::UnregisterFriendly(const UObject* WorldContextObject, AActor* Actor)
{
	if (UFireLaneSubsystem* Lane = GetFireLane(WorldContextObject))
	{
		Lane->UnregisterFriendly(Actor);
	}
}

FFireLaneStats UFireLaneStatics::GetFireLaneStats(const UObject* WorldContextObject)
{
	if (const UFireLaneSubsystem* Lane = GetFireLane(WorldContextObject))
	{
		return Lane->GetStats();
	}
	return FFireLaneStats();
}

// --------------------------------------------------------------------------------------------- rules

float UFireLaneStatics::OutOfLaneDistance()
{
	return FireLaneOutOfLane;
}

FVector UFireLaneStatics::ProjectPosition(const FVector& Location, const FVector& Velocity, float Seconds)
{
	// A negative lookahead would put the friendly where he used to be, which is worse than not looking
	// ahead at all: it grants permission based on ground he has already left.
	return Location + Velocity * FMath::Max(Seconds, 0.0f);
}

float UFireLaneStatics::DistanceToLane(const FVector& Point, const FVector& MuzzleLocation,
	const FVector& TargetLocation, float Overshoot, float& OutAlong)
{
	const FVector ToTarget = TargetLocation - MuzzleLocation;
	const float LaneLength = ToTarget.Size() + FMath::Max(Overshoot, 0.0f);

	// Degenerate lane: the shooter is standing on the target. There is no direction to speak of, so the
	// only meaningful distance is to the muzzle itself.
	if (LaneLength <= KINDA_SMALL_NUMBER)
	{
		OutAlong = 0.0f;
		return FVector::Dist(Point, MuzzleLocation);
	}

	const FVector Direction = ToTarget.GetSafeNormal();
	const float Along = FVector::DotProduct(Point - MuzzleLocation, Direction);
	OutAlong = Along;

	// Behind the barrel, or past the end of the lane. Both are "not in the way", and saying so with a
	// sentinel rather than a clamp is what stops the gate from holding fire for the man standing behind
	// the shooter - the single most common bug in a hand-rolled version of this check.
	if (Along < 0.0f || Along > LaneLength)
	{
		return FireLaneOutOfLane;
	}

	const FVector Closest = MuzzleLocation + Direction * Along;
	return FVector::Dist(Point, Closest);
}

EFireLaneVerdict UFireLaneStatics::ClassifyLane(float ClosestApproach, float LaneRadius, float GrazeMargin)
{
	const float Radius = FMath::Max(LaneRadius, 0.0f);
	const float Margin = FMath::Max(GrazeMargin, 0.0f);

	// Inclusive at the radius. A friendly standing EXACTLY at the edge of the danger volume is in the
	// danger volume; rounding that the other way is a decision to shoot somebody occasionally.
	if (ClosestApproach <= Radius)
	{
		return EFireLaneVerdict::BlockedByFriendly;
	}
	if (ClosestApproach <= Radius + Margin)
	{
		return EFireLaneVerdict::Grazing;
	}
	return EFireLaneVerdict::Clear;
}

FVector UFireLaneStatics::SidestepOffset(const FVector& MuzzleLocation, const FVector& TargetLocation,
	const FVector& FriendlyLocation, float LaneRadius, float Clearance)
{
	const FVector ToTarget = TargetLocation - MuzzleLocation;
	const float LaneLength = ToTarget.Size();
	if (LaneLength <= KINDA_SMALL_NUMBER)
	{
		return FVector::ZeroVector;
	}

	const FVector Direction = ToTarget / LaneLength;
	const float Along = FVector::DotProduct(FriendlyLocation - MuzzleLocation, Direction);
	const FVector Closest = MuzzleLocation + Direction * Along;

	// The offset of the friendly from the lane, flattened. A step is taken on the ground; a vertical
	// component here would ask an AI to solve a firing angle by levitating.
	FVector Offset = FriendlyLocation - Closest;
	Offset.Z = 0.0f;

	const float Needed = FMath::Max(LaneRadius, 0.0f) + FMath::Max(Clearance, 0.0f);
	const float Current = Offset.Size();
	if (Current >= Needed)
	{
		return FVector::ZeroVector;  // already open
	}

	// Which way to step. Away from the friendly; and when he is directly above or below the lane there is
	// no "away", so fall back to the lane's right-hand side rather than returning a zero vector that the
	// caller would read as "nothing to do".
	FVector Away = (Current > KINDA_SMALL_NUMBER)
		? (-Offset / Current)
		: FVector::CrossProduct(Direction, FVector::UpVector).GetSafeNormal();
	if (Away.IsNearlyZero())
	{
		Away = FVector::RightVector;
	}

	// Moving the shooter pivots the lane about the target, so a step of S at the muzzle moves the lane by
	// S * (1 - u) where u is how far down the lane the friendly stands. Close to the target, u approaches
	// one and the required step runs away to infinity - which is true, and the reason the caller gets to
	// refuse it rather than this function quietly clamping and reporting a step that would not work.
	const float U = FMath::Clamp(Along / LaneLength, 0.0f, 1.0f);
	const float Leverage = FMath::Max(1.0f - U, 0.05f);
	const float Step = (Needed - Current) / Leverage;

	return Away * Step;
}

bool UFireLaneStatics::IsWithinMuzzleClearance(float DistanceToMuzzle, float MuzzleClearance)
{
	// A clearance of zero switches the rule off rather than making every position a violation.
	return MuzzleClearance > 0.0f && DistanceToMuzzle <= MuzzleClearance;
}

float UFireLaneStatics::SecondsUntilLaneClears(float ClosestApproach, float ApproachRate, float LaneRadius)
{
	const float Radius = FMath::Max(LaneRadius, 0.0f);
	if (ClosestApproach > Radius)
	{
		return 0.0f;  // already out
	}

	// Standing still, or walking further in. Waiting will not help, and saying "never" out loud is what
	// lets the caller choose to move instead of holding a trigger it will never pull.
	if (ApproachRate <= KINDA_SMALL_NUMBER)
	{
		return -1.0f;
	}

	return (Radius - ClosestApproach) / ApproachRate;
}
