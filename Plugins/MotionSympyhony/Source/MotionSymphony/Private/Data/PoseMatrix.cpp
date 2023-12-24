//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#include "Data/PoseMatrix.h"
#include "MotionDataAsset.h"

FPoseMatrix::FPoseMatrix(int32 InPoseCount, int32 InAtomCount)
{
	PoseArray.Empty(InPoseCount * InAtomCount + 1);
}

FPoseMatrix::FPoseMatrix()
	: PoseCount(0),
	AtomCount(0)
{
}

float& FPoseMatrix::GetAtom(int32 PoseId, int32 AtomId)
{
	return PoseArray[PoseId * AtomCount + AtomId];
}

const float& FPoseMatrix::GetAtom(int32 PoseId, int32 AtomId) const
{
	return PoseArray[PoseId * AtomCount + AtomId];
}

FPoseMatrixSection::FPoseMatrixSection()
	: StartIndex(-1),
	EndIndex(-1)
{
}

FPoseMatrixSection::FPoseMatrixSection(int32 InStartIndex, int32 InEndIndex)
	: StartIndex(InStartIndex),
	EndIndex(InEndIndex)
{
}
