//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PoseMatrixAABB.generated.h"

struct FPoseMatrix;

USTRUCT()
struct MOTIONSYMPHONY_API FPoseAABBMatrix
{
	GENERATED_USTRUCT_BODY()

public:
	UPROPERTY()
	float DimCount;

	UPROPERTY()
	float AABBCount;
	
	UPROPERTY()
	TArray<float> ExtentsArray;

public:
	FPoseAABBMatrix();
	FPoseAABBMatrix(const FPoseMatrix& InSearchMatrix, const int32 InBoxSize);
};
