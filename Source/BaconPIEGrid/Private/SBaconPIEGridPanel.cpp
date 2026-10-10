// © 2026 Team Bacon. All Rights Reserved.

#include "SBaconPIEGridPanel.h"

#include "Layout/ArrangedChildren.h"

SBaconPIEGridPanel::SBaconPIEGridPanel()
	: Slots(this)
{
}

void SBaconPIEGridPanel::Construct(const FArguments& InArgs)
{
	Layout = InArgs._Layout;
	CellPadding = InArgs._CellPadding;
}

SBaconPIEGridPanel::FScopedWidgetSlotArguments SBaconPIEGridPanel::AddSlot(int32 InCellIndex)
{
	return FScopedWidgetSlotArguments{ MakeUnique<FSlot>(InCellIndex), Slots, INDEX_NONE };
}

void SBaconPIEGridPanel::SetLayout(const FBaconPIEGridLayout& InLayout)
{
	Layout = InLayout;
	Invalidate(EInvalidateWidgetReason::Layout);
}

void SBaconPIEGridPanel::OnArrangeChildren(const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren) const
{
	const FSlateRect GridArea(FVector2f::ZeroVector, AllottedGeometry.GetLocalSize());
	for (int32 SlotIndex = 0; SlotIndex < Slots.Num(); ++SlotIndex)
	{
		const FSlot& Slot = Slots[SlotIndex];
		const EVisibility ChildVisibility = Slot.GetWidget()->GetVisibility();
		if (ArrangedChildren.Accepts(ChildVisibility))
		{
			const FSlateRect CellArea = Layout.GetCellArea(Slot.GetCellIndex(), GridArea, CellPadding);
			ArrangedChildren.AddWidget(ChildVisibility, AllottedGeometry.MakeChild(Slot.GetWidget(), CellArea.GetTopLeft2f(), FVector2f::Max(CellArea.GetSize2f(), FVector2f::ZeroVector)));
		}
	}
}

FVector2D SBaconPIEGridPanel::ComputeDesiredSize(float) const
{
	// Always stretched over the play viewport or the panel area, so it has no size of its own.
	return FVector2D::ZeroVector;
}

FChildren* SBaconPIEGridPanel::GetChildren()
{
	return &Slots;
}
