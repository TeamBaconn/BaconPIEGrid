// © 2026 Team Bacon. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Commands/InputChord.h"

class IAssetViewport;
class SViewport;
class SWidget;
class SWindow;
struct FSlateBrush;

/** A client the switcher can walk to: a docked cell, or a client playing in the level viewport or its own PIE window. */
class IBaconPIEGridSwitchTarget
{
public:
	virtual ~IBaconPIEGridSwitchTarget() = default;

	virtual bool HasFocusedClient() const = 0;
	virtual void SetHighlighted(bool bInHighlighted) = 0;
	virtual void FocusClient() = 0;

protected:
	static const FSlateBrush* GetHighlightBrush();

	/** Moves the cursor onto the client and focuses it as a click would, so it captures the mouse when its input mode does. */
	static void FocusGameViewport(const TSharedRef<SViewport>& InViewportWidget);
};

/** A client that isn't docked: the one in the level viewport, or one in its own PIE window. */
class FBaconPIEGridUndockedTarget : public IBaconPIEGridSwitchTarget
{
public:
	explicit FBaconPIEGridUndockedTarget(const TSharedRef<IAssetViewport>& InViewport);
	FBaconPIEGridUndockedTarget(const TSharedRef<SWindow>& InWindow, const TSharedRef<SViewport>& InViewportWidget);

	/** Takes the highlight frame back off the viewport or window. */
	void Detach();

	//~ IBaconPIEGridSwitchTarget
	virtual bool HasFocusedClient() const override;
	virtual void SetHighlighted(bool bInHighlighted) override;
	virtual void FocusClient() override;

private:
	static TSharedRef<SWidget> MakeHighlightFrame();

private:
	/** Exactly one of these is set, depending on where the client plays. */
	TWeakPtr<IAssetViewport> Viewport;
	TWeakPtr<SWindow> Window;

	TWeakPtr<SViewport> ViewportWidget;
	TSharedRef<SWidget> HighlightFrame;
};

/**
 * Alt+Tab style client switcher: hold the chord's modifiers and tap its key to move a highlight across the clients,
 * then release the modifiers to focus the highlighted one. Shift walks backwards, Escape cancels.
 */
class FBaconPIEGridSwitcher : public IInputProcessor
{
public:
	explicit FBaconPIEGridSwitcher(const FInputChord& InChord);

	void AddTarget(const TSharedRef<IBaconPIEGridSwitchTarget>& InTarget);

	//~ IInputProcessor
	virtual void Tick(const float DeltaTime, FSlateApplication& SlateApplication, TSharedRef<ICursor> Cursor) override;
	virtual bool HandleKeyDownEvent(FSlateApplication& SlateApplication, const FKeyEvent& InKeyEvent) override;
	virtual bool HandleKeyUpEvent(FSlateApplication& SlateApplication, const FKeyEvent& InKeyEvent) override;
	virtual const TCHAR* GetDebugName() const override { return TEXT("BaconPIEGridSwitcher"); }

private:
	bool AreChordModifiersDown(const FModifierKeysState& InModifierKeys) const;
	int32 FindFocusedTargetIndex() const;
	void MoveHighlight(int32 InStep);
	void StopSwitching(bool bInFocusHighlighted);

private:
	FInputChord Chord;
	TArray<TWeakPtr<IBaconPIEGridSwitchTarget>> Targets;
	int32 HighlightedIndex = INDEX_NONE;
	bool bSwallowChordKeyUp = false;
};
