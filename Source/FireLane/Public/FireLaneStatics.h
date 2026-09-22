// Copyright 2026 Silvan Teufel. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "FireLaneTypes.h"
#include "FireLaneStatics.generated.h"

class UFireLaneSubsystem;

/**
 * FireLane from Blueprint, and the rules on their own.
 *
 * The second half of this class is the interesting one. Every function below the divider is pure: no
 * world, no actors, no state. The component calls exactly these, and so do the automation tests, which
 * is the only arrangement in which "the tests pass" and "the game behaves" cannot drift apart.
 */
UCLASS(meta = (ScriptName = "FireLaneStatics"))
class FIRELANE_API UFireLaneStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** This world's lane gate. May be null outside a world. */
	UFUNCTION(BlueprintPure, Category = "FireLane", meta = (WorldContext = "WorldContextObject"))
	static UFireLaneSubsystem* GetFireLane(const UObject* WorldContextObject);

	/**
	 * Count an actor as one of ours without giving it a component.
	 *
	 * For the player pawn, for an escorted VIP, for anything that must not be shot but was never going to
	 * carry a FireLaneComponent of its own.
	 */
	UFUNCTION(BlueprintCallable, Category = "FireLane", meta = (WorldContext = "WorldContextObject"))
	static void RegisterFriendly(const UObject* WorldContextObject, AActor* Actor, int32 TeamId);

	/** Stop counting an actor as one of ours. Safe to call for something that was never registered. */
	UFUNCTION(BlueprintCallable, Category = "FireLane", meta = (WorldContext = "WorldContextObject"))
	static void UnregisterFriendly(const UObject* WorldContextObject, AActor* Actor);

	/** Counters, for tuning and for the overlay. */
	UFUNCTION(BlueprintPure, Category = "FireLane", meta = (WorldContext = "WorldContextObject"))
	static FFireLaneStats GetFireLaneStats(const UObject* WorldContextObject);

	// ----------------------------------------------------------------------------------------------------
	// The rules, on their own
	// ----------------------------------------------------------------------------------------------------
	//
	// These are public on purpose. A project that wants the arithmetic without the component - inside an
	// EQS test, a behaviour tree decorator, a gameplay ability - should not have to reimplement it, because
	// a second implementation is a second set of bugs.

	/**
	 * The distance reported for anything that is not in the lane at all.
	 *
	 * A sentinel rather than zero, so that "not in the lane" compares the same way as "miles away" and no
	 * caller has to special-case it.
	 */
	UFUNCTION(BlueprintPure, Category = "FireLane|Rules")
	static float OutOfLaneDistance();

	/** Where something will be in Seconds, at its current velocity. Straight line; that is the point. */
	UFUNCTION(BlueprintPure, Category = "FireLane|Rules")
	static FVector ProjectPosition(const FVector& Location, const FVector& Velocity, float Seconds);

	/**
	 * Distance from Point to the firing lane, and how far along the lane the closest spot lies.
	 *
	 * The lane runs from the muzzle to the target, plus Overshoot for weapons that keep going. Anything
	 * BEHIND the muzzle, or past the end of the lane, is not in the lane and reports OutOfLaneDistance():
	 * a team-mate standing behind the shooter has never blocked a shot, and an implementation that used an
	 * infinite ray - which is what a naive dot product gives you - would hold fire for them anyway.
	 *
	 * @param OutAlong Centimetres from the muzzle to the closest point, measured along the lane.
	 */
	UFUNCTION(BlueprintPure, Category = "FireLane|Rules")
	static float DistanceToLane(const FVector& Point, const FVector& MuzzleLocation,
		const FVector& TargetLocation, float Overshoot, float& OutAlong);

	/**
	 * Turn a distance into a verdict.
	 *
	 * @param ClosestApproach Distance from the lane's centre line to the friendly's SURFACE - subtract
	 *                        their collision radius before calling, or a fat character will be judged as
	 *                        a point and shot through the shoulder.
	 * @param LaneRadius      Half the width of the danger zone: the projectile radius plus whatever margin
	 *                        the weapon deserves.
	 * @param GrazeMargin     The band outside the lane that still counts as uncomfortably close.
	 */
	UFUNCTION(BlueprintPure, Category = "FireLane|Rules")
	static EFireLaneVerdict ClassifyLane(float ClosestApproach, float LaneRadius, float GrazeMargin);

	/**
	 * The sideways step that would open the lane.
	 *
	 * Moving the shooter pivots the lane around the target, so a friendly close to the muzzle needs a small
	 * step and one close to the target needs a large one - which is why this cannot be a fixed "strafe
	 * right by two metres" and why the returned vector gets longer the further down the lane the problem
	 * sits. Horizontal by construction. Returns zero when the lane is already open.
	 *
	 * The caller decides whether the step is affordable; this function will happily ask for ten metres if
	 * that is what the geometry costs, and a component that compares it against a maximum is doing the
	 * right thing.
	 */
	UFUNCTION(BlueprintPure, Category = "FireLane|Rules")
	static FVector SidestepOffset(const FVector& MuzzleLocation, const FVector& TargetLocation,
		const FVector& FriendlyLocation, float LaneRadius, float Clearance);

	/** Is somebody close enough to the barrel that the lane no longer matters? */
	UFUNCTION(BlueprintPure, Category = "FireLane|Rules")
	static bool IsWithinMuzzleClearance(float DistanceToMuzzle, float MuzzleClearance);

	/**
	 * How long until the blocker leaves the lane, at the rate it is currently leaving.
	 *
	 * @param ApproachRate Centimetres per second that the gap is OPENING. Zero or negative means the
	 *                     blocker is standing still or coming further in.
	 * @return Seconds, or -1 when it is not going to happen by itself. The negative answer is the useful
	 *         one: it is the difference between an AI that waits a beat and an AI that waits forever.
	 */
	UFUNCTION(BlueprintPure, Category = "FireLane|Rules")
	static float SecondsUntilLaneClears(float ClosestApproach, float ApproachRate, float LaneRadius);
};
