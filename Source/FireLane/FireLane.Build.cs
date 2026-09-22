// Copyright 2026 Silvan Teufel. All Rights Reserved.

using UnrealBuildTool;

public class FireLane : ModuleRules
{
	public FireLane(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		// One runtime module. The whole plugin is a decision taken in the frame the AI wants to shoot, in
		// the build the player is holding - an editor-only line-of-fire check would be a drawing, not a rule.
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",

			// AActor, UActorComponent, UWorldSubsystem, the collision query for the geometry leg of the
			// check, and DrawDebugHelpers for FireLane.Debug.
			"Engine",

			// UFireLaneSettings is a UDeveloperSettings, so lane radius, lookahead and the trace budget sit
			// under Project Settings > Plugins > FireLane without an editor module.
			"DeveloperSettings",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
		});

		// Deliberately NOT here:
		//   AIModule / NavigationSystem - FireLane answers "may I shoot" and "which way would I step". It
		//                                 does not move anybody and does not own a behaviour tree, so it
		//                                 must not force a project into one. The suggested sidestep is a
		//                                 vector; what walks it is the project's business.
		//   GameplayAbilities          - the same argument. A team id here is an int, not an attribute set.
	}
}
