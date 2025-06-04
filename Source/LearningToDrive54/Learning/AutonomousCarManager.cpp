// Fill out your copyright notice in the Description page of Project Settings.


#include "AutonomousCarManager.h"
#include "LearningAgentsManager.h"

#include "Kismet/GameplayStatics.h"

#include "../ResetableVehiclePawn.h"
#include "AutonomousCarManagerComponent.h"
#include "AutonomousCarInteractor.h"
#include "AutonomousCarTrainingEnvironment.h"
#include "LearningAgentsPPOTrainer.h"
#include "LearningAgentsCommunicator.h"

AAutonomousCarManager::AAutonomousCarManager()
{
	LearningAgentsManager = CreateDefaultSubobject<UAutonomousCarManagerComponent>("Learning Agents Manager");
}

void AAutonomousCarManager::BeginPlay()
{
	Super::BeginPlay();
	OnSplineReady();
}

void AAutonomousCarManager::OnSplineReady()
{
	// Spline is ready so do the initialization
	InitializeAgents();
	InitializeManager();
}

void AAutonomousCarManager::InitializeAgents()
{
	// Get all the vehicles as agents
	TArray<AActor*> Agents;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AResetableVehiclePawn::StaticClass(), Agents);

	for (AActor* Agent : Agents)
	{
		// Make sure manager ticks first
		Agent->AddTickPrerequisiteActor(this);

		// If in inference mode, move to random spot now
		if (RunMode == EManagerModeEnum::InferenceMode)
		{
			if (AResetableVehiclePawn* VehiclePawn = Cast<AResetableVehiclePawn>(Agent); VehiclePawn != nullptr)
			{
				VehiclePawn->ResetToRandomPointOnSpline(TrackSpline, Agents);
			}
		}
	}
}

void AAutonomousCarManager::InitializeManager()
{
	const bool ReInitialize = (RunMode == EManagerModeEnum::ReInitialize);

	Interactor = Cast<UAutonomousCarInteractor>(ULearningAgentsInteractor::MakeInteractor(
		LearningAgentsManager, UAutonomousCarInteractor::StaticClass(), "Autonomous Car Interactor"));
	if (Interactor == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("Autonomous Car Manager: Failed to make interactor object."));
		return;
	}
	Interactor->TrackSpline = TrackSpline;
	Interactor->bManualTransmission = bManualTransmission;

	LearningAgentsInteractorBase = Interactor;

	Policy = ULearningAgentsPolicy::MakePolicy(LearningAgentsManager, LearningAgentsInteractorBase,
		ULearningAgentsPolicy::StaticClass(), "Learning Agents Policy",
		EncoderNeuralNetwork, PolicyNeuralNetwork, DecoderNeuralNetwork,
		ReInitialize, ReInitialize, ReInitialize,
		PolicySettings, RandomSeed);
	if (Policy == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("Autonomous Car Manager: Failed to make policy object."));
		return;
	}

	Critic = ULearningAgentsCritic::MakeCritic(LearningAgentsManager, LearningAgentsInteractorBase, Policy,
		ULearningAgentsCritic::StaticClass(), "Learning Agents Critic",
		CriticNeuralNetwork, ReInitialize, CriticSettings, RandomSeed);
	if (Critic == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("Autonomous Car Manager: Failed to make critic object."));
		return;
	}

	// Create Training Environment
	TrainingEnvironment = Cast<UAutonomousCarTrainingEnvironment>(ULearningAgentsTrainingEnvironment::MakeTrainingEnvironment(
		LearningAgentsManager, UAutonomousCarTrainingEnvironment::StaticClass(), "Autonomous Car Training Environment"));
	if (TrainingEnvironment == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("Autonomous Car Manager: Failed to make training environment object."));
		return;
	}
	TrainingEnvironment->TrackSpline = TrackSpline;
	TrainingEnvironment->bManualTransmission = bManualTransmission;

	// Setup PPO Trainer

	TrainingEnvironmentBase = TrainingEnvironment;
	FLearningAgentsCommunicator Communicator; // TODO: configure as needed
	PPOTrainer = ULearningAgentsPPOTrainer::MakePPOTrainer(
		LearningAgentsManager, LearningAgentsInteractorBase, TrainingEnvironmentBase, Policy, Critic,
		Communicator, ULearningAgentsPPOTrainer::StaticClass(), "PPO Trainer");
	if (PPOTrainer == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("Autonomous Car Manager: Failed to make PPO trainer object."));
		return;
	}
}

void AAutonomousCarManager::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (RunMode == EManagerModeEnum::InferenceMode)
	{
		if (Policy != nullptr)
		{
			Policy->RunInference();
		}
	}
	else
	{
		if (PPOTrainer != nullptr)
		{
			// TODO: Expose PPO training settings as needed
			PPOTrainer->RunTraining();
		}
	}
}
