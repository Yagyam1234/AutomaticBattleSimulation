#include "BallSimulationActor.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "UGameOverWidget.h"
#include "Interfaces/IPv4/IPv4Address.h"
#include "HAL/RunnableThread.h"
#include "Async/Async.h"
#include "Blueprint/UserWidget.h"
#include "Components/LineBatchComponent.h"

// Sets default values
ABallSimulationActor::ABallSimulationActor()
{
    PrimaryActorTick.bCanEverTick = true;
    
    LineBatchComponent = CreateDefaultSubobject<ULineBatchComponent>(TEXT("LineBatchComponent"));
    LineBatchComponent->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);

    GridSize = 0;  // Set dynamically from server
    bIsInitialized = false;
    
    Socket = nullptr;
    RecvBuffer.SetNumUninitialized(16384);
}

// Called when the game starts
void ABallSimulationActor::BeginPlay()
{
    Super::BeginPlay();
    StartSocketThread();
}

// Called when the game ends
void ABallSimulationActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    Super::EndPlay(EndPlayReason);
    StopSocketThread();
    Balls.Empty();
    if(LineBatchComponent)
    {
        LineBatchComponent->Flush();
        LineBatchComponent->RemoveFromRoot();
    }
}
// Start socket communication
void ABallSimulationActor::StartSocketThread()
{
    ConnectToServer();
}

// Stop socket communication
void ABallSimulationActor::StopSocketThread()
{
    if (Socket)
    {
        ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(Socket);
        Socket = nullptr;
    }
}
// Connect to the server
void ABallSimulationActor::ConnectToServer()
{
    ISocketSubsystem* SocketSub = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);

    if (Socket)
    {
        Socket->Close();
        SocketSub->DestroySocket(Socket);
        Socket = nullptr;
    }

    Socket = SocketSub->CreateSocket(NAME_Stream, TEXT("BallSimulation"), false);
    Socket->SetNonBlocking(true);

    FIPv4Address ServerIP;
    FIPv4Address::Parse(TEXT("127.0.0.1"), ServerIP);
    TSharedPtr<FInternetAddr> Addr = SocketSub->CreateInternetAddr();
    Addr->SetIp(ServerIP.Value);
    Addr->SetPort(8080);

    if (!Socket->Connect(*Addr))
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to Connect to Server"));
        SocketSub->DestroySocket(Socket);
        Socket = nullptr;
    }
}

// Receive data from the server
void ABallSimulationActor::ReceiveData()
{
    if (!Socket || Socket->GetConnectionState() != SCS_Connected)
        return;

    uint8 Buffer[1024];
    int32 BytesRead = 0;

    if (!Socket->Recv(Buffer, sizeof(Buffer), BytesRead))
    {
        UE_LOG(LogTemp, Warning, TEXT("Lost connection to server."));
        StopSocketThread();
        return;
    }

    FString ReceivedString = FString(ANSI_TO_TCHAR(reinterpret_cast<const char*>(Buffer)));

    // Handle initialization first, then handle updates
    if (!bIsInitialized)
    {
        InitializeClient(ReceivedString);
    }
    else
    {
        if(!ReceivedString.IsEmpty())
        {
            ParseSimulationData(ReceivedString);
        }
       
    }
}

// Step 1: Initialize client with grid size and ball data
void ABallSimulationActor::InitializeClient(const FString& Data)
{
    FScopeLock Lock(&DataMutex);

    TArray<FString> Params;
    Data.ParseIntoArray(Params, TEXT(";"), true);

    if (Params.Num() < 2)
    {
        UE_LOG(LogTemp, Error, TEXT("Invalid initialization data received: %s"), *Data);
        return;
    }

    for (const FString& Entry : Params)
    {
        TArray<FString> KeyValue;
        Entry.ParseIntoArray(KeyValue, TEXT("="), true);

        if (KeyValue.Num() == 2)
        {
            FString Key = KeyValue[0];
            FString Value = KeyValue[1];

            if (Key == "GridSize")
            {
                GridSize = FCString::Atoi(*Value);
                continue; //  Skip to next iteration
            }
        }
        else
        {
            //  Process Ball Data (Assuming: "ID,X,Y,HP,Team")
            TArray<FString> BallParams;
            Entry.ParseIntoArray(BallParams, TEXT(","), true);

            if (BallParams.Num() < 5) continue;

            int BallID = FCString::Atoi(*BallParams[0]); //  Get unique Ball ID

            FVector NewPosition = FVector(
                FMath::Clamp(FCString::Atoi(*BallParams[1]), 0, GridSize - 1) * 100,
                FMath::Clamp(FCString::Atoi(*BallParams[2]), 0, GridSize - 1) * 100,
                0
            );

            if (Balls.Contains(BallID))
            {
                //  Update existing Ball data
                FBallState& Ball = Balls[BallID];
                Ball.PrevPosition = Ball.Position;
                Ball.Position = NewPosition;
                Ball.HP = FCString::Atoi(*BallParams[3]);
                Ball.bIsRed = FCString::Atoi(*BallParams[4]) == 1;
            }
            else
            {
                //  Create new Ball and add to map
                FBallState NewBall;
                NewBall.PrevPosition = NewPosition;
                NewBall.Position = NewPosition;
                NewBall.HP = FCString::Atoi(*BallParams[3]);
                NewBall.bIsRed = FCString::Atoi(*BallParams[4]) == 1;

                Balls.Add(BallID, NewBall);
            }
        }
    }

    if (GridSize > 0)
    {
        PreallocateGridLines();
        DrawGrid();
    }

    UE_LOG(LogTemp, Log, TEXT("Client Initialized - GridSize: %d, BallCount: %d"), GridSize, Balls.Num());
    bIsInitialized = true;
}


