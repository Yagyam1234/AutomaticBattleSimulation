// Fill out your copyright notice in the Description page of Project Settings.


#include "UGameOverWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Kismet/KismetSystemLibrary.h"

bool UUGameOverWidget::Initialize()
{
	bool Success = Super::Initialize();
	if (!Success) return false;

	if (QuitButton)
	{
		QuitButton->OnClicked.AddDynamic(this, &UUGameOverWidget::OnQuitButtonClicked);
	}

	return true;
}

void UUGameOverWidget::SetWinningTeam(const FString& WinningTeamText)
{
	if (WinningTeamTextBlock)
	{
		WinningTeamTextBlock->SetText(FText::FromString(WinningTeamText));
	}
}

void UUGameOverWidget::OnQuitButtonClicked()
{
	UE_LOG(LogTemp, Warning, TEXT("Quit Button Clicked! Exiting Game."));
	UKismetSystemLibrary::QuitGame(this, nullptr, EQuitPreference::Quit, true);
}