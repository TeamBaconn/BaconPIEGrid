// © 2026 Team Bacon. All Rights Reserved.

#include "BaconPIEGridSession.h"

#include "BaconPIEGridLog.h"
#include "BaconPIEGridSettings.h"
#include "BaconPIEGridSwitcher.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/PlatformApplicationMisc.h"
#include "IAssetViewport.h"
#include "ILevelEditor.h"
#include "Input/Events.h"
#include "Layout/WidgetPath.h"
#include "LevelEditor.h"
#include "Modules/ModuleManager.h"
#include "PlayInEditorDataTypes.h"
#include "SBaconPIEGridCell.h"
#include "SBaconPIEGridPanel.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Styling/AppStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/SViewport.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "BaconPIEGridSession"

namespace BaconPIEGridSessionPrivate
{
	int32 GetNumClients(const FRequestPlaySessionParams& InParams)
	{
		const ULevelEditorPlaySettings* PlaySettings = IsValid(InParams.EditorPlaySettings) ? InParams.EditorPlaySettings.Get() : GetDefault<ULevelEditorPlaySettings>();

		int32 NumClients = 1;
		PlaySettings->GetPlayNumberOfClients(NumClients);
		return NumClients;
	}

	float GetAspectRatio(const UBaconPIEGridSettings& InSettings)
	{
		return InSettings.bEnforceClientAspectRatio && InSettings.ClientAspectRatio.X > 0 && InSettings.ClientAspectRatio.Y > 0
			? static_cast<float>(InSettings.ClientAspectRatio.X) / InSettings.ClientAspectRatio.Y
			: 0.f;
	}

	// The window's outer rect, centered in the cell. InFrameSize is its title bar and border, which the aspect ratio leaves out.
	FSlateRect FitWindowToCell(const FSlateRect& InCellArea, const FVector2f& InFrameSize, float InAspectRatio)
	{
		const FVector2f CellSize = InCellArea.GetSize2f();
		const FVector2f WindowSize = FBaconPIEGridLayout::FitToAspectRatio(CellSize - InFrameSize, InAspectRatio) + InFrameSize;
		return FSlateRect::FromPointAndExtent(InCellArea.GetTopLeft2f() + (CellSize - WindowSize) * 0.5f, WindowSize);
	}

	// SLevelEditor stacks its toolbars, panel area and status bar in one vertical box, so the panel area is the box child holding the viewport.
	TSharedPtr<SWidget> FindLevelEditorPanelArea(const TSharedRef<SWidget>& InViewportWidget)
	{
		const TSharedPtr<ILevelEditor> LevelEditor = FModuleManager::GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor")).GetLevelEditorInstance().Pin();
		if (!LevelEditor.IsValid())
		{
			return nullptr;
		}

		for (TSharedPtr<SWidget> Widget = InViewportWidget; Widget.IsValid(); Widget = Widget->GetParentWidget())
		{
			const TSharedPtr<SWidget> Parent = Widget->GetParentWidget();
			if (Parent.IsValid() && Parent->GetParentWidget().Get() == LevelEditor.Get())
			{
				return Widget;
			}
		}

		return nullptr;
	}

	int32 FindChildIndex(SSplitter& InSplitter, const TSharedPtr<SWidget>& InChild)
	{
		FChildren* Children = InSplitter.GetChildren();
		for (int32 ChildIndex = 0; ChildIndex < Children->Num(); ++ChildIndex)
		{
			if (Children->GetChildAt(ChildIndex) == InChild)
			{
				return ChildIndex;
			}
		}

		return INDEX_NONE;
	}

	// Window overlay padding that keeps the grid on the panel area as the editor window resizes or its layout changes.
	FMargin GetPanelOverlayPadding(const TWeakPtr<SWidget>& InPanelArea, const TWeakPtr<SWindow>& InWindow, const FVector2f& InSizeRatio)
	{
		const TSharedPtr<SWidget> PanelArea = InPanelArea.Pin();
		const TSharedPtr<SWindow> Window = InWindow.Pin();
		if (!PanelArea.IsValid() || !Window.IsValid())
		{
			return FMargin(0.f);
		}

		const FGeometry WindowGeometry = Window->GetWindowGeometryInScreen();
		const FGeometry& PanelGeometry = PanelArea->GetTickSpaceGeometry();
		const FVector2f PanelTopLeft = WindowGeometry.AbsoluteToLocal(PanelGeometry.GetAbsolutePosition());
		const FVector2f GridSize = FVector2f(PanelGeometry.GetAbsoluteSize()) * InSizeRatio / WindowGeometry.Scale;
		const FVector2f WindowSize = WindowGeometry.GetLocalSize();

		return FMargin(
			FMath::Max(PanelTopLeft.X, 0.f),
			FMath::Max(PanelTopLeft.Y, 0.f),
			FMath::Max(WindowSize.X - PanelTopLeft.X - GridSize.X, 0.f),
			FMath::Max(WindowSize.Y - PanelTopLeft.Y - GridSize.Y, 0.f));
	}

