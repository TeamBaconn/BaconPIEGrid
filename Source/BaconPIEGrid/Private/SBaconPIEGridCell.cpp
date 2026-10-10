// © 2026 Team Bacon. All Rights Reserved.

#include "SBaconPIEGridCell.h"

#include "Editor/EditorEngine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/IConsoleManager.h"
#include "Kismet2/DebuggerCommands.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Slate/SceneViewport.h"
#include "Slate/SGameLayerManager.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateTypes.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SViewport.h"
#include "Widgets/Text/STextBlock.h"

void SBaconPIEGridCell::Construct(const FArguments& InArgs, const TSharedRef<IAssetViewport>& InHostViewport)
{
	HostViewport = InHostViewport;

	static const IConsoleVariable* PropagateAlphaVariable = IConsoleManager::Get().FindConsoleVariable(TEXT("r.PostProcessing.PropagateAlpha"));

	SAssignNew(ViewportWidget, SViewport)
		.EnableGammaCorrection(false) // The scene renderer applies gamma in post processing.
		.IgnoreTextureAlpha(!PropagateAlphaVariable || !PropagateAlphaVariable->GetBool())
		[
			SAssignNew(GameLayerManager, SGameLayerManager)
			[
				SAssignNew(GameOverlay, SOverlay)
			]
		];
	ViewportWidget->SetClipping(EWidgetClipping::ClipToBoundsAlways);

	// The box shrinks the picture to the ratio and centers it; the grid's black background shows as the bars.
	const FOptionalSize AspectRatio = InArgs._AspectRatio > 0.f ? FOptionalSize(InArgs._AspectRatio) : FOptionalSize();

	ChildSlot
	[
		SNew(SBox)
		.MinAspectRatio(AspectRatio)
		.MaxAspectRatio(AspectRatio)
		[
			SNew(SOverlay)
			+ SOverlay::Slot()
			[
				SNew(SGlobalPlayWorldActions)
				[
					ViewportWidget.ToSharedRef()
				]
			]
			+ SOverlay::Slot()
			[
				SAssignNew(EditorOverlay, SOverlay)
				.Visibility(EVisibility::SelfHitTestInvisible)
			]
			+ SOverlay::Slot()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Top)
			.Padding(FMargin(8.f))
			[
				// Same rounded, see-through black plate as the level viewport's "Shift+F1 for Mouse Cursor" label.
				SNew(SBorder)
				.Visibility(InArgs._ShowLabel ? EVisibility::HitTestInvisible : EVisibility::Collapsed)
				.BorderImage(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("EditorViewportToolBar.MenuButton").Normal)
				.BorderBackgroundColor(FLinearColor::Black)
				.Padding(FMargin(6.f, 4.f))
				[
					SNew(STextBlock)
					.Text(InArgs._Label)
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
					.ColorAndOpacity(FLinearColor::White)
				]
			]
			+ SOverlay::Slot()
			[
				SAssignNew(HighlightFrame, SImage)
				.Visibility(EVisibility::Collapsed)
				.Image(GetHighlightBrush())
			]
		]
	];
}

bool SBaconPIEGridCell::HasFocusedClient() const
{
	return HasFocusedDescendants();
}

void SBaconPIEGridCell::SetHighlighted(bool bInHighlighted)
{
	HighlightFrame->SetVisibility(bInHighlighted ? EVisibility::HitTestInvisible : EVisibility::Collapsed);
}

void SBaconPIEGridCell::FocusClient()
{
	if (SceneViewport.IsValid() && !HasFocusedDescendants())
	{
		FocusGameViewport(ViewportWidget.ToSharedRef());
	}
}

void SBaconPIEGridCell::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	// Blocks game input while stopped at a breakpoint. Polled rather than bound so the widget stays non-volatile, like the engine's PIE viewport.
	ViewportWidget->SetEnabled(FSlateApplication::Get().GetNormalExecutionAttribute().Get());
}

FReply SBaconPIEGridCell::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	// Same as SLevelViewport during PIE: keys the game left unhandled stop here instead of bubbling into editor commands
	// such as F1 documentation. Play commands (Esc, Shift+F1, F8) were already offered to SGlobalPlayWorldActions below.
	return SceneViewport.IsValid() ? FReply::Handled() : FReply::Unhandled();
}

