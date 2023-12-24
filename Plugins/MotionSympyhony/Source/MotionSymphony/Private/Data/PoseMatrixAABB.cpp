//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#include "Data/PoseMatrixAABB.h"
#include "PoseMatrix.h"

FPoseAABBMatrix::FPoseAABBMatrix()
	: DimCount(0),
	  AABBCount(0)
{
}

FPoseAABBMatrix::FPoseAABBMatrix(const FPoseMatrix& InSearchMatrix, const int32 InBoxSize)
	: DimCount(0),
      AABBCount(0)
{
	const float AtomCount = InSearchMatrix.AtomCount;
	const float PoseCount = InSearchMatrix.PoseCount;
	const TArray<float>& PoseArray_SM = InSearchMatrix.PoseArray;

	AABBCount = FMath::CeilToInt32(PoseCount / static_cast<float>(InBoxSize));

	//Initialize the extents array so that the first pose to be checked will become the AABB bounds
	ExtentsArray.SetNumZeroed(AABBCount * AtomCount * 2);
	for (int32 i = 0; i < ExtentsArray.Num(); i += 2)
	{
		ExtentsArray[i] = FLT_MAX; //Minimum Extent
		ExtentsArray[i + 1] = -FLT_MAX; //Maximum Extent
	}

	//Iterate through AABBs
	for (int32 AABBIndex = 0; AABBIndex < AABBCount; ++AABBIndex)
	{
		const int32 StartPoseIndex = AABBIndex * InBoxSize;
		const int32 EndPoseIndex = FMath::Min(StartPoseIndex + InBoxSize, PoseCount - 1);
		const float AABBPoseIndex = AABBIndex * InBoxSize;

		//Iterate through Poses
		for (int32 PoseIndex = StartPoseIndex; PoseIndex < EndPoseIndex; ++PoseIndex)
		{
			const float StartAtomIndex = PoseIndex * AtomCount;
			const float EndAtomIndex = StartAtomIndex + AtomCount;

			//Iterate through atoms
			int32 ExtentsAtomIndex = AABBIndex * (AtomCount * 2);
			for (int32 AtomIndex = StartAtomIndex; AtomIndex < EndAtomIndex; ++AtomIndex)
			{
				const float AtomValue = PoseArray_SM[AtomIndex];

				//Determine if this atom creates the new minimum bound
				if (AtomValue < ExtentsArray[ExtentsAtomIndex])
				{
					ExtentsArray[ExtentsAtomIndex] = AtomValue;
				}

				//Determine if this atom creates the new maximum bound
				if (AtomValue > ExtentsArray[ExtentsAtomIndex + 1])
				{
					ExtentsArray[ExtentsAtomIndex + 1] = AtomValue;
				}

				ExtentsAtomIndex += 2;
			}
		}
	}
}
