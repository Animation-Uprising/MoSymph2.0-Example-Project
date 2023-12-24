//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/TrajectoryGenerator_Base.h"
#include "Enumerations/EMotionMatchingEnums.h"
#include "TrajectoryGenerator.generated.h"

/**
 * 
 */
UCLASS(BlueprintType, Category = "Motion Matching", meta = (BlueprintSpawnableComponent))
class MOTIONSYMPHONY_API UTrajectoryGenerator : public UTrajectoryGenerator_Base
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	FVector StrafeDirection;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings|Spring")
	float MaxSpeed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings|Spring")
	float MoveResponse;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings|Spring")
	float TurnResponse;

	//UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings)
	bool bUseTurnDecay = false;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviour")
	bool bResetDirectionOnIdle;

	/** Option of spring based trajectory model or based on UE5 CharacterMovement*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviour")
	ETrajectoryModel TrajectoryModel;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviour")
	ETrajectoryMoveMode TrajectoryBehaviour;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviour")
	ETrajectoryControlMode TrajectoryControlMode;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviour")
	bool bUsePathAsTrajectoryForAI = false;
	
private:
	TArray<FVector> NewTrajPosition;
	float LastDesiredOrientation;

	float MoveResponse_Remapped;
	float TurnResponse_Remapped;

	class UCharacterMovementComponent* CharacterMovement;

public:
	UTrajectoryGenerator();

protected:
	virtual void UpdatePrediction(float DeltaTime) override;
	virtual void PathFollowPrediction(const float DeltaTime, const int32 Iterations, const FVector& DesiredLinearDisplacement);
	virtual void InputPrediction(const float DeltaTime, const FVector& DesiredLinearDisplacement);
	virtual void CapsulePrediction(const float DeltaTime);
	virtual void Setup(TArray<float>& TrajTimes) override;
	virtual bool IsValidToUpdatePrediction() override;

	UFUNCTION(BlueprintCallable, Category = "MotionSymphony|TrajectoryGenerator")
	void SetStrafeDirectionFromCamera(UCameraComponent* Camera);

private:
	void CalculateDesiredLinearVelocity(FVector& OutVelocity);
	void CalculateInputVectorFromAINavAgent();

#if WITH_EDITOR
	virtual void DebugDrawTrajectory(const float InDeltaTime) override;
#endif
};