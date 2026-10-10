// © 2026 Team Bacon. All Rights Reserved.

#include "BaconPIEGridSwitcher.h"

#include "Brushes/SlateRoundedBoxBrush.h"
#include "Framework/Application/SlateApplication.h"
#include "IAssetViewport.h"
#include "Layout/WidgetPath.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/SViewport.h"
#include "Widgets/SWindow.h"

const FSlateBrush* IBaconPIEGridSwitchTarget::GetHighlightBrush()
{
	static const FSlateRoundedBoxBrush HighlightBrush(FLinearColor::Transparent, 0.f, FLinearColor(1.f, 0.8f, 0.f), 4.f);
	return &HighlightBrush;
}

void IBaconPIEGridSwitchTarget::FocusGameViewport(const TSharedRef<SViewport>& InViewportWidget)
{
	FSlateApplication& SlateApplication = FSlateApplication::Get();

	// Release the previous client's capture and lock first, otherwise the cursor can't leave it.
	SlateApplication.ResetToDefaultPointerInputSettings();

	// A fresh path instead of the cached geometry, which is a frame behind when the carousel has just moved the client.
	FWidgetPath ViewportPath;
	if (SlateApplication.GeneratePathToWidgetUnchecked(InViewportWidget, ViewportPath))
	{
		SlateApplication.SetCursorPos(ViewportPath.Widgets.Last().Geometry.GetAbsolutePositionAtCoordinates(FVector2D(0.5, 0.5)));
	}

	// In the editor a PIE viewport only captures the mouse when focus arrives by mouse (FSceneViewport::OnFocusReceived).
	// That makes this act like a click into the client without the click reaching the game; the client's input mode still decides.
	SlateApplication.RegisterGameViewport(InViewportWidget);
	SlateApplication.SetAllUserFocusToGameViewport(EFocusCause::Mouse);
}

FBaconPIEGridUndockedTarget::FBaconPIEGridUndockedTarget(const TSharedRef<IAssetViewport>& InViewport)
	: Viewport(InViewport)
	, ViewportWidget(InViewport->GetViewportWidget())
	, HighlightFrame(MakeHighlightFrame())
{
	InViewport->AddOverlayWidget(HighlightFrame);
}

FBaconPIEGridUndockedTarget::FBaconPIEGridUndockedTarget(const TSharedRef<SWindow>& InWindow, const TSharedRef<SViewport>& InViewportWidget)
	: Window(InWindow)
	, ViewportWidget(InViewportWidget)
	, HighlightFrame(MakeHighlightFrame())
{
	InWindow->AddOverlaySlot()
	[
		HighlightFrame
	];
}

void FBaconPIEGridUndockedTarget::Detach()
{
	if (const TSharedPtr<IAssetViewport> PinnedViewport = Viewport.Pin())
	{
		PinnedViewport->RemoveOverlayWidget(HighlightFrame);
	}
	else if (const TSharedPtr<SWindow> PinnedWindow = Window.Pin())
	{
		PinnedWindow->RemoveOverlaySlot(HighlightFrame);
	}
}

bool FBaconPIEGridUndockedTarget::HasFocusedClient() const
{
	if (const TSharedPtr<IAssetViewport> PinnedViewport = Viewport.Pin())
	{
		return PinnedViewport->AsWidget()->HasFocusedDescendants();
	}

	const TSharedPtr<SWindow> PinnedWindow = Window.Pin();
	return PinnedWindow.IsValid() && PinnedWindow->HasFocusedDescendants();
}

void FBaconPIEGridUndockedTarget::SetHighlighted(bool bInHighlighted)
{
	HighlightFrame->SetVisibility(bInHighlighted ? EVisibility::HitTestInvisible : EVisibility::Collapsed);
}

void FBaconPIEGridUndockedTarget::FocusClient()
{
	const TSharedPtr<SViewport> PinnedViewportWidget = ViewportWidget.Pin();
	if (!PinnedViewportWidget.IsValid() || HasFocusedClient())
	{
		return;
	}

	// The client may be in another native window, which has to be active before its keys arrive there.
	const TSharedPtr<SWindow> ClientWindow = FSlateApplication::Get().FindWidgetWindow(PinnedViewportWidget.ToSharedRef());
	if (ClientWindow.IsValid() && ClientWindow->GetNativeWindow().IsValid())
	{
		ClientWindow->BringToFront(true);
		ClientWindow->GetNativeWindow()->SetWindowFocus();
	}

	FocusGameViewport(PinnedViewportWidget.ToSharedRef());
}