void ABallSimulationActor::ShowGameOverWidget(const FString& WinningTeamMessage)
{
    if (!GameOverWidgetClass) {
        UE_LOG(LogTemp, Error, TEXT("GameOverWidgetClass is not set!"));
        return;
    }
    // Sanitize the message to remove predefined text
    // Clean up the WinningTeamMessage
    FString SanitizedMessage = WinningTeamMessage;
    SanitizedMessage.RemoveFromStart(TEXT("Blue Team Wins!"));
    SanitizedMessage.RemoveFromStart(TEXT("Red Team Wins!"));
    SanitizedMessage = SanitizedMessage.TrimStartAndEnd();


    APlayerController* PC = GetWorld()->GetFirstPlayerController();
    if (PC && PC->IsLocalController()) {
        UUGameOverWidget* GameOverWidget = CreateWidget<UUGameOverWidget>(PC, GameOverWidgetClass);
        if (GameOverWidget) {
            GameOverWidget->SetWinningTeam(WinningTeamMessage);
            GameOverWidget->AddToViewport();
        }
    }
}

// Step 2: Parse simulation updates & remove dead balls
void ABallSimulationActor::ParseSimulationData(const FString& Data) {
    FScopeLock Lock(&DataMutex);

    if (Data.Contains("GameOver:")) {
        FString WinningTeamMessage = Data.RightChop(9);
    
        // Clear simulation state
        Balls.Empty();
    
        // Log game over event
        UE_LOG(LogTemp, Warning, TEXT("[Client] GAME OVER! %s"), *WinningTeamMessage);

        //  Show Game Over Widget
        ShowGameOverWidget(WinningTeamMessage);
    
        return;
    }

    TArray<FString> Updates;
    Data.ParseIntoArray(Updates, TEXT(";"), true);

    //  Track existing balls before processing new data
    TSet<int> ExistingBallIDs;
    for (const auto& BallPair : Balls) {
        ExistingBallIDs.Add(BallPair.Key);
    }

    for (const FString& Entry : Updates) {
        TArray<FString> BallParams;
        Entry.ParseIntoArray(BallParams, TEXT(","), true);
        if (BallParams.Num() < 5) continue;

        int BallID = FCString::Atoi(*BallParams[0]);
        FVector NewPosition(
            FCString::Atoi(*BallParams[1]) * 100,
            FCString::Atoi(*BallParams[2]) * 100,
            0
        );
        int NewHP = FCString::Atoi(*BallParams[3]);

        //  If HP is zero, remove the ball
        if (NewHP <= 0) {
            if (Balls.Contains(BallID)) {
                UE_LOG(LogTemp, Warning, TEXT("[Client] Ball ID %d removed (HP 0)."), BallID);
                Balls.Remove(BallID);
            }
            continue; // Skip further processing for this ball
        }

        if (Balls.Contains(BallID)) {
            FBallState& Ball = Balls[BallID];

            if (Ball.Position != NewPosition) {
                Ball.PrevPosition = Ball.Position; //  Keep for interpolation
                Ball.Position = NewPosition;
            }
            Ball.HP = NewHP;
        } else {
            //  Add new ball if it doesn’t exist
            FBallState NewBall;
            NewBall.PrevPosition = NewPosition;
            NewBall.Position = NewPosition;
            NewBall.HP = NewHP;
            NewBall.bIsRed = FCString::Atoi(*BallParams[4]) == 1;
            Balls.Add(BallID, NewBall);
        }

        //  Mark BallID as still existing
        ExistingBallIDs.Remove(BallID);
    }

    //  Remove balls that no longer exist in server updates
    for (int RemovedID : ExistingBallIDs) {
        UE_LOG(LogTemp, Warning, TEXT("[Client] Ball ID %d removed (Not in server data)."), RemovedID);
        Balls.Remove(RemovedID);
    }

    bNewDataAvailable = true; //  Trigger Tick update
}