	// Same wording as the engine's PIE window titles.
	FText MakeClientLabel(EPlayNetMode InNetMode, int32 InIndex)
	{
		switch (InNetMode)
		{
		case PIE_ListenServer:
			return FText::Format(LOCTEXT("ServerLabel", "Server {0}"), InIndex);
		case PIE_Client:
			return FText::Format(LOCTEXT("ClientLabel", "Client {0}"), InIndex);
		default:
			return FText::Format(LOCTEXT("StandaloneLabel", "Standalone {0}"), InIndex);
		}
	}

	TSharedRef<SWidget> MakeProcessPlaceholder(const FText& InLabel)
	{
		return SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("WhiteBrush"))
			.BorderBackgroundColor(FLinearColor(0.02f, 0.02f, 0.02f))
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Justification(ETextJustify::Center)
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				.Text(FText::Format(LOCTEXT("ProcessPlaceholder", "{0}\nSeparate process window"), InLabel))
			];
	}

	using FCompoundContentSlot = TSingleWidgetChildrenWithBasicLayoutSlot<EInvalidateWidgetReason::None>;

	// SLevelViewport is a compound widget whose one child slot holds its whole view (toolbar, scene and overlays), so padding
	// that slot shrinks the view and leaves the level editor layout alone.
	FCompoundContentSlot* FindLevelViewportContentSlot(SWidget& InViewportWidget)
	{
		static const FName LevelViewportType(TEXT("SLevelViewport"));
		return InViewportWidget.GetType() == LevelViewportType ? static_cast<FCompoundContentSlot*>(InViewportWidget.GetChildren()) : nullptr;
	}
}

