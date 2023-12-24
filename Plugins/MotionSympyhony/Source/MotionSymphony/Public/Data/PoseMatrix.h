//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PoseMatrix.generated.h"

class UMotionDataAsset;
/**
 * 
 */
USTRUCT()
struct MOTIONSYMPHONY_API FPoseMatrix
{
	GENERATED_BODY()
	
public:
	/** The number of poses*/
	UPROPERTY()
	int32 PoseCount;

	/** The number of atoms per pose*/
	UPROPERTY()
	int32 AtomCount;

	/** The animation matrix */
	UPROPERTY()
	TArray<float> PoseArray;
	
	FPoseMatrix(int32 InPoseCount, int32 InAtomCont);
	FPoseMatrix();

	float& GetAtom(int32 PoseId, int32 AtomId);
	const float& GetAtom(int32 PoseId, int32 AtomId) const;
};

USTRUCT()
struct MOTIONSYMPHONY_API FPoseMatrixSection
{
	GENERATED_BODY()

public:
	UPROPERTY()
	int32 StartIndex;

	UPROPERTY()
	int32 EndIndex;

public:
	FPoseMatrixSection();
	FPoseMatrixSection(int32 InStartIndex, int32 InEndIndex);
};