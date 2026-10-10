// © 2026 Team Bacon. All Rights Reserved.

#include "BaconPIEGridEditorEngine.h"

#include "BaconPIEGridSession.h"
#include "Engine/GameInstance.h"
#include "Framework/Notifications/NotificationManager.h"
#include "IAssetViewport.h"
#include "PlayInEditorDataTypes.h"
#include "Slate/SceneViewport.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Widgets/SViewport.h"
#include "Widgets/SWindow.h"

#define LOCTEXT_NAMESPACE "BaconPIEGridEditorEngine"

void UBaconPIEGridEditorEngine::Tick(float DeltaSeconds, bool bIdleMode)
{
	// Possess/Eject swaps the first client's viewport for an editor view, which only an SLevelViewport can host.
	if (bIsToggleBetweenPIEandSIEQueued && GridSession.IsValid() && GridSession->IsDocking())
	{
		bIsToggleBetweenPIEandSIEQueued = false;

		FNotificationInfo NotificationInfo(LOCTEXT("EjectUnavailable", "Possess/Eject is unavailable while PIE clients are docked in the grid."));
		NotificationInfo.ExpireDuration = 3.f;
		FSlateNotificationManager::Get().AddNotification(NotificationInfo);
	}

	Super::Tick(DeltaSeconds, bIdleMode);
}

void UBaconPIEGridEditorEngine::EndPlayMap()
{
	const bool bIsNestedCall = bIsEndingPlay;
	Super::EndPlayMap();

	// A nested call returns at once while the outer call is still tearing down the cells.
	if (!bIsNestedCall)
	{
		EndGridSession();
	}
}

void UBaconPIEGridEditorEngine::LaunchNewProcess(const FRequestPlaySessionParams& InParams, const int32 InInstanceNum, EPlayNetMode NetMode, bool bIsDedicatedServer)
{
	if (!bIsDedicatedServer && !GridSession.IsValid() && InParams.SessionDestination == EPlaySessionDestinationType::NewProcess)
	{
		GridSession = FBaconPIEGridSession::TryStartTiling(InParams);
	}

	FString WindowArguments;
	if (bIsDedicatedServer || !GridSession.IsValid() || !GridSession->AddProcessClient(InInstanceNum, NetMode, WindowArguments))
	{
		Super::LaunchNewProcess(InParams, InInstanceNum, NetMode, bIsDedicatedServer);
		return;
	}

	// The engine appends its own -WinX/-ResX after these parameters, and FParse::Value takes the first match.
	FRequestPlaySessionParams TiledRequest = InParams;
	TiledRequest.AdditionalStandaloneCommandLineParameters = InParams.AdditionalStandaloneCommandLineParameters.IsSet()
		? WindowArguments + TEXT(" ") + InParams.AdditionalStandaloneCommandLineParameters.GetValue()
		: WindowArguments;

	Super::LaunchNewProcess(TiledRequest, InInstanceNum, NetMode, bIsDedicatedServer);
}

void UBaconPIEGridEditorEngine::StartPlayInEditorSession(FRequestPlaySessionParams& InRequestParams)
{
	// A start that failed part way may never have reached EndPlayMap.
	EndGridSession();
	Super::StartPlayInEditorSession(InRequestParams);
}

void UBaconPIEGridEditorEngine::StartPlayInNewProcessSession(FRequestPlaySessionParams& InRequestParams)
{
	EndGridSession();
	Super::StartPlayInNewProcessSession(InRequestParams);

	// Every process has been launched by now, and their windows need nothing more from the editor.
	EndGridSession();
}

UGameInstance* UBaconPIEGridEditorEngine::CreateInnerProcessPIEGameInstance(FRequestPlaySessionParams& InParams, const FGameInstancePIEParameters& InPIEParameters, int32 InPIEInstanceIndex)
{
	if (!InPIEParameters.bRunAsDedicated)
	{
		// Only a session's first viewport may start the grid, so a late joiner never docks over a running single-client session.
		if (!GridSession.IsValid() && PlayInEditorSessionInfo.IsSet() && PlayInEditorSessionInfo->NumViewportInstancesCreated == 0)
		{
			GridSession = FBaconPIEGridSession::TryStartDocking(InParams);
			if (!GridSession.IsValid())
			{
				GridSession = FBaconPIEGridSession::TryStartTiling(InParams);
			}
		}

		// The engine gives the destination viewport to one client and clears it so the rest get windows. Every client gets a cell instead.
		if (GridSession.IsValid() && GridSession->IsDocking())
		{
			InParams.DestinationSlateViewport = TWeakPtr<IAssetViewport>(GridSession->AddDockedClient(InPIEParameters.NetMode, InPIEInstanceIndex));
		}
	}

	UGameInstance* GameInstance = Super::CreateInnerProcessPIEGameInstance(InParams, InPIEParameters, InPIEInstanceIndex);

	// A client that doesn't play in a viewport got a window from the engine, which goes on its cell.
	if (IsValid(GameInstance) && !InPIEParameters.bRunAsDedicated && GridSession.IsValid())
	{
		const FWorldContext* WorldContext = GameInstance->GetWorldContext();
		const FSlatePlayInEditorInfo* SlateInfo = WorldContext ? SlatePlayInEditorMap.Find(WorldContext->ContextHandle) : nullptr;
		const TSharedPtr<SWindow> Window = SlateInfo ? SlateInfo->SlatePlayInEditorWindow.Pin() : nullptr;
		const TSharedPtr<SViewport> ViewportWidget = Window.IsValid() && SlateInfo->SlatePlayInEditorWindowViewport.IsValid()
			? SlateInfo->SlatePlayInEditorWindowViewport->GetViewportWidget().Pin()
			: nullptr;
		if (ViewportWidget.IsValid())
		{
			GridSession->AddWindowClient(Window.ToSharedRef(), ViewportWidget.ToSharedRef());
		}
	}

	return GameInstance;
}

void UBaconPIEGridEditorEngine::OnAllPIEInstancesStarted()
{
	Super::OnAllPIEInstancesStarted();

	// The engine just made the last client the game viewport so its PIE window stays in front. Docked clients start on the first one.
	if (GridSession.IsValid())
	{
		GridSession->FocusFirstClient();
	}
}

void UBaconPIEGridEditorEngine::EndGridSession()
{
	if (GridSession.IsValid())
	{
		GridSession->End();
		GridSession.Reset();
	}
}

#undef LOCTEXT_NAMESPACE
