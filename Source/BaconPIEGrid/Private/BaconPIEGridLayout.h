// © 2026 Team Bacon. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Layout/SlateRect.h"

/** Cell arrangement shared by the docked clients, the shrunk level viewport and every client window, so they all line up. */
struct FBaconPIEGridLayout
{
	/** InGridSize and InAspectRatio only matter for auto columns with a locked ratio. An aspect ratio of 0 means unlocked. */
	static FBaconPIEGridLayout MakeGrid(int32 InNumCells, int32 InPreferredColumns, const FVector2f& InGridSize, float InAspectRatio);

	/**
	 * InMainCellIndex fills the main view, and the cells after it line up in the carousel below, wrapping around. Carousel cells
	 * take InAspectRatio, or the main view's shape when it's 0.
	 */
	static FBaconPIEGridLayout MakeCarousel(int32 InNumCells, int32 InMainCellIndex, float InCarouselHeightRatio, float InAspectRatio);

	static FVector2f FitToAspectRatio(const FVector2f& InAvailableSize, float InAspectRatio);

	FSlateRect GetCellArea(int32 InCellIndex, const FSlateRect& InGridArea, float InPadding) const;

	bool IsCarousel() const { return CarouselHeightRatio > 0.f; }

private:
	FSlateRect GetCarouselCellArea(int32 InCellIndex, const FSlateRect& InGridArea, float InPadding) const;

public:
	/** A carousel has one row, with a column for every cell but the main one. */
	int32 Columns = 1;
	int32 Rows = 1;

	// === CAROUSEL ===

	/** Share of the grid's height below the main view. 0 for a grid. */
	float CarouselHeightRatio = 0.f;
	float CarouselAspectRatio = 0.f;
	int32 MainCellIndex = 0;
};
