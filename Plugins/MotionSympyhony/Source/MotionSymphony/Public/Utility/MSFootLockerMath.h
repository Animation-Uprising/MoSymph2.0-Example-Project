// Copyright 2020-2023 Kenneth Claassen, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstanceProxy.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "MSFootLockerMath.generated.h"

UCLASS()
class MOTIONSYMPHONY_API UMSFootLockerMath : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	/** Returns the shortest angle (degrees) beteen two vectors */
	UFUNCTION(BlueprintCallable, Category = "FootLocker Math")
	static float AngleBetween(const FVector& A, const FVector& B);

	static float GetPointOnPlane(const FVector& InPoint, const FVector& SlopeNormal, const FVector& SlopeLocation);
	static FVector GetBoneWorldLocation(const FTransform& InBoneTransform_CS, FAnimInstanceProxy* InAnimInstanceProxy);
	static FVector GetBoneWorldLocation(const FVector& InBoneLocation_CS, FAnimInstanceProxy* InAnimInstanceProxy);
};