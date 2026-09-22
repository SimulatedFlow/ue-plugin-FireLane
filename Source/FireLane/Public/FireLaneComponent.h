// Copyright 2026 Silvan Teufel. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "FireLaneTypes.h"
#include "FireLaneComponent.generated.h"

class USceneComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnFireLaneBlocked,
	AActor*, Blocker, FVector, SuggestedSidestep, float, SecondsUntilClear);

/**
 * Put this on anything that shoots.
 *
 * It answers one question - may I fire at that - and answers it with enough detail that the AI can do
 * something other than stand still. The component holds no navigation, no behaviour tree, no weapon: it
 * is a gate in front of whatever you already have.
 */
UCLASS(ClassGroup = (FireLane), meta = (BlueprintSpawnableComponent, DisplayName = "Fire Lane"))
class FIRELANE_API UFireLaneComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UFireLaneComponent();

	// ------------------------------------------------------------------------------------------ asking

	/** The full answer for a shot at an actor. */
	UFUNCTION(BlueprintCallable, Category = "FireLane")
	FFireLaneResult CheckLane(AActor* TargetActor);

	/** The same, for a point - a suspected position, a grenade arc's landing spot, a marked door. */
	UFUNCTION(BlueprintCallable, Category = "FireLane")
	FFireLaneResult CheckLaneToLocation(const FVector& TargetLocation);

	/**
	 * The one-pin version for a behaviour tree.
	 *
	 * Grazing counts as permission only when bAllowGrazingShots is on, which is the switch between a
	 * careful squad and a decisive one.
	 */
	UFUNCTION(BlueprintCallable, Category = "FireLane")
	bool CanFire(AActor* TargetActor);

	/** The last answer, without re-checking. */
	UFUNCTION(BlueprintPure, Category = "FireLane")
	FFireLaneResult GetLastResult() const { return LastResult; }

	// ----------------------------------------------------------------------------------------- muzzle

	/**
	 * Where the shot starts.
	 *
	 * Without this the lane starts at the actor's origin, which for a character is between the feet - and
	 * a lane from the floor is blocked by every kerb in the level.
	 */
	UFUNCTION(BlueprintCallable, Category = "FireLane")
	void SetMuzzle(USceneComponent* Component, FName SocketName);

	/** The muzzle position, falling back to the owner's location plus MuzzleOffset. */
	UFUNCTION(BlueprintPure, Category = "FireLane")
	FVector GetMuzzleLocation() const;

	// ------------------------------------------------------------------------------------------- team

	/** Who this shooter will not shoot. Changing it re-registers with the subsystem. */
	UFUNCTION(BlueprintCallable, Category = "FireLane")
	void SetTeamId(int32 NewTeamId);

	UFUNCTION(BlueprintPure, Category = "FireLane")
	int32 GetTeamId() const { return TeamId; }

	// ----------------------------------------------------------------------------------------- events

	/** Fires when a check that would otherwise have been Clear came back blocked by one of ours. */
	UPROPERTY(BlueprintAssignable, Category = "FireLane")
	FOnFireLaneBlocked OnBlocked;

	// ----------------------------------------------------------------------------------------- config

	/** Shooters and friendlies sharing this id will not shoot each other. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FireLane")
	int32 TeamId = 0;

	/** Also register the owner as something others must not shoot. Normally yes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FireLane")
	bool bRegisterOwnerAsFriendly = true;

	/** Used when no muzzle component is set. Owner space. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FireLane")
	FVector MuzzleOffset = FVector(0.0f, 0.0f, 60.0f);

	/** Treat a graze as permission. Off by default: careful is the safer default for a shipped game. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FireLane")
	bool bAllowGrazingShots = false;

	/**
	 * Per-component overrides. Left at -1 they follow Project Settings > Plugins > FireLane, which is how
	 * a project tunes once and how a rocket launcher opts out of that tuning.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FireLane|Overrides", meta = (ClampMin = "-1.0"))
	float LaneRadiusOverride = -1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FireLane|Overrides", meta = (ClampMin = "-1.0"))
	float GrazeMarginOverride = -1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FireLane|Overrides", meta = (ClampMin = "-1.0"))
	float LookaheadSecondsOverride = -1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FireLane|Overrides", meta = (ClampMin = "-1.0"))
	float TargetOvershootOverride = -1.0f;

	// -------------------------------------------------------------------------------- UActorComponent

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	/** The whole check, shared by both public entry points. */
	FFireLaneResult Evaluate(AActor* TargetActor, const FVector& TargetLocation);

	/** Settings with the per-component overrides already applied. */
	float EffectiveLaneRadius() const;
	float EffectiveGrazeMargin() const;
	float EffectiveLookahead() const;
	float EffectiveOvershoot() const;

	UPROPERTY()
	TWeakObjectPtr<USceneComponent> MuzzleComponent;

	FName MuzzleSocket = NAME_None;

	FFireLaneResult LastResult;
	double LastCheckTime = -1.0;
	TWeakObjectPtr<AActor> LastCheckTarget;
	FVector LastCheckLocation = FVector::ZeroVector;
};
