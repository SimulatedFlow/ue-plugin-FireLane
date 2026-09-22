// Copyright 2026 Silvan Teufel. All Rights Reserved.

#include "FireLaneSettings.h"

UFireLaneSettings::UFireLaneSettings()
	// A rifle round through a shoulder-width lane, with a hand's width of margin on top.
	: LaneRadius(55.0f)
	, GrazeMargin(45.0f)
	, MuzzleClearance(90.0f)
	, TargetOvershoot(0.0f)
	// A third of a second. Long enough to catch the stride somebody is already taking, short enough that
	// a squad does not refuse to shoot at anything that moves in the same postcode.
	, LookaheadSeconds(0.35f)
	, MaxSidestep(250.0f)
	, SidestepClearance(30.0f)
	, bCheckGeometry(true)
	, GeometryChannel(ECC_Visibility)
	, MaxGeometryTracesPerFrame(32)
	, VerdictStaleAfter(0.1f)
	, bDrawDebugLanes(false)
{
}

const UFireLaneSettings* UFireLaneSettings::Get()
{
	const UFireLaneSettings* Settings = GetDefault<UFireLaneSettings>();
	check(Settings);
	return Settings;
}
