//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InputProfile.generated.h"


USTRUCT(BlueprintType)
struct MOTIONSYMPHONY_API FInputSet
{
	GENERATED_USTRUCT_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "InputProfile")
	FVector2D InputRemapRange;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "InputProfile")
	float SpeedMultiplier;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "InputProfile")
	float MoveResponseMultiplier;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "InputProfile")
	float TurnResponseMultiplier;

public:
	FInputSet();
};

USTRUCT(BlueprintType)
struct MOTIONSYMPHONY_API FInputProfile
{
	GENERATED_USTRUCT_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "InputProfile")
	TArray<FInputSet> InputSets;

public:
	FInputProfile();

	const FInputSet* GetInputSet(const FVector& Input);
};