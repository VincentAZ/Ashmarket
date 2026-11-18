// Ashmarket prototype: the Gut.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "Fonts/SlateFontInfo.h"
#include "GutHUD.generated.h"

/** The colours every screen is drawn in */
namespace GutHUDColors
{
	inline const FLinearColor Backing(0.0f, 0.0f, 0.0f, 0.6f);
	inline const FLinearColor Health(0.75f, 0.08f, 0.06f);
	inline const FLinearColor Stamina(0.25f, 0.7f, 0.2f);
	inline const FLinearColor StaminaRefused(1.0f, 0.85f, 0.1f);
	inline const FLinearColor Text(0.92f, 0.9f, 0.85f);
	inline const FLinearColor DimText(0.92f, 0.9f, 0.85f, 0.7f);
	inline const FLinearColor Warning(1.0f, 0.55f, 0.1f);

	inline const FLinearColor Body(0.55f, 0.55f, 0.52f);
	inline const FLinearColor LimbHealthy(0.75f, 0.74f, 0.7f);
	inline const FLinearColor LimbHurt(0.95f, 0.8f, 0.2f);
	inline const FLinearColor LimbBad(1.0f, 0.45f, 0.1f);
	inline const FLinearColor LimbCrippled(0.45f, 0.03f, 0.03f);

	inline const FLinearColor Card(0.0f, 0.0f, 0.0f, 0.72f);
	inline const FLinearColor CardBar(0.8f, 0.78f, 0.72f);
	inline const FLinearColor CardBarBacking(1.0f, 1.0f, 1.0f, 0.12f);
}

class AGutFighter;
class AGutGameMode;
class UGutWeaponData;
class UTexture2D;
struct FCompositeFont;

/** What a screen of text is drawn on: photographs of surfaces, made in Mixar */
enum class EGutPanel : uint8
{
	/** Scratched black steel plate: the fight card, the gate, the owner's yard */
	Steel,

	/** A wiped slate: everything to do with the betting window */
	Slate,

	/** A hoarding with the remains of old posters: the street outside */
	Hoarding
};

/**
 *  Grey-box HUD for a cage fight, drawn straight to the canvas:
 *  the weapon rack, health, stamina, weapon and limbs for both fighters, the bell, the fight clock and round,
 *  warnings of hazards to come, the finisher prompt, the result, the lock-on marker, and labels over the fighters' heads.
 *  On a ladder it also draws the card before each fight, the betting window, what each fight paid, and the bill at the gate.
 */
UCLASS()
class AGutHUD : public AHUD
{
	GENERATED_BODY()

protected:

	/**
	 *  If true, each fighter's current action is written over its head. It stood in for readable animation while the fighters were
	 *  placeholders; now that every action has a captured clip it is off, and -GutStateLabels on the command line brings it back.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gut")
	bool bShowStateLabels = false;

	/** If true, the controls are listed on screen */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gut")
	bool bShowControls = true;

	/** Lines of the controls list. Each line is an action and its inputs, separated by a | character. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gut")
	TArray<FString> ControlsHelp;

	/** Time a damage number or other text stays over a fighter's head */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gut", meta = (ClampMin = 0, ClampMax = 10, Units = "s"))
	float CombatTextDuration = 0.9f;

	/** Time the news that a hazard has gone live stays on screen */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gut", meta = (ClampMin = 0, ClampMax = 10, Units = "s"))
	float HazardNewsDuration = 2.0f;

	/** Scale from the 1080p layout to the current viewport */
	float UIScale = 1.0f;

	/** The last panel drawn, in screen pixels. What is written on a panel is kept inside it. */
	FBox2D LastPanel = FBox2D(ForceInit);

	/** What has been reported as too long for its place: each once, with -GutTextLog on the command line */
	TSet<FString> ReportedTooLong;

public:

	/** Constructor */
	AGutHUD();

	/** Draws the HUD */
	virtual void DrawHUD() override;

protected:

	/** Draws the weapon rack: a card for every weapon on it, who has drawn which, and the inputs */
	void DrawRack(const AGutGameMode* GameMode, float CenterX);

	/** Draws one weapon's card at the rack, with its numbers as bars measured against the best on the rack. Position is in 1080p layout units. */
	void DrawWeaponCard(const AGutGameMode* GameMode, const UGutWeaponData* Weapon, float X, float Y, float Width, float Height, bool bPlayers, bool bEnemys);

	/** Draws the card before a ladder fight: the story so far, the opponent, the state the player is in, and the cutter's offer */
	void DrawCard(const AGutGameMode* GameMode, float CenterX);

	/** Draws the betting window: what the book thinks, what each side pays, and the bet the player is about to place */
	void DrawBettingWindow(const AGutGameMode* GameMode, float CenterX);

	/** Draws what a ladder fight came to in water, under the verdict, a line for each thing that changed hands. Returns where the lines ended. */
	float DrawSettlement(const AGutGameMode* GameMode, float CenterX, float Y);