TSharedPtr<FBaconPIEGridSession> FBaconPIEGridSession::TryStartDocking(const FRequestPlaySessionParams& InParams)
{
	const UBaconPIEGridSettings* Settings = GetDefault<UBaconPIEGridSettings>();
	const TSharedPtr<IAssetViewport> DestinationViewport = InParams.DestinationSlateViewport.Get(nullptr).Pin();
	const bool bIsVRPreview = InParams.SessionPreviewTypeOverride.Get(EPlaySessionPreviewType::NoPreview) == EPlaySessionPreviewType::VRPreview;
	const int32 NumClients = BaconPIEGridSessionPrivate::GetNumClients(InParams);
	if (!Settings->bDockClients || !DestinationViewport.IsValid() || NumClients < 2
		|| InParams.WorldType != EPlaySessionWorldType::PlayInEditor || bIsVRPreview)
	{
		return nullptr;
	}

	const TSharedRef<FBaconPIEGridSession> Session = MakeShareable(new FBaconPIEGridSession(NumClients, Settings->ViewMode));
	Session->HostViewport = DestinationViewport;

	// A full 1 : 1 dock covers the whole panel area. Anything smaller pushes the neighboring panels aside so nothing is covered.
	const FVector2f PlayScreenSizeRatio(Settings->PlayScreenSizeRatio);
	const bool bCoverPanelArea = PlayScreenSizeRatio.X >= 1.f && PlayScreenSizeRatio.Y >= 1.f;
	const TSharedPtr<SWidget> PanelArea = Settings->bUseCustomPlayScreenSize ? BaconPIEGridSessionPrivate::FindLevelEditorPanelArea(DestinationViewport->AsWidget()) : nullptr;
	const TSharedPtr<SWindow> PanelWindow = PanelArea.IsValid() && bCoverPanelArea ? FSlateApplication::Get().FindWidgetWindow(PanelArea.ToSharedRef()) : nullptr;
	if (PanelArea.IsValid())
	{
		// Where the grid ends up. A pushed viewport only reaches that size next frame, after process windows have been placed.
		Session->CaptureGridArea(PanelArea->GetTickSpaceGeometry(), PlayScreenSizeRatio);
	}
	else if (const TSharedPtr<SViewport> ViewportWidget = DestinationViewport->GetViewportWidget().Pin())
	{
		Session->CaptureGridArea(ViewportWidget->GetTickSpaceGeometry(), FVector2f(1.f, 1.f));
	}

	Session->BuildGrid();

	if (PanelWindow.IsValid())
	{
		// The window overlay draws above every panel; the padding keeps the grid inside the panel area so the toolbar and status bar stay usable.
		Session->OverlayWindow = PanelWindow;
		PanelWindow->AddOverlaySlot()
		.Padding(TAttribute<FMargin>::CreateLambda([WeakPanelArea = TWeakPtr<SWidget>(PanelArea), WeakWindow = TWeakPtr<SWindow>(PanelWindow), PlayScreenSizeRatio]()
		{
			return BaconPIEGridSessionPrivate::GetPanelOverlayPadding(WeakPanelArea, WeakWindow, PlayScreenSizeRatio);
		}))
		[
			Session->GridRoot.ToSharedRef()
		];
	}
	else
	{
		if (PanelArea.IsValid())
		{
			Session->PushPanelsAside(DestinationViewport->AsWidget(), PanelArea.ToSharedRef(), PlayScreenSizeRatio);
		}
		else if (Settings->bUseCustomPlayScreenSize)
		{
			UE_LOG(LogBaconPIEGrid, Warning, TEXT("FBaconPIEGridSession::TryStartDocking: the level editor's panel area wasn't found, docking over the play viewport instead"));
		}

		DestinationViewport->AddOverlayWidget(Session->GridRoot.ToSharedRef());
	}

	// The Editor bucket runs before the PreGame/Game buckets CommonUI registers in, so a client's UI never sees the switch chord.
	Session->Switcher = MakeShared<FBaconPIEGridSwitcher>(Settings->SwitchClientChord);
	FSlateApplication::Get().RegisterInputPreProcessor(Session->Switcher, EInputPreProcessorType::Editor);

	Session->FocusChangingHandle = FSlateApplication::Get().OnFocusChanging().AddSP(Session, &FBaconPIEGridSession::HandleFocusChanging);
	Session->PostTickHandle = FSlateApplication::Get().OnPostTick().AddSP(Session, &FBaconPIEGridSession::HandleDockedPostTick);

	const FString LayoutDescription = Session->Layout.IsCarousel() ? FString(TEXT("carousel")) : FString::Printf(TEXT("%dx%d grid"), Session->Layout.Columns, Session->Layout.Rows);
	UE_LOG(LogBaconPIEGrid, Log, TEXT("FBaconPIEGridSession::TryStartDocking: docking %d clients in a %s"), NumClients, *LayoutDescription);
	return Session;
}

