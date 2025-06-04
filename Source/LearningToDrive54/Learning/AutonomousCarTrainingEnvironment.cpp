// Fill out your copyright notice in the Description page of Project Settings.

#include "AutonomousCarTrainingEnvironment.h"
#include "LearningAgentsManager.h"
#include "LearningAgentsRewards.h"
#include "Components/SplineComponent.h"
#include "../ResetableVehiclePawn.h"
#include "ChaosWheeledVehicleMovementComponent.h"

UAutonomousCarTrainingEnvironment::UAutonomousCarTrainingEnvironment() {
	TrackSpline = nullptr;
	OffTrackThreshold = 1200.0f;
	CollisionThreshold = 5;
	bManualTransmission = false;
	UpShiftAt = 3000.0f;
	DownShiftAt = 1500.0f;
}

void UAutonomousCarTrainingEnvironment::GatherAgentReward_Implementation(float& OutReward, const int32 AgentId)
{
	AResetableVehiclePawn* Agent = Cast<AResetableVehiclePawn>(Manager->GetAgent(AgentId, AWheeledVehiclePawn::StaticClass()));
	if (Agent == nullptr) {
		UE_LOG(LogTemp, Error, TEXT("TrainingEnv: Casting of agent failed."));
		return;
	}
	const UChaosWheeledVehicleMovementComponent* VehicleMovement = Cast<UChaosWheeledVehicleMovementComponent>(Agent->GetVehicleMovementComponent());
	if (VehicleMovement == nullptr) {
		UE_LOG(LogTemp, Error, TEXT("TrainingEnv: Failed to Retrieve Vehicle Movement Component."))
		return;
	}
	const float SpeedReward = ULearningAgentsRewards::MakeRewardFromVelocityAlongSpline(
		TrackSpline, Agent->GetActorLocation(), Agent->GetVelocity(), 1000.0f);
	const FVector TrackLocation = TrackSpline->FindLocationClosestToWorldLocation(Agent->GetActorLocation(), ESplineCoordinateSpace::World);
	const float OffTrackPenalty = ULearningAgentsRewards::MakeRewardOnLocationDifferenceAboveThreshold(
		Agent->GetActorLocation(), TrackLocation, OffTrackThreshold, -10.0f);
	const float CollisionPenalty = ULearningAgentsRewards::MakeReward(Agent->GetCollisionCount(), -15.0f);
	float GearshiftReward = 0.0f;
	if (bManualTransmission) {
		const int TargetGear = VehicleMovement->GetTargetGear();
		const float EngineRPMs = VehicleMovement->GetEngineRotationSpeed();
		GearshiftReward = ULearningAgentsRewards::MakeRewardOnCondition(EngineRPMs > DownShiftAt && EngineRPMs < UpShiftAt, TargetGear * 1.5f);
	}
	OutReward = (SpeedReward + OffTrackPenalty + CollisionPenalty + GearshiftReward);
}

void UAutonomousCarTrainingEnvironment::GatherAgentCompletion_Implementation(ELearningAgentsCompletion& OutCompletion, const int32 AgentId) {
	const AResetableVehiclePawn* Agent = Cast<AResetableVehiclePawn>(Manager->GetAgent(AgentId, AResetableVehiclePawn::StaticClass()));
	if (Agent == nullptr) {
		UE_LOG(LogTemp, Error, TEXT("TrainingEnv: Casting of agent failed."));
		return;
	}
	const ELearningAgentsCompletion CollisionCompletion = ULearningAgentsCompletions::MakeCompletionOnCondition(
		Agent->GetCollisionCount() > CollisionThreshold);
	if (CollisionCompletion == ELearningAgentsCompletion::Termination)
	{
		OutCompletion = ELearningAgentsCompletion::Termination;
		return;
	}
	const FVector TrackLocation = TrackSpline->FindLocationClosestToWorldLocation(Agent->GetActorLocation(), ESplineCoordinateSpace::World);
	const ELearningAgentsCompletion TrackCompletion = ULearningAgentsCompletions::MakeCompletionOnLocationDifferenceAboveThreshold(
		Agent->GetActorLocation(), TrackLocation, OffTrackThreshold);
	if (TrackCompletion == ELearningAgentsCompletion::Termination)
	{
		OutCompletion = ELearningAgentsCompletion::Termination;
	}
}

void UAutonomousCarTrainingEnvironment::ResetAgentEpisode_Implementation(const int32 AgentId) {
	AResetableVehiclePawn* Agent = Cast<AResetableVehiclePawn>(Manager->GetAgent(AgentId, AResetableVehiclePawn::StaticClass()));
	if (Agent == nullptr) {
		UE_LOG(LogTemp, Error, TEXT("TrainingEnv: Casting of agent failed."));
		return;
	}
	TArray<UObject*> AllObjects;
	TArray<int32> AllIds;
	Manager->GetAllAgents(AllObjects, AllIds, AResetableVehiclePawn::StaticClass());
	TArray<AActor*> AllAgents;
	AllAgents.Reserve(AllObjects.Num());
	for (UObject* Other : AllObjects) {
		AllAgents.Add(StaticCast<AActor*>(Other));
	}
	Agent->ResetCollisionCount();
	Agent->ResetToRandomPointOnSpline(TrackSpline, AllAgents);
}
