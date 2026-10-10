// © 2026 Team Bacon. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BaconPIEGridLayout.h"
#include "Layout/Margin.h"
#include "Layout/SlateRect.h"

class FBaconPIEGridSwitcher;
class FBaconPIEGridUndockedTarget;
class FWeakWidgetPath;
class FWidgetPath;
class IAssetViewport;
class SBaconPIEGridCell;
class SBaconPIEGridPanel;
class SSplitter;
class SViewport;
class SWidget;
class SWindow;
struct FFocusEvent;
struct FGeometry;
struct FRequestPlaySessionParams;
enum class EBaconPIEGridViewMode : uint8;
enum EPlayNetMode : int;

/** A level editor split resized to make room for the grid, and the size coefficient to put back when play stops. */
struct FBaconPIEGridPushedSplit
{
	TWeakPtr<SSplitter> Splitter;

	/** The split's child on the viewport's side, used to find its slot again in case the split changed during play. */
	TWeakPtr<SWidget> ViewportSide;
	float OriginalSizeValue = 0.f;
};

/** An in-process client's PIE window and the cell area it was last placed on. */
struct FBaconPIEGridTiledWindow
{
	TWeakPtr<SWindow> Window;
	int32 CellIndex = INDEX_NONE;
	FSlateRect CellArea;
};

/**
 * Grid for one multi-client play session: docked cells over the play viewport (or the level editor's panel area),
 * plus the screen area used to put client windows on the same cells.
 */
class FBaconPIEGridSession : public TSharedFromThis<FBaconPIEGridSession>
{
public:
	/** Null unless the request plays into a level viewport with more than one client and docking is enabled. */
	static TSharedPtr<FBaconPIEGridSession> TryStartDocking(const FRequestPlaySessionParams& InParams);

	/**
	 * For sessions where nothing docks. With a play viewport, the first client stays in it, shrunk to the first cell, and the
	 * other windows follow the viewport. Without one (New Editor Window play, or Standalone Game), every window is tiled over
	 * the active viewport.
	 */
	static TSharedPtr<FBaconPIEGridSession> TryStartTiling(const FRequestPlaySessionParams& InParams);

	bool IsDocking() const { return GridPanel.IsValid(); }

	TSharedRef<IAssetViewport> AddDockedClient(EPlayNetMode InNetMode, int32 InPIEInstanceIndex);

	/** Puts an in-process client's PIE window on the next free cell. */
	void AddWindowClient(const TSharedRef<SWindow>& InWindow, const TSharedRef<SViewport>& InViewportWidget);

	/** Fills OutWindowArguments with the -WinX/-WinY/-ResX/-ResY that put the process window on its cell. */
	bool AddProcessClient(int32 InPIEInstanceIndex, EPlayNetMode InNetMode, FString& OutWindowArguments);

	/** Makes the first docked client the game viewport again, unless the user has already switched away from it. */
	void FocusFirstClient() const;
	void End();

private:
	FBaconPIEGridSession(int32 InNumCells, EBaconPIEGridViewMode InViewMode);

	/** InSizeRatio is the share of InGeometry's width and height the grid covers, anchored at its top-left. */
	void CaptureGridArea(const FGeometry& InGeometry, const FVector2f& InSizeRatio);

	/** Resizes the outermost split on each axis around the viewport so the viewport's side gets InShare of it. */
	void PushPanelsAside(const TSharedRef<SWidget>& InViewportWidget, const TSharedRef<SWidget>& InPanelArea, const FVector2f& InShare);
	void PushSplit(const TSharedRef<SSplitter>& InSplitter, const TSharedRef<SWidget>& InViewportSide, float InShare);
	void RestorePushedSplits();

	/** Pads the level viewport's content so the first client plays in the first cell only. */
	bool ShrinkHostViewport();
	void RestoreHostViewport();
	FMargin GetHostViewportPadding() const;

	/** Brings the picked client into the carousel's main view. */
	void HandleCellSwitchedTo(int32 InCellIndex);

	/**
	 * Until the user switches to another client, focus the other clients' game code takes (such as setting the input mode as
	 * each client spawns) goes back to the first client.
	 */
	void HandleFocusChanging(const FFocusEvent& InFocusEvent, const FWeakWidgetPath& InOldFocusPath, const TSharedPtr<SWidget>& InOldFocusedWidget, const FWidgetPath& InNewFocusPath, const TSharedPtr<SWidget>& InNewFocusedWidget);
	void HandleDockedPostTick(float InDeltaTime);
	void HandlePostTick(float InDeltaTime);
	void PlaceWindow(FBaconPIEGridTiledWindow& InTiledWindow);
	bool HasGridArea() const;
	void UpdateLayout();
	void BuildGrid();
	void SetCellContent(int32 InCellIndex, const TSharedRef<SWidget>& InContent);
	void EnsureCellCount(int32 InNumCells);
	int32 FindFreeCellIndex() const;

private:
	EBaconPIEGridViewMode ViewMode;
	FBaconPIEGridLayout Layout;
	int32 PreferredColumns = 0;
	float CarouselHeightRatio = 0.f;
	int32 MainCellIndex = 0;
	float CellPadding = 0.f;

	/** Width / height every client is letterboxed to. 0 lets clients fill their cells. */
	float AspectRatio = 0.f;
	bool bShowCellLabels = true;

	/** Desktop pixels, from the docking area's last layout when the session started, or every frame while following the host viewport. */
	FSlateRect GridArea;
	float GridAreaScale = 1.f;

	TSharedPtr<IAssetViewport> HostViewport;

	/** Set when the grid covers the level editor's panel area through this window's overlay instead of sitting on the play viewport. */
	TWeakPtr<SWindow> OverlayWindow;
	TArray<FBaconPIEGridPushedSplit> PushedSplits;
	TSharedPtr<SWidget> GridRoot;
	TSharedPtr<SBaconPIEGridPanel> GridPanel;

	/** Indexed by cell. Null until that cell's client arrives. */
	TArray<TSharedPtr<SWidget>> CellContents;
	TArray<TSharedPtr<SBaconPIEGridCell>> DockedCells;
	TSharedPtr<FBaconPIEGridSwitcher> Switcher;

	/** Cleared once the user switches to another client. */
	bool bHoldFirstClientFocus = true;
	bool bRefocusFirstClient = false;
	FDelegateHandle FocusChangingHandle;

	/** Docked sessions refocus the first client in it; tiled ones move their windows. */
	FDelegateHandle PostTickHandle;

	// === TILING ===

	TArray<FBaconPIEGridTiledWindow> TiledWindows;
	TArray<TSharedRef<FBaconPIEGridUndockedTarget>> UndockedTargets;

	/** The level viewport shrunk to the first cell, and its content padding to put back when play stops. */
	TWeakPtr<SWidget> ShrunkViewport;
	FMargin OriginalViewportPadding;
};
