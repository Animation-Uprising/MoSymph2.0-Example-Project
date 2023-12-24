//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EDistanceMatchingEnums.h"
#include "TagPoint.h"
#include "Tag_DistanceMarker.generated.h"

UCLASS(editinlinenew, hidecategories = (Object, TriggerSettings, Category))
class MOTIONSYMPHONY_API UTag_DistanceMarker : public UTagPoint
{
	GENERATED_UCLASS_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Match Feature|Distance Matching")
	EDistanceMatchTrigger DistanceMatchTrigger;

	UPROPERTY(EditAnywhere, Category = "Match Feature|Distance Matching")
	EDistanceMatchType DistanceMatchType;

	UPROPERTY(EditAnywhere, Category = "Match Feature|Distance Matching")
	EDistanceMatchBasis DistanceMatchBasis;
	
	UPROPERTY(EditAnywhere, Category = "Match Feature|Distance Matching", meta = (ClampMin = 0.0f))
	float Lead;

	UPROPERTY(EditAnywhere, Category = "Match Feature|Distance Matching", meta = (ClampMin = 0.0f))
	float Tail;
};