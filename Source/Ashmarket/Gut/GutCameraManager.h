// Ashmarket prototype: the Gut.

#pragma once

#include "CoreMinimal.h"
#include "Camera/PlayerCameraManager.h"
#include "GutCameraManager.generated.h"

/**
 *  The player's view on screens wider than 16:9.
 *
 *  The engine keeps a camera's view from top to bottom whatever the screen, and opens it to the sides: a camera framed at
 *  90 degrees on a 16:9 screen is 127 degrees wide on a 32:9 one. Drawn flat, anybody standing toward the side of such a
 *  view is more than twice as broad as they are tall, and reads as squat.
 *
 *  Here the picture is drawn through the engine's panini projection, which keeps upright things upright and takes the
 *  sideways stretch out, and the view is chosen so that the middle of the picture is the size it was framed at. Where that
 *  would ask for a wider view than the widest allowed, the middle comes out larger, and a camera on a boom is drawn back
 *  to make up for it.
 */
UCLASS()
class AGutCameraManager : public APlayerCameraManager
{
	GENERATED_BODY()

public:

	/** The widest the view is ever opened, in degrees from side to side */
	UPROPERTY(EditAnywhere, Category="Wide screen", meta=(ClampMin=90, ClampMax=138))
	float WidestView = 124.0f;

	/** How much of the engine's panini projection is used on a screen twice as wide as 16:9: 0 none, 1 all of it */
	UPROPERTY(EditAnywhere, Category="Wide screen", meta=(ClampMin=0, ClampMax=1))
	float Panini = 0.5f;

	/** The farthest a camera on a boom is drawn back to make up for a middle that came out larger, as a share of the boom */
	UPROPERTY(EditAnywhere, Category="Wide screen", meta=(ClampMin=0, ClampMax=1))
	float MostDrawnBack = 0.45f;

	/** Where across a panini picture something at the given angle from straight ahead is drawn, in the units of a flat picture's tangent */
	static float PaniniAcross(float Angle, float D);

	/**
	 *  Works out the view for a screen of the given shape.
	 *  @param FramedView		the camera's own view, as framed for a 16:9 screen, in degrees
	 *  @param OutView			the view to give the engine in its place, in the same terms
	 *  @param OutPanini		how much panini projection to draw it through
	 *  @param OutLarger		how much larger than framed the middle of the picture comes out: 1 when it is as framed
	 */
	void GetWideView(float FramedView, float Aspect, float& OutView, float& OutPanini, float& OutLarger) const;

	/**
	 *  A blow has landed near whoever is watching: the picture is jarred for a moment, the harder the blow the more.
	 *  @param Strength  0 to 1: a jab is about 0.3, the heaviest blow in the game 1
	 */
	void Jar(float Strength);

	/** Jars the picture of whoever is watching the given world, if anybody is */
	static void JarViewers(const UObject* WorldContext, float Strength);

	/** The most the picture is turned by the heaviest blow, in degrees, and how fast it settles */
	UPROPERTY(EditAnywhere, Category="Blows", meta=(ClampMin=0, ClampMax=4))
	float MostJarred = 0.9f;

	UPROPERTY(EditAnywhere, Category="Blows", meta=(ClampMin=1, ClampMax=40))
	float JarSettles = 9.0f;

	/** Nearer the followed body's chest than this, the camera begins to go up over it; by a little under half of this it is all the way up */
	UPROPERTY(EditAnywhere, Category="Close", meta=(ClampMin=50, ClampMax=400))
	float CloseBy = 210.0f;

	/** How near the camera comes to somebody other than the one it follows before they are left out of the picture */
	UPROPERTY(EditAnywhere, Category="Gut|Camera", meta = (ClampMin = 0, Units = "cm"))
	float LeftOutWithin = 115.0f;

	/** How far up it goes, and how far down it tilts to keep the body in the picture */
	UPROPERTY(EditAnywhere, Category="Close", meta=(ClampMin=0, ClampMax=200))
	float RaisedWhenClose = 80.0f;

	UPROPERTY(EditAnywhere, Category="Close", meta=(ClampMin=0, ClampMax=60))
	float TiltedWhenClose = 24.0f;

protected:

	virtual void UpdateViewTarget(FTViewTarget& OutVT, float DeltaTime) override;

	/** How much of the way up over the followed body the camera is, 0 to 1 */
	float Crowded = 0.0f;
	bool bAnyLeftOut = false;

	/** How jarred the picture is now, 0 to 1, and how long it has been shaking */
	float Jarred = 0.0f;
	float JarTime = 0.0f;

	/** What the panini projection was last set to, so that it is only set when it changes */
	float PaniniSet = -1.0f;
};
