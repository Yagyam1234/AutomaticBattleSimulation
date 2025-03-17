// Fill out your copyright notice in the Description page of Project Settings.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HAL/Runnable.h"
#include "Json.h"
#include "BallSimulationActor.generated.h"

USTRUCT(BlueprintType)
struct FBallState
{
	GENERATED_BODY()

	FVector Position;  // Current position of the ball
	FVector PrevPosition;  // Previous position for smooth interpolation
	int HP;  // Health points
	bool bIsRed;  // True if it's a red ball, false if blue

	FBallState() 
		: Position(FVector::ZeroVector), PrevPosition(FVector::ZeroVector), HP(0), bIsRed(false) {}
};

UCLASS()
class UNREALSIMULATION_API ABallSimulationActor : public AActor
{
	GENERATED_BODY()
    
public:    
	// Sets default values for this actor's properties
	ABallSimulationActor();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:    
	virtual void Tick(float DeltaTime) override;

	// Grid size (dynamically received from the server)
	int GridSize;

	
	// Socket communication
	void StartSocketThread();
	void StopSocketThread();
	void ConnectToServer();
	void ReceiveData();
    
	// Handling Initialization
	void InitializeClient(const FString& Data);
    
	// Handling Simulation Updates
	void ParseSimulationData(const FString& Data);

	// Visualization
	void DrawBalls();
	void DrawGrid();

	TMap<int, FBallState> Balls; // ✅ Store Balls in a TMap instead of clearing them
	bool bIsInitialized;

private:
	// Networking
	FSocket* Socket;
	TArray<uint8> RecvBuffer;
    
	// Mutex for thread safety
	FCriticalSection DataMutex;
	float InterpFactor = 0.0f;
	float InterpSpeed = 10.0f;
	// Simulation state

	bool bNewDataAvailable = false;
	

	// For smooth visualization
	float LastUpdateTime;

	// Grid visualization
	TArray<FVector> GridLineStartPoints;
	TArray<FVector> GridLineEndPoints;

	// Helper function to preallocate grid lines
	void PreallocateGridLines();
	
};