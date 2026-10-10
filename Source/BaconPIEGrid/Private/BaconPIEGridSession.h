// © 2026 Team Bacon. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Layout/Margin.h"
#include "Layout/SlateRect.h"

class FBaconPIEGridSwitcher;
class FBaconPIEGridUndockedTarget;
class IAssetViewport;
class SBaconPIEGridCell;
class SSplitter;
class SUniformGridPanel;
class SViewport;
class SWidget;
class SWindow;
struct FGeometry;
struct FRequestPlaySessionParams;
enum EPlayNetMode : int;

/** Cell arrangement shared by the docked grid, the shrunk level viewport and every client window, so they all line up. */
struct FBaconPIEGridLayout
{
	/** InGridSize and InAspectRatio only matter for auto columns with a locked ratio. An aspect ratio of 0 means unlocked. */
	static FBaconPIEGridLayout Make(int32 InNumCells, int32 InPreferredColumns, const FVector2f& InGridSize, float InAspectRatio);

	/** Same arrangement as SUniformGridPanel with InPadding as its slot padding. */
	FSlateRect GetCellArea(int32 InCellIndex, const FSlateRect& InGridArea, float InPadding) const;

	int32 Columns = 1;
	int32 Rows = 1;
};

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

	void FocusFirstClient() const;
	void End();

private:
	explicit FBaconPIEGridSession(int32 InNumCells);

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

	void HandlePostTick(float InDeltaTime);
	void PlaceWindow(FBaconPIEGridTiledWindow& InTiledWindow);
	bool HasGridArea() const;
	void UpdateLayout();
	void BuildGrid();
	void RebuildGrid();
	void SetCellContent(int32 InCellIndex, const TSharedRef<SWidget>& InContent);
	void EnsureCellCount(int32 InNumCells);
	int32 FindFreeCellIndex() const;

private:
	FBaconPIEGridLayout Layout;
	int32 PreferredColumns = 0;
	float CellPadding = 0.f;

	/** Width / height every client is letterboxed to. 0 lets clients fill their cells. */
	float AspectRatio = 0.f;
	bool bShowCellLabels = true;
	bool bTileProcessWindows = true;

	/** Desktop pixels, from the docking area's last layout when the session started, or every frame while following the host viewport. */
	FSlateRect GridArea;
	float GridAreaScale = 1.f;

	TSharedPtr<IAssetViewport> HostViewport;

	/** Set when the grid covers the level editor's panel area through this window's overlay instead of sitting on the play viewport. */
	TWeakPtr<SWindow> OverlayWindow;
	TArray<FBaconPIEGridPushedSplit> PushedSplits;
	TSharedPtr<SWidget> GridRoot;
	TSharedPtr<SUniformGridPanel> GridPanel;

	/** Indexed by cell. Null until that cell's client arrives. */
	TArray<TSharedPtr<SWidget>> CellContents;
	TArray<TSharedPtr<SBaconPIEGridCell>> DockedCells;
	TSharedPtr<FBaconPIEGridSwitcher> Switcher;

	// === TILING ===

	TArray<FBaconPIEGridTiledWindow> TiledWindows;
	TArray<TSharedRef<FBaconPIEGridUndockedTarget>> UndockedTargets;

	/** The level viewport shrunk to the first cell, and its content padding to put back when play stops. */
	TWeakPtr<SWidget> ShrunkViewport;
	FMargin OriginalViewportPadding;
	FDelegateHandle PostTickHandle;
};