// Draw balls in the scene
void ABallSimulationActor::DrawBalls() {
    for (const auto& BallPair : Balls) {
        int BallID = BallPair.Key;
        const FBallState& Ball = BallPair.Value;

        //  Ensure we're not drawing balls with HP <= 0 (should already be removed)
        if (Ball.HP <= 0) continue;

        //  Interpolating position for smooth transition
        FVector InterpPosition = FMath::Lerp(Ball.PrevPosition, Ball.Position, InterpFactor);

        FColor BallColor = Ball.bIsRed ? FColor::Red : FColor::Blue;

        //  Draw interpolated sphere
        DrawDebugSphere(GetWorld(), InterpPosition, 50, 12, BallColor, false, 0.0f, 0, 3);
        
        //  Display Ball ID and HP above it
        FVector HPPosition = InterpPosition + FVector(0, 0, 60);
        FString HPText = FString::Printf(TEXT("ID: %d | HP: %d"), BallID, Ball.HP);
        DrawDebugString(GetWorld(), HPPosition, HPText, nullptr, FColor::White, 0.0f, true);

        //  Debugging Log
        UE_LOG(LogTemp, Log, TEXT("[DrawBalls] Ball ID: %d | Interpolated Pos: (%.1f, %.1f) | Target Pos: (%.1f, %.1f)"),
               BallID, InterpPosition.X, InterpPosition.Y, Ball.Position.X, Ball.Position.Y);
    }
}




// Fix grid drawing
void ABallSimulationActor::PreallocateGridLines()
{
    GridLineStartPoints.Empty();
    GridLineEndPoints.Empty();

    if (GridSize <= 0) return;

    for (int i = 0; i <= GridSize; i++)
    {
        FVector StartX(i * 100, 0, 0);
        FVector EndX(i * 100, GridSize * 100, 0);
        FVector StartY(0, i * 100, 0);
        FVector EndY(GridSize * 100, i * 100, 0);

        GridLineStartPoints.Add(StartX);
        GridLineEndPoints.Add(EndX);
        GridLineStartPoints.Add(StartY);
        GridLineEndPoints.Add(EndY);
    }

    UE_LOG(LogTemp, Log, TEXT("Grid lines preallocated: %d lines"), GridLineStartPoints.Num());
}


void ABallSimulationActor::DrawGrid()
{
    if (!LineBatchComponent || GridSize <= 0 || GridLineStartPoints.Num() == 0) return;

    LineBatchComponent->Flush(); // Clears previous lines

    for (int32 i = 0; i < GridLineStartPoints.Num(); i++)
    {
        LineBatchComponent->DrawLine(GridLineStartPoints[i], GridLineEndPoints[i], 
                                     FColor::White, 10.0f, 5.0f);
    }

    UE_LOG(LogTemp, Log, TEXT("Grid drawn successfully."));
}

// Tick function to update visuals
void ABallSimulationActor::Tick(float DeltaTime) {
    Super::Tick(DeltaTime);
    ReceiveData();

    if (!bIsInitialized || Balls.Num() == 0)
        return;

    float CurrentTime = GetWorld()->GetTimeSeconds();

    //  Only update positions when new data arrives
    if (bNewDataAvailable) {
        bNewDataAvailable = false;

        //  Instead of resetting, calculate the time between updates
        float DeltaUpdateTime = CurrentTime - LastUpdateTime;
        LastUpdateTime = CurrentTime;

        //  Set interpolation factor to allow smooth movement between updates
        InterpFactor = 0.0f;
        InterpSpeed = 1.0f / DeltaUpdateTime;  //  Adjust interpolation speed dynamically
    }

    //  Continuously increase interpolation factor until next update
    InterpFactor = FMath::Clamp(InterpFactor + (DeltaTime * InterpSpeed), 0.0f, 1.0f);

    //  Draw Balls with continuously interpolated positions
    DrawBalls();
    
}