TSharedPtr<FBaconPIEGridSession> FBaconPIEGridSession::TryStartTiling(const FRequestPlaySessionParams& InParams)
{
	const UBaconPIEGridSettings* Settings = GetDefault<UBaconPIEGridSettings>();
	const bool bIsVRPreview = InParams.SessionPreviewTypeOverride.Get(EPlaySessionPreviewType::NoPreview) == EPlaySessionPreviewType::VRPreview;
	const int32 NumClients = BaconPIEGridSessionPrivate::GetNumClients(InParams);
	if (NumClients < 2 || InParams.WorldType != EPlaySessionWorldType::PlayInEditor || bIsVRPreview)
	{
		return nullptr;
	}

	const TSharedPtr<IAssetViewport> DestinationViewport = InParams.DestinationSlateViewport.Get(nullptr).Pin();
	const TSharedPtr<IAssetViewport> GridViewport = DestinationViewport.IsValid()
		? DestinationViewport
		: FModuleManager::GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor")).GetFirstActiveViewport();
	if (!GridViewport.IsValid())
	{
		return nullptr;
	}

	const TSharedRef<FBaconPIEGridSession> Session = MakeShareable(new FBaconPIEGridSession(NumClients, EBaconPIEGridViewMode::Grid));
	Session->HostViewport = GridViewport;

	// Process windows are launched before pushed panels have moved, so they are placed where the grid will end up.
	const FVector2f PlayScreenSizeRatio(Settings->PlayScreenSizeRatio);
	const TSharedPtr<SWidget> PanelArea = Settings->bUseCustomPlayScreenSize ? BaconPIEGridSessionPrivate::FindLevelEditorPanelArea(GridViewport->AsWidget()) : nullptr;
	if (PanelArea.IsValid())
	{
		Session->CaptureGridArea(PanelArea->GetTickSpaceGeometry(), PlayScreenSizeRatio);
	}
	else if (DestinationViewport.IsValid())
	{
		// The whole level viewport, toolbar included, since that's what shrinks to the first cell.
		Session->CaptureGridArea(DestinationViewport->AsWidget()->GetTickSpaceGeometry(), FVector2f(1.f, 1.f));
	}
	else if (const TSharedPtr<SViewport> ViewportWidget = GridViewport->GetViewportWidget().Pin())
	{
		Session->CaptureGridArea(ViewportWidget->GetTickSpaceGeometry(), FVector2f(1.f, 1.f));
	}

	if (!Session->HasGridArea())
	{
		return nullptr;
	}

	// Standalone Game clients are separate processes the editor can't focus, so only PIE windows get the switcher.
	if (InParams.SessionDestination == EPlaySessionDestinationType::InProcess)
	{
		Session->Switcher = MakeShared<FBaconPIEGridSwitcher>(Settings->SwitchClientChord);
		FSlateApplication::Get().RegisterInputPreProcessor(Session->Switcher, EInputPreProcessorType::Editor);

		if (DestinationViewport.IsValid())
		{
			Session->Switcher->AddTarget(Session->UndockedTargets.Add_GetRef(MakeShared<FBaconPIEGridUndockedTarget>(DestinationViewport.ToSharedRef())));
		}
	}

	// The first client plays in the destination viewport, so it keeps the first cell and the windows follow the viewport from then on.
	if (DestinationViewport.IsValid() && Session->ShrinkHostViewport())
	{
		if (PanelArea.IsValid())
		{
			Session->PushPanelsAside(DestinationViewport->AsWidget(), PanelArea.ToSharedRef(), PlayScreenSizeRatio);
		}

		Session->SetCellContent(0, DestinationViewport->AsWidget());
		Session->PostTickHandle = FSlateApplication::Get().OnPostTick().AddSP(Session, &FBaconPIEGridSession::HandlePostTick);
	}

	UE_LOG(LogBaconPIEGrid, Log, TEXT("FBaconPIEGridSession::TryStartTiling: placing %d client windows in a %dx%d grid"), NumClients, Session->Layout.Columns, Session->Layout.Rows);
	return Session;
}

TSharedRef<IAssetViewport> FBaconPIEGridSession::AddDockedClient(EPlayNetMode InNetMode, int32 InPIEInstanceIndex)
{
	const int32 CellIndex = FindFreeCellIndex();
	const TSharedRef<SBaconPIEGridCell> Cell = SNew(SBaconPIEGridCell, HostViewport.ToSharedRef())
		.Label(BaconPIEGridSessionPrivate::MakeClientLabel(InNetMode, InPIEInstanceIndex))
		.ShowLabel(bShowCellLabels)
		.AspectRatio(AspectRatio)
		.OnSwitchedTo(this, &FBaconPIEGridSession::HandleCellSwitchedTo, CellIndex);

	DockedCells.Add(Cell);
	Switcher->AddTarget(Cell);
	SetCellContent(CellIndex, Cell);
	return Cell;
}

void FBaconPIEGridSession::AddWindowClient(const TSharedRef<SWindow>& InWindow, const TSharedRef<SViewport>& InViewportWidget)
{
	FBaconPIEGridTiledWindow& TiledWindow = TiledWindows.AddDefaulted_GetRef();
	TiledWindow.Window = InWindow;
	TiledWindow.CellIndex = FindFreeCellIndex();
	SetCellContent(TiledWindow.CellIndex, InWindow);
	PlaceWindow(TiledWindow);

	if (Switcher.IsValid())
	{
		Switcher->AddTarget(UndockedTargets.Add_GetRef(MakeShared<FBaconPIEGridUndockedTarget>(InWindow, InViewportWidget)));
	}
}

