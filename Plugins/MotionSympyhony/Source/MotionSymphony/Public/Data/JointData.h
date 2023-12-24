//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "JointData.generated.h"

USTRUCT(BlueprintInternalUseOnly)
struct MOTIONSYMPHONY_API FJointData
{
	GENERATED_USTRUCT_BODY()

public:
	UPROPERTY(BlueprintReadWrite, Category = "JointData")
	FVector Position;

	UPROPERTY(BlueprintReadWrite, Category = "JointData")
	FVector Velocity;

public:
	FJointData();
	FJointData(FVector Position, FVector Velocity);
	~FJointData();
	
	static void Lerp(FJointData& OutJoint, FJointData& From, 
		FJointData& To, float Progress);

	FJointData& operator += (const FJointData& rhs);
	FJointData& operator /= (const float rhs);
	FJointData& operator *= (const float rhs);
};