void SBaconPIEGridCell::StartPlayInEditorSession(UGameViewportClient* PlayClient, const bool bInSimulateInEditor)
{
	check(!SceneViewport.IsValid());

	SceneViewport = MakeShared<FSceneViewport>(PlayClient, ViewportWidget);
	SceneViewport->SetPlayInEditorViewport(true);
	SceneViewport->SetPlayInEditorGetsMouseControl(GetDefault<ULevelEditorPlaySettings>()->GameGetsMouseControl);
	SceneViewport->SetPlayInEditorIsSimulate(bInSimulateInEditor);

	const TSharedPtr<IAssetViewport> Host = HostViewport.Pin();
	const TSharedPtr<SWindow> HostWindow = Host.IsValid() ? FSlateApplication::Get().FindWidgetWindow(Host->AsWidget()) : nullptr;
	PlayClient->SetViewportOverlayWidget(HostWindow, GameOverlay.ToSharedRef());
	PlayClient->SetGameLayerManager(GameLayerManager);
	GameLayerManager->SetSceneViewport(SceneViewport.Get());

	ViewportWidget->SetViewportInterface(SceneViewport.ToSharedRef());
	PlayClient->Viewport = SceneViewport.Get();

	FSlateApplication::Get().RegisterGameViewport(ViewportWidget.ToSharedRef());

	const TWeakObjectPtr<UGameViewportClient> WeakPlayClient = PlayClient;
	PreviewFeatureLevelChangedHandle = CastChecked<UEditorEngine>(GEngine)->OnPreviewFeatureLevelChanged().AddLambda([WeakPlayClient](ERHIFeatureLevel::Type NewFeatureLevel)
	{
		if (UWorld* World = WeakPlayClient.IsValid() ? WeakPlayClient->GetWorld() : nullptr)
		{
			World->ChangeFeatureLevel(NewFeatureLevel);
		}
	});
}

void SBaconPIEGridCell::EndPlayInEditorSession()
{
	check(SceneViewport.IsValid());

	FSlateApplication::Get().UnregisterGameViewport();

	if (PreviewFeatureLevelChangedHandle.IsValid())
	{
		CastChecked<UEditorEngine>(GEngine)->OnPreviewFeatureLevelChanged().Remove(PreviewFeatureLevelChangedHandle);
		PreviewFeatureLevelChangedHandle.Reset();
	}

	GameLayerManager->SetSceneViewport(nullptr);
	SceneViewport->SetViewportClient(nullptr);
	SceneViewport.Reset();
}

void SBaconPIEGridCell::RegisterGameViewportIfPIE()
{
	if (SceneViewport.IsValid())
	{
		FSlateApplication::Get().RegisterGameViewport(ViewportWidget.ToSharedRef());
	}
}

bool SBaconPIEGridCell::HasPlayInEditorViewport() const
{
	return SceneViewport.IsValid();
}

TSharedPtr<FSceneViewport> SBaconPIEGridCell::GetSharedActiveViewport() const
{
	return SceneViewport;
}

FEditorViewportClient& SBaconPIEGridCell::GetAssetViewportClient()
{
	// PIE teardown restores the editor camera through this and static-casts it to FLevelEditorViewportClient, so it must be the host's real client.
	const TSharedPtr<IAssetViewport> Host = HostViewport.Pin();
	check(Host.IsValid());
	return Host->GetAssetViewportClient();
}

FViewport* SBaconPIEGridCell::GetActiveViewport()
{
	return SceneViewport.Get();
}

void SBaconPIEGridCell::AddOverlayWidget(TSharedRef<SWidget> OverlaidWidget, int32 ZOrder)
{
	EditorOverlay->AddSlot(ZOrder)
	[
		OverlaidWidget
	];
}

void SBaconPIEGridCell::RemoveOverlayWidget(TSharedRef<SWidget> OverlaidWidget)
{
	EditorOverlay->RemoveSlot(OverlaidWidget);
}

TSharedRef<const SWidget> SBaconPIEGridCell::AsWidget() const
{
	return AsShared();
}

TSharedRef<SWidget> SBaconPIEGridCell::AsWidget()
{
	return AsShared();
}

TWeakPtr<SViewport> SBaconPIEGridCell::GetViewportWidget()
{
	return ViewportWidget;
}