bool FBaconPIEGridSession::AddProcessClient(int32 InPIEInstanceIndex, EPlayNetMode InNetMode, FString& OutWindowArguments)
{
	EnsureCellCount(InPIEInstanceIndex + 1);
	const int32 CellIndex = CellContents[InPIEInstanceIndex].IsValid() ? FindFreeCellIndex() : InPIEInstanceIndex;

	if (IsDocking())
	{
		const FText Label = BaconPIEGridSessionPrivate::MakeClientLabel(InNetMode, InPIEInstanceIndex);
		SetCellContent(CellIndex, BaconPIEGridSessionPrivate::MakeProcessPlaceholder(Label));
	}

	if (!HasGridArea())
	{
		return false;
	}

	const FSlateRect CellArea = Layout.GetCellArea(CellIndex, GridArea, CellPadding * GridAreaScale);

	// -WinX/-WinY place the client area of an OS-bordered game window, so its title bar is drawn above that point.
	const float TitleBarHeight = SWindowDefs::DefaultTitleBarSize * FPlatformApplicationMisc::GetDPIScaleFactorAtPoint(CellArea.Left, CellArea.Top);
	const FSlateRect WindowArea = BaconPIEGridSessionPrivate::FitWindowToCell(CellArea, FVector2f(0.f, TitleBarHeight), AspectRatio);
	const FVector2f ClientSize = WindowArea.GetSize2f() - FVector2f(0.f, TitleBarHeight);
	if (ClientSize.X < 1.f || ClientSize.Y < 1.f)
	{
		return false;
	}

	OutWindowArguments = FString::Printf(TEXT("-WinX=%d -WinY=%d -ResX=%d -ResY=%d"),
		FMath::RoundToInt(WindowArea.Left), FMath::RoundToInt(WindowArea.Top + TitleBarHeight), FMath::RoundToInt(ClientSize.X), FMath::RoundToInt(ClientSize.Y));
	UE_LOG(LogBaconPIEGrid, Verbose, TEXT("FBaconPIEGridSession::AddProcessClient: cell %d gets %s"), CellIndex, *OutWindowArguments);
	return true;
}

void FBaconPIEGridSession::FocusFirstClient() const
{
	if (bHoldFirstClientFocus && DockedCells.Num() > 0)
	{
		DockedCells[0]->RegisterGameViewportIfPIE();
	}
}

void FBaconPIEGridSession::End()
{
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().OnPostTick().Remove(PostTickHandle);
		FSlateApplication::Get().OnFocusChanging().Remove(FocusChangingHandle);
		if (Switcher.IsValid())
		{
			FSlateApplication::Get().UnregisterInputPreProcessor(Switcher);
		}
	}

	PostTickHandle.Reset();
	FocusChangingHandle.Reset();

	for (const TSharedRef<FBaconPIEGridUndockedTarget>& UndockedTarget : UndockedTargets)
	{
		UndockedTarget->Detach();
	}

	UndockedTargets.Reset();
	RestoreHostViewport();

	if (!HostViewport.IsValid() || !GridRoot.IsValid())
	{
		RestorePushedSplits();
		return;
	}

	const bool bGridHadFocus = GridRoot->HasFocusedDescendants();
	if (const TSharedPtr<SWindow> Window = OverlayWindow.Pin())
	{
		Window->RemoveOverlaySlot(GridRoot.ToSharedRef());
	}
	else
	{
		HostViewport->RemoveOverlayWidget(GridRoot.ToSharedRef());
	}

	RestorePushedSplits();

	// Single-client PIE leaves keyboard focus on the play viewport when it stops.
	const TSharedPtr<SViewport> HostViewportWidget = HostViewport->GetViewportWidget().Pin();
	if (bGridHadFocus && HostViewportWidget.IsValid() && FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().SetKeyboardFocus(HostViewportWidget, EFocusCause::SetDirectly);
	}
}

FBaconPIEGridSession::FBaconPIEGridSession(int32 InNumCells, EBaconPIEGridViewMode InViewMode)
	: ViewMode(InViewMode)
{
	const UBaconPIEGridSettings* Settings = GetDefault<UBaconPIEGridSettings>();
	PreferredColumns = Settings->Columns;
	CarouselHeightRatio = Settings->CarouselHeightRatio;
	CellPadding = Settings->CellPadding;
	AspectRatio = BaconPIEGridSessionPrivate::GetAspectRatio(*Settings);
	bShowCellLabels = Settings->bShowCellLabels;

	EnsureCellCount(InNumCells);
}

void FBaconPIEGridSession::CaptureGridArea(const FGeometry& InGeometry, const FVector2f& InSizeRatio)
{
	// Last frame's layout of the widget the grid will cover; the process windows are placed against it too.
	const FSlateRect Bounds = InGeometry.GetLayoutBoundingRect();
	GridArea = FSlateRect(
		Bounds.Left,
		Bounds.Top,
		Bounds.Left + (Bounds.Right - Bounds.Left) * InSizeRatio.X,
		Bounds.Top + (Bounds.Bottom - Bounds.Top) * InSizeRatio.Y);
	GridAreaScale = InGeometry.Scale;

	UpdateLayout();
}

