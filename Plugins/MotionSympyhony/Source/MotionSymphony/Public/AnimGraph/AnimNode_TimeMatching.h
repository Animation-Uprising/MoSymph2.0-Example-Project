//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimNode_SequencePlayer.h"
#include "Components/DistanceMatching.h"
#include "AnimNode_TimeMatching.generated.h"

USTRUCT(BlueprintInternalUseOnly)
struct MOTIONSYMPHONY_API FAnimNode_TimeMatching : public FAnimNode_SequencePlayer
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Inputs, meta = (PinShownByDefault, ClampMin = 0.0f))
	float DesiredTime;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings)
	float MarkerTime;

private:
	bool bInitialized;

	UDistanceMatching* DistanceMatching;
	FAnimInstanceProxy* AnimInstanceProxy;

public:
	FAnimNode_TimeMatching();

protected:
	float FindMatchingTime() const;

	// FAnimNode_Base interface
	virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
	// End of FAnimNode_Base interface
};