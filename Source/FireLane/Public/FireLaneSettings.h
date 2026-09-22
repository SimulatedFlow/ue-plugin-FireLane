// Copyright 2026 Silvan Teufel. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Engine/EngineTypes.h"
#include "FireLaneSettings.generated.h"

/**
 * Project Settings > Plugins > FireLane.
 *
 * These are the defaults a new FireLaneComponent starts from. Everything here can also be set per
 * component, because a rocket launcher and a pistol do not deserve the same lane.
 */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "FireLane"))
class FIRELANE_API UFireLaneSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UFireLaneSettings();

	virtual FName GetContainerName() const override { return TEXT("Project"); }
	virtual FName GetCategoryName() const override { return TEXT("Plugins"); }

	/** Convenience accessor; never null after module start-up. */
	static const UFireLaneSettings* Get();

	// ---------------------------------------------------------------------------------------------- lane

	/**
	 * Half the width of the danger zone, in centimetres.
	 *
	 * Not the projectile's visual radius - the radius of the volume you would be unhappy to find a friend
	 * in. For a hitscan rifle 40-60 is sensible; for a rocket it should be the explosion radius, because
	 * that is what actually hurts.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Lane", meta = (ClampMin = "0.0", UIMax = "400.0"))
	float LaneRadius;

	/** The band outside the lane that still counts as uncomfortably close. */
	UPROPERTY(config, EditAnywhere, Category = "Lane", meta = (ClampMin = "0.0", UIMax = "400.0"))
	float GrazeMargin;

	/** Nobody within this distance of the barrel, whatever the lane says. */
	UPROPERTY(config, EditAnywhere, Category = "Lane", meta = (ClampMin = "0.0", UIMax = "400.0"))
	float MuzzleClearance;

	/** How far past the target the lane keeps going. Zero for anything that stops in the target. */
	UPROPERTY(config, EditAnywhere, Category = "Lane", meta = (ClampMin = "0.0", UIMax = "5000.0"))
	float TargetOvershoot;

	// ------------------------------------------------------------------------------------------ movement

	/**
	 * How far ahead friendlies are projected along their own velocity before the lane is judged.
	 *
	 * This is the setting that separates a gate that works from one that reads well in a code review. A
	 * check against where people are RIGHT NOW gives permission to fire at a man who is one stride from
	 * the lane, and he takes that stride while the bullet is in the air. Roughly the flight time of your
	 * fastest common projectile, plus a reaction beat. Zero switches the whole idea off.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Movement", meta = (ClampMin = "0.0", UIMax = "2.0"))
	float LookaheadSeconds;

	/** The furthest sideways step the gate will suggest before it gives up and says go around. */
	UPROPERTY(config, EditAnywhere, Category = "Movement", meta = (ClampMin = "0.0", UIMax = "1000.0"))
	float MaxSidestep;

	/** Extra room the suggested step leaves beyond the bare lane radius, so the AI does not shave it. */
	UPROPERTY(config, EditAnywhere, Category = "Movement", meta = (ClampMin = "0.0", UIMax = "300.0"))
	float SidestepClearance;

	// ------------------------------------------------------------------------------------------- traces

	/** Also ask whether the world is in the way, not only whether a friend is. */
	UPROPERTY(config, EditAnywhere, Category = "Traces")
	bool bCheckGeometry;

	/** The channel the geometry leg of the check traces on. */
	UPROPERTY(config, EditAnywhere, Category = "Traces")
	TEnumAsByte<ECollisionChannel> GeometryChannel;

	/**
	 * How many geometry traces the whole world may spend per frame.
	 *
	 * The friendly test is arithmetic and is never rationed. The trace is the part that scales with the
	 * number of shooters, so it is the part with a ceiling: sixty AI asking every frame is sixty traces,
	 * and at some squad size that is a real cost for a question whose answer barely changes. Checks that
	 * arrive after the budget is gone still get their friendly verdict, flagged as untraced.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Traces", meta = (ClampMin = "0", UIMax = "512"))
	int32 MaxGeometryTracesPerFrame;

	/**
	 * How long a verdict stays usable before the component re-checks.
	 *
	 * A lane does not change meaningfully at 120 Hz. Zero re-checks on every call.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Traces", meta = (ClampMin = "0.0", UIMax = "1.0"))
	float VerdictStaleAfter;

	// -------------------------------------------------------------------------------------------- debug

	/** Draw every lane that is checked. Also switchable live with FireLane.Debug. */
	UPROPERTY(config, EditAnywhere, Category = "Debug")
	bool bDrawDebugLanes;
};
