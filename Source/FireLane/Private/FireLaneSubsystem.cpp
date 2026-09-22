// Copyright 2026 Silvan Teufel. All Rights Reserved.

#include "FireLaneSubsystem.h"

#include "Engine/World.h"
#include "FireLaneLog.h"
#include "FireLaneSettings.h"
#include "GameFramework/Actor.h"

void UFireLaneSubsystem::RegisterFriendly(AActor* Actor, int32 TeamId)
{
	if (!IsValid(Actor))
	{
		return;
	}

	PruneDeadRegistrations();

	// Re-registering is an update, not a duplicate. A pawn that switches sides mid-match is a real thing,
	// and two entries for one actor would make it both shootable and not.
	for (FFireLaneFriendly& Existing : Friendlies)
	{
		if (Existing.Actor.Get() == Actor)
		{
			Existing.TeamId = TeamId;
			Existing.Radius = Actor->GetSimpleCollisionRadius();
			return;
		}
	}

	FFireLaneFriendly Entry;
	Entry.Actor = Actor;
	Entry.TeamId = TeamId;

	// Cached once. The lane test must stay arithmetic, and asking an actor for its bounds sixty times a
	// frame is how a cheap check turns into a profile entry.
	Entry.Radius = Actor->GetSimpleCollisionRadius();
	Friendlies.Add(Entry);
}

void UFireLaneSubsystem::UnregisterFriendly(AActor* Actor)
{
	Friendlies.RemoveAll([Actor](const FFireLaneFriendly& Entry)
	{
		return !Entry.Actor.IsValid() || Entry.Actor.Get() == Actor;
	});
}

void UFireLaneSubsystem::GatherFriendlies(int32 TeamId, const AActor* Ignore,
	TArray<FFireLaneFriendly>& Out) const
{
	PruneDeadRegistrations();

	Out.Reset();
	for (const FFireLaneFriendly& Entry : Friendlies)
	{
		const AActor* Actor = Entry.Actor.Get();
		if (!Actor || Actor == Ignore || Entry.TeamId != TeamId)
		{
			continue;
		}
		Out.Add(Entry);
	}
}

bool UFireLaneSubsystem::ClaimTrace()
{
	RefreshFrame();

	const int32 Budget = UFireLaneSettings::Get()->MaxGeometryTracesPerFrame;
	if (Budget <= 0 || TracesThisFrame >= Budget)
	{
		return false;
	}

	++TracesThisFrame;
	return true;
}

void UFireLaneSubsystem::NoteCheck(bool bFriendlyBlock, bool bHadTrace)
{
	++ChecksRequested;
	if (bFriendlyBlock)
	{
		++FriendlyBlocks;
	}
	if (!bHadTrace)
	{
		++ChecksWithoutTrace;
	}
}

FFireLaneStats UFireLaneSubsystem::GetStats() const
{
	PruneDeadRegistrations();

	FFireLaneStats Stats;
	Stats.RegisteredFriendlies = Friendlies.Num();
	Stats.ChecksRequested = ChecksRequested;
	Stats.FriendlyBlocks = FriendlyBlocks;
	Stats.TracesThisFrame = TracesThisFrame;
	Stats.ChecksWithoutTrace = ChecksWithoutTrace;
	return Stats;
}

void UFireLaneSubsystem::Deinitialize()
{
	Friendlies.Reset();
	Super::Deinitialize();
}

void UFireLaneSubsystem::PruneDeadRegistrations() const
{
	Friendlies.RemoveAll([](const FFireLaneFriendly& Entry)
	{
		return !Entry.Actor.IsValid();
	});
}

void UFireLaneSubsystem::RefreshFrame()
{
	const UWorld* World = GetWorld();
	const uint64 Frame = World ? GFrameCounter : 0;
	if (Frame != BudgetFrame)
	{
		BudgetFrame = Frame;
		TracesThisFrame = 0;
	}
}
