//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Enumerations/EMMPreProcessEnums.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Data/Trajectory.h"
#include "MMBlueprintFunctionLibrary.generated.h"

struct FMotionMatchingInputData;
class UCameraComponent;
/**
 * 
 */
UCLASS()
class MOTIONSYMPHONY_API UMMBlueprintFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:	

	UFUNCTION(BlueprintCallable, Category = "Motion Symphony")
	static FVector GetInputVectorRelativeToCamera(FVector InputVector, UCameraComponent* CameraComponent);

	UFUNCTION(BlueprintCallable, Category = "Motion Symphony")
	static FVector GetVectorRelativeToCamera(const float InputX, const float InputY, UCameraComponent* CameraComponent);

	UFUNCTION(BlueprintCallable, Category = "Motion Symphony|Trajectory")
	static void InitializeTrajectory(UPARAM(ref) FTrajectory& OutTrajectory, const int32 TrajectoryCount);

	UFUNCTION(BlueprintCallable, Category = "Motion Symphony|Trajectory")
	static void SetTrajectoryPoint(UPARAM(ref) FTrajectory& OutTrajectory, const int32 Index, const FVector Position, const float RotationZ);

	UFUNCTION(BlueprintCallable, Category = "Motion Symphony")
	static void TransformFromUpForwardAxis(UPARAM(ref) FTransform& OutTransform, const EAllAxis UpAxis, const EAllAxis ForwardAxis);

	UFUNCTION(BlueprintCallable, Category = "Motion Symphony|Trajectory", meta = (DeprecatedFunction, DepricatedMessage="Function has been deprecated Please use the ConstructMotionInputFeatureArray function instead."))
	static void CreateInputDataFromTrajectory(UPARAM(ref)FTrajectory& Trajectory, UPARAM(ref)FMotionMatchingInputData& InputData);

	UFUNCTION(BlueprintCallable, Category = "Motion Symphony|MotionMatching")
	static void ConstructMotionInputFeatureArray(UPARAM(ref)FMotionMatchingInputData& InputData, AActor* Actor, UMotionMatchConfig* MotionConfig);
};
