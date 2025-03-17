// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UGameOverWidget.generated.h"

/**
 * 
 */
UCLASS()
class UNREALSIMULATION_API UUGameOverWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// ✅ Sets the winning team message dynamically
	void SetWinningTeam(const FString& WinningTeamText);

protected:
	virtual bool Initialize() override;

private:
	// ✅ UI Components
	UPROPERTY(meta = (BindWidget))
	class UTextBlock* WinningTeamTextBlock;

	UPROPERTY(meta = (BindWidget))
	class UButton* QuitButton;

	// ✅ Callback for when Quit Button is clicked
	UFUNCTION()
	void OnQuitButtonClicked();
};
