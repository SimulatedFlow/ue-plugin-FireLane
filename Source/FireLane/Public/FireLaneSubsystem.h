// Copyright 2026 Silvan Teufel. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "FireLaneTypes.h"
#include "FireLaneSubsystem.generated.h"

class UFireLaneComponent;

/** One registered actor that must not be shot. */
USTRUCT()
struct FFireLaneFriendly
{
	GENERATED_BODY()

	UPROPERTY()
	TWeakObjectPtr<AActor> Actor;

	UPROPERTY()
	int32 TeamId = 0;

	/** Cached so the lane test stays arithmetic; refreshed when the registration is made. */
	UPROPERTY()
	float Radius = 40.0f;
};

/**
 * The registry of who counts as ours, and the frame's trace budget.
 *
 * Deliberately not a tickable subsystem. It has nothing to do between checks; the only per-frame state
 * is the trace counter, and that is reset lazily when the first check of a new frame arrives. A tick
 * that exists only to zero an integer is a tick that shows up in a profile for no reason.
 */
UCLASS()
class FIRELANE_API UFireLaneSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	// ------------------------------------------------------------------------------------ registration

	/** Count an actor as ours. Registering the same actor twice updates it rather than duplicating it. */
	void RegisterFriendly(AActor* Actor, int32 TeamId);

	/** Stop counting an actor as ours. */
	void UnregisterFriendly(AActor* Actor);

	/** Every registered actor on a team, minus one to ignore - normally the shooter itself. */
	void GatherFriendlies(int32 TeamId, const AActor* Ignore, TArray<FFireLaneFriendly>& Out) const;

	// ------------------------------------------------------------------------------------ trace budget

	/**
	 * Ask for one geometry trace this frame.
	 *
	 * @return False when the frame's budget is spent, in which case the caller answers without it and says
	 *         so. Refusing the trace is the honest failure: the alternative is a hidden cost that grows
	 *         with squad size and shows up as a frame spike nobody can attribute.
	 */
	bool ClaimTrace();

	/** Counters for the overlay and for tuning. */
	UFUNCTION(BlueprintPure, Category = "FireLane")
	FFireLaneStats GetStats() const;

	/** Bookkeeping from the components, so the stats mean something. */
	void NoteCheck(bool bFriendlyBlock, bool bHadTrace);

	// --------------------------------------------------------------------------------------- debug

	/** FireLane.Debug - draw every lane that gets checked. */
	bool bDebugDraw = false;

	// --------------------------------------------------------------------------------- UWorldSubsystem

	virtual void Deinitialize() override;

private:
	/** Drops registrations whose actor has gone away. Called whenever the list is read. */
	void PruneDeadRegistrations() const;

	/** Rolls the trace counter over when a new frame has started. */
	void RefreshFrame();

	mutable TArray<FFireLaneFriendly> Friendlies;

	uint64 BudgetFrame = 0;
	int32 TracesThisFrame = 0;

	int32 ChecksRequested = 0;
	int32 FriendlyBlocks = 0;
	int32 ChecksWithoutTrace = 0;
};
