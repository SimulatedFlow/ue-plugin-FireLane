// Copyright 2026 Silvan Teufel. All Rights Reserved.

#include "FireLane.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "FireLaneLog.h"
#include "FireLaneStatics.h"
#include "FireLaneSubsystem.h"
#include "HAL/IConsoleManager.h"

DEFINE_LOG_CATEGORY(LogFireLane);

#define LOCTEXT_NAMESPACE "FFireLaneModule"

namespace
{
	/** The world a console command should talk to: the one the player is looking at. */
	UWorld* FireLaneConsoleWorld()
	{
		if (!GEngine)
		{
			return nullptr;
		}
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.World() && (Context.WorldType == EWorldType::PIE || Context.WorldType == EWorldType::Game))
			{
				return Context.World();
			}
		}
		return nullptr;
	}

	void FireLaneDebugCommand(const TArray<FString>& Args)
	{
		UWorld* World = FireLaneConsoleWorld();
		UFireLaneSubsystem* Lane = World ? World->GetSubsystem<UFireLaneSubsystem>() : nullptr;
		if (!Lane)
		{
			UE_LOG(LogFireLane, Warning, TEXT("FireLane.Debug: no running world."));
			return;
		}

		Lane->bDebugDraw = Args.Num() > 0 ? (FCString::Atoi(*Args[0]) != 0) : !Lane->bDebugDraw;
		UE_LOG(LogFireLane, Display, TEXT("FireLane.Debug: %s"),
			Lane->bDebugDraw ? TEXT("on") : TEXT("off"));
	}

	void FireLaneDumpCommand()
	{
		UWorld* World = FireLaneConsoleWorld();
		const UFireLaneSubsystem* Lane = World ? World->GetSubsystem<UFireLaneSubsystem>() : nullptr;
		if (!Lane)
		{
			UE_LOG(LogFireLane, Warning, TEXT("FireLane.Dump: no running world."));
			return;
		}

		const FFireLaneStats Stats = Lane->GetStats();
		UE_LOG(LogFireLane, Display, TEXT("FireLane: %d friendlies registered."), Stats.RegisteredFriendlies);
		UE_LOG(LogFireLane, Display, TEXT("  checks %d, of which %d were blocked by one of ours (%.1f%%)"),
			Stats.ChecksRequested, Stats.FriendlyBlocks,
			Stats.ChecksRequested > 0 ? 100.0f * Stats.FriendlyBlocks / Stats.ChecksRequested : 0.0f);

		// The number worth watching. A high count here means the trace budget is too small for the squad
		// size, and Clear verdicts are being handed out without ever looking at the level geometry.
		UE_LOG(LogFireLane, Display, TEXT("  %d checks answered without a geometry trace (budget spent)"),
			Stats.ChecksWithoutTrace);
	}

	FAutoConsoleCommand GFireLaneDebug(
		TEXT("FireLane.Debug"),
		TEXT("Draw every firing lane that gets checked. FireLane.Debug [0|1]"),
		FConsoleCommandWithArgsDelegate::CreateStatic(&FireLaneDebugCommand));

	FAutoConsoleCommand GFireLaneDump(
		TEXT("FireLane.Dump"),
		TEXT("Log how many lane checks ran, how many were blocked, and how many missed the trace budget."),
		FConsoleCommandDelegate::CreateStatic(&FireLaneDumpCommand));
}

void FFireLaneModule::StartupModule()
{
}

void FFireLaneModule::ShutdownModule()
{
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FFireLaneModule, FireLane)
