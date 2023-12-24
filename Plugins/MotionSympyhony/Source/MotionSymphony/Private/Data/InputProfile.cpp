//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#include "Data/InputProfile.h"

FInputSet::FInputSet()
	: InputRemapRange(FVector2D(0.0f, 0.0f)),
	SpeedMultiplier(1.0f),
	MoveResponseMultiplier(1.0f),
	TurnResponseMultiplier(1.0f)
{
}

FInputProfile::FInputProfile()
{
}

const FInputSet* FInputProfile::GetInputSet(const FVector& Input)
{
	const float InputSqrMagnitude = Input.SizeSquared();

	for (const FInputSet& InputSet : InputSets)
	{
		if (InputSqrMagnitude >= (InputSet.InputRemapRange.X * InputSet.InputRemapRange.X)
			&& InputSqrMagnitude < (InputSet.InputRemapRange.Y * InputSet.InputRemapRange.Y))
		{
			return &InputSet;
		}
	}

	return nullptr;
}