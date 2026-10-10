// © 2026 Team Bacon. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BaconPIEGridSwitcher.h"
#include "IAssetViewport.h"
#include "Widgets/SCompoundWidget.h"

class FSceneViewport;
class SGameLayerManager;
class SOverlay;
class SViewport;
class UGameViewportClient;

/**
 * Hosts one docked PIE client. The engine drives it through IAssetViewport exactly like the level viewport
 * of single-client PIE, so the setup mirrors SLevelViewport::StartPlayInEditorSession.
 */
class SBaconPIEGridCell : public SCompoundWidget, public IAssetViewport, public IBaconPIEGridSwitchTarget
{
public:
	SLATE_BEGIN_ARGS(SBaconPIEGridCell)
		: _ShowLabel(true)
		, _AspectRatio(0.f)
	{}
		SLATE_ARGUMENT(FText, Label)
		SLATE_ARGUMENT(bool, ShowLabel)

		/** Width / height to letterbox the client to. 0 fills the cell. */
		SLATE_ARGUMENT(float, AspectRatio)

		/** Called when the client switcher picks this client, before the client takes focus. */
		SLATE_EVENT(FSimpleDelegate, OnSwitchedTo)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<IAssetViewport>& InHostViewport);

	/** Focuses the client as a click would. Unlike FocusClient, it doesn't count as a switch, so the carousel stays put. */
	void FocusViewport();

	//~ IBaconPIEGridSwitchTarget
	virtual bool HasFocusedClient() const override;
	virtual void SetHighlighted(bool bInHighlighted) override;
	virtual void FocusClient() override;

	//~ SWidget
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

	//~ IAssetViewport
	virtual void StartPlayInEditorSession(UGameViewportClient* PlayClient, const bool bInSimulateInEditor) override;
	virtual void EndPlayInEditorSession() override;
	virtual void RegisterGameViewportIfPIE() override;
	virtual bool HasPlayInEditorViewport() const override;
	virtual TSharedPtr<FSceneViewport> GetSharedActiveViewport() const override;
	virtual FEditorViewportClient& GetAssetViewportClient() override;
	virtual FViewport* GetActiveViewport() override;
	virtual void AddOverlayWidget(TSharedRef<SWidget> OverlaidWidget, int32 ZOrder = INDEX_NONE) override;
	virtual void RemoveOverlayWidget(TSharedRef<SWidget> OverlaidWidget) override;
	virtual TSharedRef<const SWidget> AsWidget() const override;
	virtual TSharedRef<SWidget> AsWidget() override;
	virtual TWeakPtr<SViewport> GetViewportWidget() override;

	// Simulate and Possess/Eject never reach a cell (the grid editor engine blocks the toggle), and immersive/game view are level viewport modes.
	virtual void SwapViewportsForSimulateInEditor() override {}
	virtual void SwapViewportsForPlayInEditor() override {}
	virtual void OnSimulateSessionStarted() override {}
	virtual void OnSimulateSessionFinished() override {}
	virtual void MakeImmersive(const bool bWantImmersive, const bool bAllowAnimation) override {}
	virtual bool IsImmersive() const override { return false; }
	virtual void ToggleGameView() override {}
	virtual bool IsInGameView() const override { return false; }

private:
	// === EVENTS ===
	FSimpleDelegate OnSwitchedTo;

private:
	TWeakPtr<IAssetViewport> HostViewport;
	TSharedPtr<SViewport> ViewportWidget;
	TSharedPtr<SGameLayerManager> GameLayerManager;
	TSharedPtr<SOverlay> GameOverlay;
	TSharedPtr<SOverlay> EditorOverlay;
	TSharedPtr<SWidget> HighlightFrame;
	TSharedPtr<FSceneViewport> SceneViewport;
	FDelegateHandle PreviewFeatureLevelChangedHandle;
};