void FBaconPIEGridSession::PushPanelsAside(const TSharedRef<SWidget>& InViewportWidget, const TSharedRef<SWidget>& InPanelArea, const FVector2f& InShare)
{
	static const FName SplitterType(TEXT("SSplitter"));

	// Indexed by EOrientation. Walking outward overwrites inner splits, so the outermost one on each axis is what's left.
	TSharedPtr<SSplitter> OutermostSplitters[2];
	TSharedPtr<SWidget> OutermostViewportSides[2];
	for (TSharedPtr<SWidget> Child = InViewportWidget; Child.IsValid() && Child.Get() != &InPanelArea.Get(); Child = Child->GetParentWidget())
	{
		const TSharedPtr<SWidget> Parent = Child->GetParentWidget();
		if (Parent.IsValid() && Parent->GetType() == SplitterType)
		{
			const TSharedPtr<SSplitter> Splitter = StaticCastSharedPtr<SSplitter>(Parent);
			OutermostSplitters[Splitter->GetOrientation()] = Splitter;
			OutermostViewportSides[Splitter->GetOrientation()] = Child;
		}
	}

	if (OutermostSplitters[Orient_Horizontal].IsValid())
	{
		PushSplit(OutermostSplitters[Orient_Horizontal].ToSharedRef(), OutermostViewportSides[Orient_Horizontal].ToSharedRef(), InShare.X);
	}

	if (OutermostSplitters[Orient_Vertical].IsValid())
	{
		PushSplit(OutermostSplitters[Orient_Vertical].ToSharedRef(), OutermostViewportSides[Orient_Vertical].ToSharedRef(), InShare.Y);
	}
}

void FBaconPIEGridSession::PushSplit(const TSharedRef<SSplitter>& InSplitter, const TSharedRef<SWidget>& InViewportSide, float InShare)
{
	const int32 ViewportSlotIndex = BaconPIEGridSessionPrivate::FindChildIndex(*InSplitter, InViewportSide);
	if (ViewportSlotIndex == INDEX_NONE)
	{
		return;
	}

	// Docking splits bind this delegate to the panel's size coefficient; without it the slot isn't a docking panel to push.
	SSplitter::FSlot& ViewportSlot = InSplitter->SlotAt(ViewportSlotIndex);
	if (ViewportSlot.GetSizingRule() != SSplitter::FractionOfParent || !ViewportSlot.OnSlotResized().IsBound())
	{
		return;
	}

	float OtherSizeTotal = 0.f;
	for (int32 SlotIndex = 0; SlotIndex < InSplitter->GetChildren()->Num(); ++SlotIndex)
	{
		const SSplitter::FSlot& Slot = InSplitter->SlotAt(SlotIndex);
		if (SlotIndex != ViewportSlotIndex && Slot.GetSizingRule() == SSplitter::FractionOfParent)
		{
			OtherSizeTotal += Slot.GetSizeValue();
		}
	}

	if (OtherSizeTotal <= 0.f)
	{
		return;
	}

	// Fraction slots share space by coefficient, so this gives the viewport's side InShare of the split. The other panels
	// keep their proportions and stop at the splitter's minimum size.
	const float Share = FMath::Clamp(InShare, 0.05f, 0.999f);
	PushedSplits.Add({ InSplitter, InViewportSide, ViewportSlot.GetSizeValue() });
	ViewportSlot.OnSlotResized().Execute(OtherSizeTotal * Share / (1.f - Share));
}

void FBaconPIEGridSession::RestorePushedSplits()
{
	for (int32 PushedIndex = PushedSplits.Num() - 1; PushedIndex >= 0; --PushedIndex)
	{
		const FBaconPIEGridPushedSplit& PushedSplit = PushedSplits[PushedIndex];
		const TSharedPtr<SSplitter> Splitter = PushedSplit.Splitter.Pin();
		const TSharedPtr<SWidget> ViewportSide = PushedSplit.ViewportSide.Pin();
		const int32 SlotIndex = Splitter.IsValid() && ViewportSide.IsValid() ? BaconPIEGridSessionPrivate::FindChildIndex(*Splitter, ViewportSide) : INDEX_NONE;
		if (SlotIndex != INDEX_NONE)
		{
			Splitter->SlotAt(SlotIndex).OnSlotResized().ExecuteIfBound(PushedSplit.OriginalSizeValue);
		}
	}

	PushedSplits.Reset();
}

