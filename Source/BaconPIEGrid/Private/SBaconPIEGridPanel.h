// © 2026 Team Bacon. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BaconPIEGridLayout.h"
#include "Layout/Children.h"
#include "SlotBase.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SPanel.h"

/**
 * Arranges the docked grid's cells where FBaconPIEGridLayout puts them, the same rects the client windows are placed on.
 * A new layout only moves the cells, so a client never leaves the panel and keeps its focus and mouse capture.
 */
class SBaconPIEGridPanel : public SPanel
{
public:
	class FSlot : public TSlotBase<FSlot>
	{
	public:
		explicit FSlot(int32 InCellIndex)
			: CellIndex(InCellIndex)
		{
		}

		SLATE_SLOT_BEGIN_ARGS(FSlot, TSlotBase<FSlot>)
		SLATE_SLOT_END_ARGS()
		using TSlotBase<FSlot>::Construct;

		int32 GetCellIndex() const { return CellIndex; }

	private:
		int32 CellIndex;
	};

	SLATE_BEGIN_ARGS(SBaconPIEGridPanel)
		: _CellPadding(0.f)
	{
		_Visibility = EVisibility::SelfHitTestInvisible;
	}
		SLATE_ARGUMENT(FBaconPIEGridLayout, Layout)
		SLATE_ARGUMENT(float, CellPadding)
	SLATE_END_ARGS()

	using FScopedWidgetSlotArguments = TPanelChildren<FSlot>::FScopedWidgetSlotArguments;

	SBaconPIEGridPanel();

	void Construct(const FArguments& InArgs);

	FScopedWidgetSlotArguments AddSlot(int32 InCellIndex);
	void SetLayout(const FBaconPIEGridLayout& InLayout);

	//~ SPanel
	virtual void OnArrangeChildren(const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren) const override;
	virtual FVector2D ComputeDesiredSize(float) const override;
	virtual FChildren* GetChildren() override;

private:
	TPanelChildren<FSlot> Slots;
	FBaconPIEGridLayout Layout;
	float CellPadding = 0.f;
};
