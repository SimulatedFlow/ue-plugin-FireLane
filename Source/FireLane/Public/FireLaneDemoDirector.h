// Copyright 2026 Silvan Teufel. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FireLaneTypes.h"
#include "FireLaneDemoDirector.generated.h"

class UTextRenderComponent;

/**
 * Runs the shipped demo: a shooter, a target, and a team-mate who keeps walking through the shot.
 *
 * This is the one plugin in the house whose whole subject is geometry, so the demo draws the geometry:
 * the lane as a real volume, the team-mate crossing it, the verdict changing, and the sidestep the gate
 * suggests. It reads the answer from the same pure rules the component uses, so the picture cannot say
 * one thing while the plugin does another.
 *
 * It ticks in the editor viewport, because that is where the store video is filmed and a Blueprint does
 * not tick there. Assign the three actors from the demo level for a scene with meshes in it; assign
 * nothing and it draws itself, which is what makes it worth dropping into an empty level.
 */
UCLASS(meta = (DisplayName = "FireLane Demo Director"))
class FIRELANE_API AFireLaneDemoDirector : public AActor
{
	GENERATED_BODY()

public:
	AFireLaneDemoDirector();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual bool ShouldTickIfViewportsOnly() const override { return true; }

	/** The shooter. Left empty, the demo draws one. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FireLane Demo")
	TObjectPtr<AActor> ShooterActor;

	/** What it is shooting at. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FireLane Demo")
	TObjectPtr<AActor> TargetActor;

	/** The team-mate who walks through the lane. The demo moves this one. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FireLane Demo")
	TObjectPtr<AActor> AllyActor;

	/** One run of the script. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FireLane Demo",
		meta = (ClampMin = "5.0", ClampMax = "120.0"))
	float CycleSeconds = 18.0f;

	/** How far the team-mate walks either side of the lane. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FireLane Demo",
		meta = (ClampMin = "50.0", ClampMax = "2000.0"))
	float AllyTravel = 320.0f;

	/**
	 * Which way he walks.
	 *
	 * Should be roughly across the lane; walking along it never blocks anything, which is a correct
	 * result and a dull demo.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FireLane Demo")
	FVector AllyAxis = FVector(0.0f, 1.0f, 0.0f);

	/** Draw the lane, the verdict and the board. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FireLane Demo")
	bool bDrawDemo = true;

	/**
	 * Lane radius for the demo only. -1 follows Project Settings.
	 *
	 * A rifle's 55 cm lane over a twenty-metre shot is a hairline on screen - correct, and invisible.
	 * The demo is set up as a rocket launcher instead, where the danger volume is genuinely that wide,
	 * so the picture shows a volume and not a line.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FireLane Demo", meta = (ClampMin = "-1.0"))
	float DemoLaneRadius = -1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FireLane Demo", meta = (ClampMin = "-1.0"))
	float DemoGrazeMargin = -1.0f;

	/**
	 * Longest step the demo will draw. -1 follows Project Settings.
	 *
	 * Needs to scale with the lane: a wide rocket lane needs a bigger step to clear than a rifle one,
	 * and at the shipped 250 cm the demo silently drew no arrow at all.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FireLane Demo", meta = (ClampMin = "-1.0"))
	float DemoMaxSidestep = -1.0f;

	/**
	 * The headline.
	 *
	 * TextRender rather than DrawDebugString: HighResShot renders the scene and not the viewport canvas,
	 * so debug lines survive a screenshot and debug text does not.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FireLane Demo")
	TObjectPtr<UTextRenderComponent> BoardText;

	/** The running verdict, in the verdict's own colour. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FireLane Demo")
	TObjectPtr<UTextRenderComponent> VerdictText;

private:
	/** Where the three parties are this frame, actors or not. */
	FVector ShooterLocation() const;
	FVector TargetLocation() const;
	FVector AllyLocation(float T) const;

	/** The lane test, run exactly the way the component runs it. */
	FFireLaneResult Judge(float T, FVector& OutAlly, FVector& OutFuture) const;

	/**
	 * The teaching line on the board.
	 *
	 * Rotates through statements that are true at EVERY moment of the walk. The first cut keyed them to
	 * the phase of the sine, and the board ended up saying "blocked" over a picture whose verdict read
	 * GRAZING - the lookahead shifts the judgement half a second ahead of the position, which is exactly
	 * the plugin's point and exactly what made a phase-keyed caption lie.
	 */
	FString TeachingLine(float T) const;

	void DrawScene(float T, const FFireLaneResult& Result, const FVector& Ally, const FVector& Future) const;

	float DemoRadius() const;
	float DemoMargin() const;

	/** Fallback positions, used when no actors are assigned. Relative to the director. */
	FVector FallbackShooter = FVector(-450.0f, 0.0f, 90.0f);
	FVector FallbackTarget = FVector(700.0f, 0.0f, 90.0f);

	float CycleTime = 0.0f;

	/** The ally's starting point, captured once so the demo animates around wherever it was placed. */
	FVector AllyHome = FVector::ZeroVector;
	bool bAllyHomeCaptured = false;
};
