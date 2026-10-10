// © 2026 Team Bacon. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Editor/UnrealEdEngine.h"
#include "BaconPIEGridEditorEngine.generated.h"

class FBaconPIEGridSession;

/**
 * Docks multi-client PIE into a grid over the play viewport instead of opening a window per client, and puts the windows of
 * clients that aren't docked on the same grid. The plugin's Config/Engine.ini makes it the editor engine while the plugin is
 * enabled.
 */
UCLASS()
class BACONPIEGRID_API UBaconPIEGridEditorEngine : public UUnrealEdEngine
{
	GENERATED_BODY()

public:
	virtual void Tick(float DeltaSeconds, bool bIdleMode) override;
	virtual void EndPlayMap() override;
	virtual void LaunchNewProcess(const FRequestPlaySessionParams& InParams, const int32 InInstanceNum, EPlayNetMode NetMode, bool bIsDedicatedServer) override;

protected:
	virtual void StartPlayInEditorSession(FRequestPlaySessionParams& InRequestParams) override;
	virtual void StartPlayInNewProcessSession(FRequestPlaySessionParams& InRequestParams) override;
	virtual UGameInstance* CreateInnerProcessPIEGameInstance(FRequestPlaySessionParams& InParams, const FGameInstancePIEParameters& InPIEParameters, int32 InPIEInstanceIndex) override;
	virtual void OnAllPIEInstancesStarted() override;

private:
	void EndGridSession();

private:
	TSharedPtr<FBaconPIEGridSession> GridSession;
};