TSharedRef<SWidget> FBaconPIEGridUndockedTarget::MakeHighlightFrame()
{
	return SNew(SImage)
		.Visibility(EVisibility::Collapsed)
		.Image(GetHighlightBrush());
}

FBaconPIEGridSwitcher::FBaconPIEGridSwitcher(const FInputChord& InChord)
	: Chord(InChord)
{
}

void FBaconPIEGridSwitcher::AddTarget(const TSharedRef<IBaconPIEGridSwitchTarget>& InTarget)
{
	Targets.Add(InTarget);
}

void FBaconPIEGridSwitcher::Tick(const float DeltaTime, FSlateApplication& SlateApplication, TSharedRef<ICursor> Cursor)
{
	if (HighlightedIndex == INDEX_NONE)
	{
		return;
	}

	// Polled rather than taken from key-up events, so a release while another window has focus still ends the switch.
	if (!SlateApplication.IsActive())
	{
		StopSwitching(false);
	}
	else if (!Chord.HasAnyModifierKeys() || !AreChordModifiersDown(SlateApplication.GetModifierKeys()))
	{
		StopSwitching(true);
	}
}

bool FBaconPIEGridSwitcher::HandleKeyDownEvent(FSlateApplication& SlateApplication, const FKeyEvent& InKeyEvent)
{
	// Escape would otherwise stop the play session.
	if (HighlightedIndex != INDEX_NONE && InKeyEvent.GetKey() == EKeys::Escape)
	{
		StopSwitching(false);
		return true;
	}

	if (InKeyEvent.GetKey() != Chord.Key || !AreChordModifiersDown(InKeyEvent.GetModifierKeys()))
	{
		return false;
	}

	if (HighlightedIndex == INDEX_NONE)
	{
		// The chord only belongs to the switcher while a client has focus. Everywhere else the editor keeps it.
		const int32 FocusedIndex = FindFocusedTargetIndex();
		if (FocusedIndex == INDEX_NONE || Targets.Num() < 2)
		{
			return false;
		}

		HighlightedIndex = FocusedIndex;
	}

	const bool bWalkBackwards = !Chord.NeedsShift() && InKeyEvent.IsShiftDown();
	MoveHighlight(bWalkBackwards ? -1 : 1);
	bSwallowChordKeyUp = true;
	return true;
}

bool FBaconPIEGridSwitcher::HandleKeyUpEvent(FSlateApplication& SlateApplication, const FKeyEvent& InKeyEvent)
{
	// Its key-down never reached the game, so neither does this.
	if (bSwallowChordKeyUp && InKeyEvent.GetKey() == Chord.Key)
	{
		bSwallowChordKeyUp = false;
		return true;
	}

	return false;
}

bool FBaconPIEGridSwitcher::AreChordModifiersDown(const FModifierKeysState& InModifierKeys) const
{
	return (!Chord.NeedsControl() || InModifierKeys.IsControlDown())
		&& (!Chord.NeedsAlt() || InModifierKeys.IsAltDown())
		&& (!Chord.NeedsShift() || InModifierKeys.IsShiftDown())
		&& (!Chord.NeedsCommand() || InModifierKeys.IsCommandDown());
}

int32 FBaconPIEGridSwitcher::FindFocusedTargetIndex() const
{
	return Targets.IndexOfByPredicate([](const TWeakPtr<IBaconPIEGridSwitchTarget>& Target)
	{
		const TSharedPtr<IBaconPIEGridSwitchTarget> PinnedTarget = Target.Pin();
		return PinnedTarget.IsValid() && PinnedTarget->HasFocusedClient();
	});
}

void FBaconPIEGridSwitcher::MoveHighlight(int32 InStep)
{
	if (const TSharedPtr<IBaconPIEGridSwitchTarget> PreviousTarget = Targets[HighlightedIndex].Pin())
	{
		PreviousTarget->SetHighlighted(false);
	}

	HighlightedIndex = (HighlightedIndex + InStep + Targets.Num()) % Targets.Num();

	if (const TSharedPtr<IBaconPIEGridSwitchTarget> NextTarget = Targets[HighlightedIndex].Pin())
	{
		NextTarget->SetHighlighted(true);
	}
}

void FBaconPIEGridSwitcher::StopSwitching(bool bInFocusHighlighted)
{
	const TSharedPtr<IBaconPIEGridSwitchTarget> HighlightedTarget = Targets[HighlightedIndex].Pin();
	HighlightedIndex = INDEX_NONE;

	if (HighlightedTarget.IsValid())
	{
		HighlightedTarget->SetHighlighted(false);
		if (bInFocusHighlighted)
		{
			HighlightedTarget->FocusClient();
		}
	}
}