bool FBaconPIEGridSession::ShrinkHostViewport()
{
	const TSharedRef<SWidget> ViewportWidget = HostViewport->AsWidget();
	BaconPIEGridSessionPrivate::FCompoundContentSlot* ContentSlot = BaconPIEGridSessionPrivate::FindLevelViewportContentSlot(ViewportWidget.Get());
	if (!ContentSlot)
	{
		UE_LOG(LogBaconPIEGrid, Warning, TEXT("FBaconPIEGridSession::ShrinkHostViewport: the play viewport is a %s, not a level viewport, so the windows are tiled over it instead"), *ViewportWidget->GetTypeAsString());
		return false;
	}

	ShrunkViewport = ViewportWidget;
	OriginalViewportPadding = ContentSlot->GetPadding();
	ContentSlot->SetPadding(TAttribute<FMargin>::CreateSP(this, &FBaconPIEGridSession::GetHostViewportPadding));
	return true;
}

void FBaconPIEGridSession::RestoreHostViewport()
{
	if (const TSharedPtr<SWidget> ViewportWidget = ShrunkViewport.Pin())
	{
		if (BaconPIEGridSessionPrivate::FCompoundContentSlot* ContentSlot = BaconPIEGridSessionPrivate::FindLevelViewportContentSlot(*ViewportWidget))
		{
			ContentSlot->SetPadding(OriginalViewportPadding);
		}
	}

	ShrunkViewport.Reset();
}

FMargin FBaconPIEGridSession::GetHostViewportPadding() const
{
	const TSharedPtr<SWidget> ViewportWidget = ShrunkViewport.Pin();
	if (!ViewportWidget.IsValid())
	{
		return OriginalViewportPadding;
	}

	// Slate units of the viewport's last layout, letterboxed like the windows when the aspect ratio is locked.
	const FVector2f ViewportSize = ViewportWidget->GetTickSpaceGeometry().GetLocalSize();
	const FSlateRect CellArea = Layout.GetCellArea(0, FSlateRect(FVector2f::ZeroVector, ViewportSize), CellPadding);
	const FVector2f PictureSize = FBaconPIEGridLayout::FitToAspectRatio(CellArea.GetSize2f(), AspectRatio);
	const FVector2f PictureTopLeft = CellArea.GetTopLeft2f() + (CellArea.GetSize2f() - PictureSize) * 0.5f;
	const FVector2f PictureBottomRight = PictureTopLeft + PictureSize;

	return FMargin(
		FMath::Max(PictureTopLeft.X, 0.f),
		FMath::Max(PictureTopLeft.Y, 0.f),
		FMath::Max(ViewportSize.X - PictureBottomRight.X, 0.f),
		FMath::Max(ViewportSize.Y - PictureBottomRight.Y, 0.f));
}

void FBaconPIEGridSession::HandleCellSwitchedTo(int32 InCellIndex)
{
	// Process windows can't follow the carousel, but they never have to: a session with client processes docks only its
	// first client, so the switcher has no other client to bring in.
	if (ViewMode == EBaconPIEGridViewMode::Carousel)
	{
		MainCellIndex = InCellIndex;
		UpdateLayout();
	}
}

void FBaconPIEGridSession::HandleFocusChanging(const FFocusEvent& InFocusEvent, const FWeakWidgetPath& InOldFocusPath, const TSharedPtr<SWidget>& InOldFocusedWidget, const FWidgetPath& InNewFocusPath, const TSharedPtr<SWidget>& InNewFocusedWidget)
{
	if (!bHoldFirstClientFocus || InFocusEvent.GetUser() != FSlateApplication::Get().GetUserIndexForKeyboard())
	{
		return;
	}

	const bool bFocusesOtherClient = DockedCells.ContainsByPredicate([this, &InNewFocusPath](const TSharedPtr<SBaconPIEGridCell>& Cell)
	{
		return Cell != DockedCells[0] && InNewFocusPath.ContainsWidget(Cell.Get());
	});
	if (!bFocusesOtherClient)
	{
		return;
	}

	// Clicks and the switcher focus by mouse. Anything else is engine or game code, and since the focus change can't be
	// undone from inside this event, the first client takes it back after the tick.
	if (InFocusEvent.GetCause() == EFocusCause::Mouse)
	{
		bHoldFirstClientFocus = false;
		bRefocusFirstClient = false;
	}
	else
	{
		bRefocusFirstClient = true;
	}
}

void FBaconPIEGridSession::HandleDockedPostTick(float InDeltaTime)
{
	if (bRefocusFirstClient)
	{
		bRefocusFirstClient = false;
		DockedCells[0]->FocusViewport();
	}
}

void FBaconPIEGridSession::HandlePostTick(float InDeltaTime)
{
	// The viewport settles a frame after pushed panels move and changes with the editor's layout, so the windows track its last layout.
	CaptureGridArea(HostViewport->AsWidget()->GetTickSpaceGeometry(), FVector2f(1.f, 1.f));
	for (FBaconPIEGridTiledWindow& TiledWindow : TiledWindows)
	{
		PlaceWindow(TiledWindow);
	}
}

