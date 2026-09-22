// Copyright 2026 Silvan Teufel. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FireLaneTypes.generated.h"

/**
 * What the lane looks like right now.
 *
 * Grazing exists because the two-state version of this question is what makes AI shooters feel broken.
 * A gate that only knows "clear" and "blocked" is tuned either so tight that the squad never fires, or
 * so loose that it shoots its own medic. Grazing is the band in between: nobody is in the lane, but
 * somebody is close enough that a designer may want an important shot to hold anyway.
 */
UENUM(BlueprintType)
enum class EFireLaneVerdict : uint8
{
	/** Nothing of ours in the lane and, if it was checked, nothing solid either. Fire. */
	Clear,

	/** A friendly is outside the danger radius but inside the graze margin. The project decides. */
	Grazing,

	/** A friendly is in the lane. Do not fire. */
	BlockedByFriendly,

	/** The world is in the way - a wall, a crate, the floor. Not a safety problem, just a wasted shot. */
	BlockedByGeometry,

	/** Somebody is directly in front of the barrel, whatever the lane says. */
	MuzzleBlocked
};

/**
 * The full answer, so that an AI can act on it rather than only obey it.
 *
 * A gate that returns a bool forces every project to re-derive the interesting part: who is in the way,
 * how badly, whether waiting would help, and where to stand instead.
 */
USTRUCT(BlueprintType)
struct FIRELANE_API FFireLaneResult
{
	GENERATED_BODY()

	/** The verdict. Everything else is context for it. */
	UPROPERTY(BlueprintReadOnly, Category = "FireLane")
	EFireLaneVerdict Verdict = EFireLaneVerdict::Clear;

	/** Who or what is in the way. Null when the lane is clear. */
	UPROPERTY(BlueprintReadOnly, Category = "FireLane")
	TObjectPtr<AActor> Blocker = nullptr;

	/**
	 * Centimetres from the lane's centre line to the nearest friendly's surface - their collision radius
	 * is already subtracted. Large when nobody is anywhere near.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "FireLane")
	float ClosestApproach = 0.0f;

	/**
	 * A world-space offset from the shooter that would open the lane, or zero when there is nothing to
	 * solve. Horizontal by construction - nobody steps upwards to get a firing angle.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "FireLane")
	FVector SuggestedSidestep = FVector::ZeroVector;

	/**
	 * How long until the blocker walks out of the lane on its own, at its current velocity.
	 *
	 * Negative means it will not: the blocker is standing still, or moving further in. That difference is
	 * the whole reason to return the number - "wait 0.3 s" and "go around" are different orders.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "FireLane")
	float SecondsUntilClear = -1.0f;

	/**
	 * False when the frame's trace budget was already spent and only the friendly test ran.
	 *
	 * The friendly half of the answer is always computed - it is arithmetic and costs nothing. The
	 * geometry half needs a trace, and traces are the thing worth rationing. A Clear verdict with this
	 * flag false means "clear of our people", not "clear of the world".
	 */
	UPROPERTY(BlueprintReadOnly, Category = "FireLane")
	bool bGeometryChecked = false;
};

/** What the lane gate has been doing, for tuning and for the debug overlay. */
USTRUCT(BlueprintType)
struct FIRELANE_API FFireLaneStats
{
	GENERATED_BODY()

	/** Registered actors that count as friendly, across all teams. */
	UPROPERTY(BlueprintReadOnly, Category = "FireLane")
	int32 RegisteredFriendlies = 0;

	/** Lane checks asked for since the level started. */
	UPROPERTY(BlueprintReadOnly, Category = "FireLane")
	int32 ChecksRequested = 0;

	/** Of those, how many came back as a hard block on one of our own. */
	UPROPERTY(BlueprintReadOnly, Category = "FireLane")
	int32 FriendlyBlocks = 0;

	/** Geometry traces actually spent this frame. */
	UPROPERTY(BlueprintReadOnly, Category = "FireLane")
	int32 TracesThisFrame = 0;

	/** Checks that had to answer without a trace because the frame's budget was gone. */
	UPROPERTY(BlueprintReadOnly, Category = "FireLane")
	int32 ChecksWithoutTrace = 0;
};
