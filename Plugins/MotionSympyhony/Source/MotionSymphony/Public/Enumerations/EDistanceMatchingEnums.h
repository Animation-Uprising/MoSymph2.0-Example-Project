//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EDistanceMatchingEnums.generated.h"

UENUM(BlueprintType)
enum class EDistanceMatchType : uint8
{
	None,
	Backward,
	Forward,
	Both,
};

UENUM(BlueprintType)
enum class EDistanceMatchBasis : uint8
{
	Positional,
	Rotational
};

UENUM(BlueprintType)
enum class EDistanceMatchTrigger : uint8
{
	None,
	Start,
	Stop,
	Plant,
	Jump,
	TurnInPlace,
	Pivot
};