void FBaconPIEGridSession::PlaceWindow(FBaconPIEGridTiledWindow& InTiledWindow)
{
	const TSharedPtr<SWindow> Window = InTiledWindow.Window.Pin();
	if (!Window.IsValid() || !HasGridArea())
	{
		return;
	}

	// Only a moved cell moves its window, so one the user drags away stays there until the grid changes.
	const FSlateRect CellArea = Layout.GetCellArea(InTiledWindow.CellIndex, GridArea, CellPadding * GridAreaScale);
	if (CellArea == InTiledWindow.CellArea)
	{
		return;
	}

	InTiledWindow.CellArea = CellArea;

	// PIE windows draw their own title bar and border, sized for the DPI of the monitor the cell is on.
	const float DPIScale = FPlatformApplicationMisc::GetDPIScaleFactorAtPoint(CellArea.Left, CellArea.Top);
	const FVector2f FrameSize(Window->GetWindowSizeFromClientSize(FVector2f::ZeroVector, DPIScale));
	const FSlateRect WindowArea = BaconPIEGridSessionPrivate::FitWindowToCell(CellArea, FrameSize, AspectRatio);
	const FVector2f ClientSize = WindowArea.GetSize2f() - FrameSize;
	if (ClientSize.X < 1.f || ClientSize.Y < 1.f)
	{
		return;
	}

	Window->ReshapeWindow(WindowArea);
	UE_LOG(LogBaconPIEGrid, Verbose, TEXT("FBaconPIEGridSession::PlaceWindow: cell %d gets a %.0fx%.0f window at %.0f,%.0f"),
		InTiledWindow.CellIndex, WindowArea.GetSize2f().X, WindowArea.GetSize2f().Y, WindowArea.Left, WindowArea.Top);
}

bool FBaconPIEGridSession::HasGridArea() const
{
	return GridArea.Right > GridArea.Left && GridArea.Bottom > GridArea.Top;
}

void FBaconPIEGridSession::UpdateLayout()
{
	const FVector2f GridSize(FMath::Max(GridArea.Right - GridArea.Left, 0.f), FMath::Max(GridArea.Bottom - GridArea.Top, 0.f));
	Layout = ViewMode == EBaconPIEGridViewMode::Carousel
		? FBaconPIEGridLayout::MakeCarousel(CellContents.Num(), MainCellIndex, CarouselHeightRatio, AspectRatio)
		: FBaconPIEGridLayout::MakeGrid(CellContents.Num(), PreferredColumns, GridSize, AspectRatio);

	if (GridPanel.IsValid())
	{
		GridPanel->SetLayout(Layout);
	}
}

void FBaconPIEGridSession::BuildGrid()
{
	GridRoot = SNew(SBorder)
		.Visibility(EVisibility::Visible)
		.BorderImage(FAppStyle::GetBrush("WhiteBrush"))
		.BorderBackgroundColor(FLinearColor::Black)
		.Padding(0.f)
		// Clicks on the gaps between cells would otherwise fall through to the editor underneath.
		.OnMouseButtonDown_Lambda([](const FGeometry&, const FPointerEvent&) { return FReply::Handled(); })
		.OnMouseDoubleClick_Lambda([](const FGeometry&, const FPointerEvent&) { return FReply::Handled(); })
		[
			SAssignNew(GridPanel, SBaconPIEGridPanel)
			.Layout(Layout)
			.CellPadding(CellPadding)
		];
}

void FBaconPIEGridSession::SetCellContent(int32 InCellIndex, const TSharedRef<SWidget>& InContent)
{
	EnsureCellCount(InCellIndex + 1);
	CellContents[InCellIndex] = InContent;

	if (GridPanel.IsValid())
	{
		GridPanel->AddSlot(InCellIndex)
		[
			InContent
		];
	}
}

void FBaconPIEGridSession::EnsureCellCount(int32 InNumCells)
{
	if (InNumCells > CellContents.Num())
	{
		CellContents.SetNum(InNumCells);
		UpdateLayout();
	}
}

int32 FBaconPIEGridSession::FindFreeCellIndex() const
{
	const int32 FreeIndex = CellContents.IndexOfByPredicate([](const TSharedPtr<SWidget>& Content) { return !Content.IsValid(); });
	return FreeIndex != INDEX_NONE ? FreeIndex : CellContents.Num();
}

#undef LOCTEXT_NAMESPACE
