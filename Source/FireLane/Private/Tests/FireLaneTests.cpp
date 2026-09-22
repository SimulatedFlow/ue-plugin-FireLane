// Copyright 2026 Silvan Teufel. All Rights Reserved.

#include "Misc/AutomationTest.h"

#include "FireLaneStatics.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace FireLaneTests
{
	constexpr EAutomationTestFlags TestFlags = EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::CommandletContext
		| EAutomationTestFlags::EngineFilter;

	// A lane straight down +X, ten metres long, at chest height.
	const FVector Muzzle(0.0f, 0.0f, 150.0f);
	const FVector Target(1000.0f, 0.0f, 150.0f);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFireLaneOnlyWhatIsInFront,
	"FireLane.Lane.OnlyWhatIsInFrontCounts", FireLaneTests::TestFlags)

bool FFireLaneOnlyWhatIsInFront::RunTest(const FString&)
{
	using namespace FireLaneTests;
	float Along = 0.0f;

	TestNearlyEqual(TEXT("dead centre of the lane"),
		UFireLaneStatics::DistanceToLane(FVector(500.0f, 0.0f, 150.0f), Muzzle, Target, 0.0f, Along),
		0.0f, 0.01f);
	TestNearlyEqual(TEXT("and it reports how far along"), Along, 500.0f, 0.01f);

	TestNearlyEqual(TEXT("a metre to the side, halfway down"),
		UFireLaneStatics::DistanceToLane(FVector(500.0f, 100.0f, 150.0f), Muzzle, Target, 0.0f, Along),
		100.0f, 0.01f);

	// THE ONE EVERY HAND-ROLLED VERSION GETS WRONG. A dot product against an infinite ray says the man
	// standing half a metre behind the shooter is on the firing line. He is not, he never has been, and a
	// gate that thinks so holds fire whenever the squad bunches up behind its own gunner.
	TestEqual(TEXT("behind the barrel is not in the lane"),
		UFireLaneStatics::DistanceToLane(FVector(-50.0f, 0.0f, 150.0f), Muzzle, Target, 0.0f, Along),
		UFireLaneStatics::OutOfLaneDistance());
	TestTrue(TEXT("and the along value says why"), Along < 0.0f);

	TestEqual(TEXT("past the target is not in the lane either"),
		UFireLaneStatics::DistanceToLane(FVector(1200.0f, 0.0f, 150.0f), Muzzle, Target, 0.0f, Along),
		UFireLaneStatics::OutOfLaneDistance());

	// ...unless the weapon keeps going, which is what the overshoot is for.
	TestNearlyEqual(TEXT("with overshoot, past the target counts again"),
		UFireLaneStatics::DistanceToLane(FVector(1200.0f, 0.0f, 150.0f), Muzzle, Target, 500.0f, Along),
		0.0f, 0.01f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFireLaneClassificationEdges,
	"FireLane.Lane.EdgeOfTheDangerVolumeIsInIt", FireLaneTests::TestFlags)

bool FFireLaneClassificationEdges::RunTest(const FString&)
{
	// Exactly at the radius is INSIDE. Rounding this the other way is a decision to shoot somebody
	// occasionally, and it will not look like a rounding bug when it happens - it will look like the AI
	// being malicious.
	TestEqual(TEXT("exactly at the radius is blocked"),
		UFireLaneStatics::ClassifyLane(55.0f, 55.0f, 45.0f), EFireLaneVerdict::BlockedByFriendly);

	TestEqual(TEXT("a hair outside is only a graze"),
		UFireLaneStatics::ClassifyLane(55.1f, 55.0f, 45.0f), EFireLaneVerdict::Grazing);

	TestEqual(TEXT("at the end of the graze band, still a graze"),
		UFireLaneStatics::ClassifyLane(100.0f, 55.0f, 45.0f), EFireLaneVerdict::Grazing);

	TestEqual(TEXT("beyond it, fire"),
		UFireLaneStatics::ClassifyLane(100.1f, 55.0f, 45.0f), EFireLaneVerdict::Clear);

	// No graze band configured collapses to two states, and must not accidentally widen the danger zone.
	TestEqual(TEXT("no graze band means clear right outside the radius"),
		UFireLaneStatics::ClassifyLane(55.1f, 55.0f, 0.0f), EFireLaneVerdict::Clear);

	// Nobody anywhere near reads as clear without the caller special-casing the sentinel.
	TestEqual(TEXT("the out-of-lane sentinel is clear"),
		UFireLaneStatics::ClassifyLane(UFireLaneStatics::OutOfLaneDistance(), 55.0f, 45.0f),
		EFireLaneVerdict::Clear);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFireLaneLookaheadCatchesTheStride,
	"FireLane.Movement.LookaheadCatchesTheStride", FireLaneTests::TestFlags)

bool FFireLaneLookaheadCatchesTheStride::RunTest(const FString&)
{
	using namespace FireLaneTests;
	float Along = 0.0f;

	// A team-mate halfway down the lane, two metres to the side, walking towards it at 4 m/s.
	const FVector Here(500.0f, 200.0f, 150.0f);
	const FVector Walking(0.0f, -400.0f, 0.0f);

	const float Now = UFireLaneStatics::DistanceToLane(Here, Muzzle, Target, 0.0f, Along);
	TestEqual(TEXT("right now he is clear"),
		UFireLaneStatics::ClassifyLane(Now, 55.0f, 45.0f), EFireLaneVerdict::Clear);

	// THE POINT OF THE PLUGIN. Judged where he is, the shot is allowed. Judged where he will be when the
	// bullet gets there, it is not - and he takes that stride either way.
	const FVector Soon = UFireLaneStatics::ProjectPosition(Here, Walking, 0.5f);
	const float Later = UFireLaneStatics::DistanceToLane(Soon, Muzzle, Target, 0.0f, Along);
	TestEqual(TEXT("half a second later he is in it"),
		UFireLaneStatics::ClassifyLane(Later, 55.0f, 45.0f), EFireLaneVerdict::BlockedByFriendly);

	// A negative lookahead would judge the shot against ground he has already left. Clamped, not obeyed.
	TestTrue(TEXT("a negative lookahead does not rewind him"),
		UFireLaneStatics::ProjectPosition(Here, Walking, -1.0f).Equals(Here, 0.01f));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFireLaneSidestepActuallyClearsTheLane,
	"FireLane.Sidestep.TheSuggestedStepActuallyWorks", FireLaneTests::TestFlags)

bool FFireLaneSidestepActuallyClearsTheLane::RunTest(const FString&)
{
	using namespace FireLaneTests;
	constexpr float Radius = 55.0f;
	constexpr float Clearance = 30.0f;

	// Somebody standing in the lane, a quarter of the way down, slightly to the left.
	const FVector Friendly(250.0f, 20.0f, 150.0f);

	const FVector Step = UFireLaneStatics::SidestepOffset(Muzzle, Target, Friendly, Radius, Clearance);

	TestTrue(TEXT("the step is not nothing"), !Step.IsNearlyZero());
	// Double literals, not float: FVector is double-precision in UE5 and the mixed overload is ambiguous.
	TestNearlyEqual(TEXT("and it is taken on the ground, not in the air"), Step.Z, 0.0, 0.001);
	TestTrue(TEXT("and it goes away from him, not through him"), Step.Y < 0.0f);

	// A property, not a number: take the suggested step and the lane must genuinely be open. This is the
	// assertion that survives any future change to how the offset is derived - an implementation that
	// returns a plausible-looking vector which does not actually clear the lane fails here and nowhere
	// else.
	float Along = 0.0f;
	const FVector NewMuzzle = Muzzle + Step;
	const float After = UFireLaneStatics::DistanceToLane(Friendly, NewMuzzle, Target, 0.0f, Along);
	TestEqual(TEXT("after stepping, the lane is clear"),
		UFireLaneStatics::ClassifyLane(After, Radius, 0.0f), EFireLaneVerdict::Clear);

	// Already clear means there is nothing to suggest, and saying so with a zero vector is what lets a
	// caller test the result instead of comparing it against a tolerance.
	TestTrue(TEXT("a friendly already clear gets no suggestion"),
		UFireLaneStatics::SidestepOffset(Muzzle, Target, FVector(250.0f, 600.0f, 150.0f),
			Radius, Clearance).IsNearlyZero());

	// Further down the lane needs a bigger step, because the lane pivots about the target. A fixed strafe
	// distance - which is what everyone writes first - clears the near case and not the far one.
	const FVector Near = UFireLaneStatics::SidestepOffset(Muzzle, Target,
		FVector(200.0f, 0.0f, 150.0f), Radius, Clearance);
	const FVector Far = UFireLaneStatics::SidestepOffset(Muzzle, Target,
		FVector(800.0f, 0.0f, 150.0f), Radius, Clearance);
	TestTrue(TEXT("the far blocker costs a longer step"), Far.Size() > Near.Size());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFireLaneWaitingOrMoving,
	"FireLane.Timing.WaitingIsOnlyWorthItWhenHeIsLeaving", FireLaneTests::TestFlags)

bool FFireLaneWaitingOrMoving::RunTest(const FString&)
{
	// He is in the lane and walking out of it: 30 cm to go at 60 cm/s.
	TestNearlyEqual(TEXT("walking out"),
		UFireLaneStatics::SecondsUntilLaneClears(25.0f, 60.0f, 55.0f), 0.5f, 0.01f);

	// Standing still. "Never" has to be sayable, or the AI holds a trigger it will not pull this decade.
	TestTrue(TEXT("standing still never clears"),
		UFireLaneStatics::SecondsUntilLaneClears(25.0f, 0.0f, 55.0f) < 0.0f);

	TestTrue(TEXT("walking further in never clears either"),
		UFireLaneStatics::SecondsUntilLaneClears(25.0f, -40.0f, 55.0f) < 0.0f);

	TestNearlyEqual(TEXT("already out takes no time at all"),
		UFireLaneStatics::SecondsUntilLaneClears(80.0f, 0.0f, 55.0f), 0.0f, 0.001f);

	// A clearance of zero switches the muzzle rule off instead of making every position a violation.
	TestFalse(TEXT("zero clearance disables the muzzle rule"),
		UFireLaneStatics::IsWithinMuzzleClearance(0.0f, 0.0f));
	TestTrue(TEXT("inside the clearance"),
		UFireLaneStatics::IsWithinMuzzleClearance(50.0f, 90.0f));
	TestFalse(TEXT("outside it"),
		UFireLaneStatics::IsWithinMuzzleClearance(150.0f, 90.0f));

	return true;
}

#endif  // WITH_DEV_AUTOMATION_TESTS