	/** Draws the gate at the end of a ladder: how it ended, and the bill */
	void DrawGate(const AGutGameMode* GameMode, float CenterX);

	/** For an owner: the card for the day, a page at a time, and what a fight came to */
	void DrawStableCard(const AGutGameMode* GameMode, float CenterX);

	/** Outside the Gut: the ways in */
	void DrawDoor(const AGutGameMode* GameMode, float CenterX);
	float DrawStableResult(const AGutGameMode* GameMode, float CenterX);

	/** For somebody who came to bet: the card for the next fight on the bill, the betting, what a fight came to, and the night's reckoning */
	void DrawWindowCard(const AGutGameMode* GameMode, float CenterX);
	void DrawWindowBetting(const AGutGameMode* GameMode, float CenterX);
	float DrawWindowResult(const AGutGameMode* GameMode, float CenterX);
	void DrawWindowNight(const AGutGameMode* GameMode, float CenterX);

	/** Draws a dark panel for text to sit on. Position and size are in 1080p layout units. */
	void DrawPanel(float CenterX, float Y, float Width, float Height, EGutPanel Kind = EGutPanel::Steel);

	/** The menu that stops the game, over whatever else is on the screen */
	void DrawStopped(const AGutGameMode* GameMode, float CenterX);

	/** A part of the story told in pictures: the photograph over the whole screen, and its words along the bottom */
	void DrawStory(const AGutGameMode* GameMode, float LayoutWidth);

	/** The story's photographs, kept once they have been read */
	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UTexture2D>> StoryPictures;

	/** The surfaces the panels are drawn on. Without them a panel is a dark rectangle. */
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> PanelSteel;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> PanelSlate;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> PanelHoarding;

	/** White paint brushed on steel, which every bar is filled with in its own colour, and a stencilled figure for the limbs */
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> PanelPaint;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> FigureStencil;

	/** A part of a photograph drawn into a part of the screen, in a colour: the corners of each are given as shares of the whole */
	void DrawPiece(UTexture2D* Picture, float X, float Y, float Width, float Height, const FVector2D& From, const FVector2D& To, const FLinearColor& Color);

	/** The typefaces: a typewriter's for everything, a stencil for the big words. Read from Content/Gut/Fonts. Without them the engine's own. */
	TSharedPtr<const FCompositeFont> BodyFont;
	TSharedPtr<const FCompositeFont> HeadFont;

	bool bLookLoaded = false;

	/** Fetches the surfaces and the typefaces, once */
	void LoadLook();

	/** Draws a name with a health bar and a stamina bar under it, and the small figure after the bars or before them. Position and size are in 1080p layout units. */
	void DrawFighterPanel(const AGutFighter* Fighter, float X, float Y, float Width, bool bCentered, bool bFigureBefore = false);

	/** Draws a small figure whose arms and legs are colored by how much damage each can still take. Position is in 1080p layout units. */
	void DrawLimbFigure(const AGutFighter* Fighter, float X, float Y);

	/** Draws the prompt to finish a staggered enemy, or the countdown to the player's own second wind. Somebody watching is only told who is down. */
	void DrawStaggerPrompt(const AGutFighter* Player, const AGutFighter* Enemy, float CenterX, bool bWatching);

	/** Draws the countdown to a hazard that is about to go live, and the news of one that just has */
	void DrawHazardNews(const AGutGameMode* GameMode, float CenterX);

	/** Draws a small marker on the fighter the camera is locked on to */
	void DrawLockOnMarker(const AGutFighter* Target);

	/** Draws a bar with a backing. Position and size are in 1080p layout units. */
	void DrawBar(float X, float Y, float Width, float Height, float Fraction, const FLinearColor& FillColor);

	/** Draws the state label and combat text over a fighter's head */
	void DrawOverheadText(const AGutFighter* Fighter);

	/** Draws text. Position and size are in 1080p layout units. It is set smaller if it would run off the panel under it, the screen, or a width given. */
	void DrawLabel(const FString& Text, float X, float Y, int32 Size, const FLinearColor& Color, bool bCentered = false, float MaxWidth = 0.0f);

	/** Draws text at a position in screen pixels. Shrink makes the type smaller than its size by that much. */
	void DrawLabelAtScreen(const FString& Text, float ScreenX, float ScreenY, int32 Size, const FLinearColor& Color, bool bCentered, float Shrink = 1.0f);

	/** How wide text of a size is when drawn, in 1080p layout units */
	float MeasureLabel(const FString& Text, int32 Size);

	/** Breaks text between its words into lines no wider than a width in 1080p layout units */
	TArray<FString> WrapLabel(const FString& Text, int32 Size, float MaxWidth);

private:

	/** The face and size text is drawn in */
	FSlateFontInfo LabelFont(int32 Size, float Shrink);
};
