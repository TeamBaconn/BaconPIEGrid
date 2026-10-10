// © 2026 Team Bacon. All Rights Reserved.

#include "BaconPIEGridLayout.h"

FBaconPIEGridLayout FBaconPIEGridLayout::MakeGrid(int32 InNumCells, int32 InPreferredColumns, const FVector2f& InGridSize, float InAspectRatio)
{
	const int32 NumCells = FMath::Max(InNumCells, 1);

	FBaconPIEGridLayout Layout;
	Layout.Columns = InPreferredColumns > 0 ? FMath::Min(InPreferredColumns, NumCells) : FMath::CeilToInt(FMath::Sqrt(static_cast<float>(NumCells)));

	// With a locked ratio, auto columns switch to whichever count gives each client the biggest picture. Near ties keep the near-square grid.
	if (InPreferredColumns <= 0 && InAspectRatio > 0.f && InGridSize.X > 0.f && InGridSize.Y > 0.f)
	{
		const auto GetPictureArea = [&](int32 InColumnCount)
		{
			const int32 RowCount = FMath::DivideAndRoundUp(NumCells, InColumnCount);
			const FVector2f Picture = FitToAspectRatio(FVector2f(InGridSize.X / InColumnCount, InGridSize.Y / RowCount), InAspectRatio);
			return Picture.X * Picture.Y;
		};

		float BestPictureArea = GetPictureArea(Layout.Columns);
		for (int32 ColumnCount = 1; ColumnCount <= NumCells; ++ColumnCount)
		{
			const float PictureArea = GetPictureArea(ColumnCount);
			if (PictureArea > BestPictureArea * 1.001f)
			{
				BestPictureArea = PictureArea;
				Layout.Columns = ColumnCount;
			}
		}
	}

	Layout.Rows = FMath::DivideAndRoundUp(NumCells, Layout.Columns);
	return Layout;
}

FBaconPIEGridLayout FBaconPIEGridLayout::MakeCarousel(int32 InNumCells, int32 InMainCellIndex, float InCarouselHeightRatio, float InAspectRatio)
{
	// A single cell fills the whole area, like a 1x1 grid.
	FBaconPIEGridLayout Layout;
	if (InNumCells > 1)
	{
		Layout.Columns = InNumCells - 1;

		// Hand-edited config skips the settings clamp. 0 would read as a grid, and 1 leaves no room for the main view.
		Layout.CarouselHeightRatio = FMath::Clamp(InCarouselHeightRatio, 0.05f, 0.95f);
		Layout.CarouselAspectRatio = InAspectRatio;
		Layout.MainCellIndex = InMainCellIndex;
	}

	return Layout;
}

FVector2f FBaconPIEGridLayout::FitToAspectRatio(const FVector2f& InAvailableSize, float InAspectRatio)
{
	if (InAspectRatio <= 0.f || InAvailableSize.Y <= 0.f)
	{
		return InAvailableSize;
	}

	return InAvailableSize.X / InAvailableSize.Y > InAspectRatio
		? FVector2f(InAvailableSize.Y * InAspectRatio, InAvailableSize.Y)
		: FVector2f(InAvailableSize.X, InAvailableSize.X / InAspectRatio);
}

FSlateRect FBaconPIEGridLayout::GetCellArea(int32 InCellIndex, const FSlateRect& InGridArea, float InPadding) const
{
	if (IsCarousel())
	{
		return GetCarouselCellArea(InCellIndex, InGridArea, InPadding);
	}

	const float CellWidth = (InGridArea.Right - InGridArea.Left) / Columns;
	const float CellHeight = (InGridArea.Bottom - InGridArea.Top) / Rows;
	const float CellLeft = InGridArea.Left + (InCellIndex % Columns) * CellWidth;
	const float CellTop = InGridArea.Top + (InCellIndex / Columns) * CellHeight;
	return FSlateRect(CellLeft + InPadding, CellTop + InPadding, CellLeft + CellWidth - InPadding, CellTop + CellHeight - InPadding);
}

FSlateRect FBaconPIEGridLayout::GetCarouselCellArea(int32 InCellIndex, const FSlateRect& InGridArea, float InPadding) const
{
	const FMargin Padding(InPadding);
	const float CarouselTop = InGridArea.Bottom - (InGridArea.Bottom - InGridArea.Top) * CarouselHeightRatio;
	const FSlateRect MainArea(InGridArea.Left, InGridArea.Top, InGridArea.Right, CarouselTop);

	// Wrapping around keeps the carousel in client order from the main view on, which is the order the switcher walks.
	const int32 NumCells = Columns + 1;
	const int32 CarouselPosition = (InCellIndex - MainCellIndex + NumCells) % NumCells;
	if (CarouselPosition == 0)
	{
		return MainArea.InsetBy(Padding);
	}

	// Carousel cells share the main view's shape, so a client only changes scale when it moves in or out of the main view.
	const FVector2f MainPictureSize = MainArea.InsetBy(Padding).GetSize2f();
	const float PictureAspectRatio = CarouselAspectRatio > 0.f ? CarouselAspectRatio : (MainPictureSize.Y > 0.f ? MainPictureSize.X / MainPictureSize.Y : 0.f);

	// Every cell gets the biggest picture that fits its share of the carousel, and the row is centered below the main view.
	const FVector2f PaddingSize(2.f * InPadding);
	const FVector2f CarouselSize(InGridArea.Right - InGridArea.Left, InGridArea.Bottom - CarouselTop);
	const FVector2f PictureSize = FitToAspectRatio(FVector2f::Max(FVector2f(CarouselSize.X / Columns, CarouselSize.Y) - PaddingSize, FVector2f::ZeroVector), PictureAspectRatio);
	const FVector2f CellSize = PictureSize + PaddingSize;
	const FVector2f RowTopLeft(InGridArea.Left + (CarouselSize.X - CellSize.X * Columns) * 0.5f, CarouselTop + (CarouselSize.Y - CellSize.Y) * 0.5f);
	return FSlateRect::FromPointAndExtent(RowTopLeft + FVector2f(CellSize.X * (CarouselPosition - 1), 0.f), CellSize).InsetBy(Padding);
}
