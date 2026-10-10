// © 2026 Team Bacon. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Framework/Commands/InputChord.h"
#include "BaconPIEGridSettings.generated.h"

/**
 * Per-user options for docking multi-client PIE into a grid and tiling client windows
 * (Editor Preferences > Level Editor > PIE Grid).
 */
UCLASS(config = EditorPerProjectUserSettings, meta = (DisplayName = "PIE Grid"))
class BACONPIEGRID_API UBaconPIEGridSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override;

public:
	/** Dock in-process clients into a grid over the play viewport. Applies to "Selected Viewport" play with more than one client. */
	UPROPERTY(EditAnywhere, config, Category = "PIE Grid")
	bool bDockClients = true;

	UPROPERTY(EditAnywhere, config, Category = "PIE Grid", meta = (InlineEditConditionToggle))
	bool bUseCustomPlayScreenSize = false;

	/**
	 * When ticked, size the grid against this width : height share of the level editor's panel area (everything between the
	 * top toolbar and the status bar) instead of the play viewport's current size. Below 1 the panels next to the play viewport
	 * are pushed aside to make room, which only works in a direction that has a panel split next to the viewport. 1 : 1 covers
	 * the whole panel area instead, with the panels and drawers hidden behind the grid. The level editor layout is put back
	 * when play stops.
	 */
	UPROPERTY(EditAnywhere, config, Category = "PIE Grid", meta = (EditCondition = "bUseCustomPlayScreenSize", ClampMin = "0.1", ClampMax = "1"))
	FVector2D PlayScreenSizeRatio = FVector2D(1.0, 1.0);

	/**
	 * Place client windows on the same grid cells: separate-process windows (Run Under One Process off, or Standalone Game), and
	 * the windows of PIE clients that aren't docked. With Dock Clients off, "Selected Viewport" play keeps the first client in the
	 * level viewport, shrunk to the first cell, and the other windows follow the viewport as it moves or resizes.
	 */
	UPROPERTY(EditAnywhere, config, Category = "PIE Grid")
	bool bTileProcessWindows = true;

	/** 0 picks the layout automatically: near-square, or the one with the biggest picture when the aspect ratio is enforced. */
	UPROPERTY(EditAnywhere, config, Category = "PIE Grid", meta = (ClampMin = "0", ClampMax = "8"))
	int32 Columns = 0;

	UPROPERTY(EditAnywhere, config, Category = "PIE Grid", meta = (ClampMin = "0", ClampMax = "16"))
	float CellPadding = 2.f;

	UPROPERTY(EditAnywhere, config, Category = "PIE Grid", meta = (InlineEditConditionToggle))
	bool bEnforceClientAspectRatio = false;

	/** When ticked, letterbox every client to this width : height inside its cell. Separate-process windows get the same shape. */
	UPROPERTY(EditAnywhere, config, Category = "PIE Grid", meta = (EditCondition = "bEnforceClientAspectRatio", ClampMin = "1"))
	FIntPoint ClientAspectRatio = FIntPoint(16, 9);

	UPROPERTY(EditAnywhere, config, Category = "PIE Grid")
	bool bShowCellLabels = true;

	/**
	 * Hold the modifiers and tap the key to move a highlight across the grid's PIE clients, docked or in their own windows, then
	 * release the modifiers to switch. Shift walks backwards and Escape cancels. Only taken over while one of those clients has focus.
	 */
	UPROPERTY(EditAnywhere, config, Category = "PIE Grid")
	FInputChord SwitchClientChord = FInputChord(EModifierKey::Control, EKeys::Tab);
};
