//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AnimationModifier.h"
#include "MotionSymphony.h"
#include "AnimMod_FootStepNotifies.generated.h"

class UAnimSequence;

USTRUCT(BlueprintType)
struct MOTIONSYMPHONYEDITOR_API FMSFootLockLimb
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FootLockMod")
	EMSFootLockId FootId;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FootLockMod")
	FName ToeName;
};

UCLASS()
class MOTIONSYMPHONYEDITOR_API UAnimMod_FootStepNotifies : public UAnimationModifier
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FootLockMod")
	TArray<FMSFootLockLimb> ToeLimbs;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FootLockMod")
	float SpeedThreshold = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FootLockMod")
	float HeightThreshold = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FootLockMod")
	float SpeedOverlap = 0.01f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FootLockMod")
	float HeightOverlap = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FootLockMod")
	float MinFootStepDuration = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FootLockMod")
	bool bAddEndNotify = false;
	
	virtual void OnApply_Implementation(UAnimSequence* AnimationSequence) override;
	virtual void OnRevert_Implementation(UAnimSequence* AnimationSequence) override;

protected:
	virtual void AnalyseToe(const FMSFootLockLimb& Limb, UAnimSequence* AnimationSequence, const int32 TrackIndex);
	virtual void AddLockNotify(const float Time, const float GroundingTime, const FMSFootLockLimb& Limb, UAnimSequence* AnimationSequence, const int32
	                           TrackIndex);
	virtual void AddUnlockNotify(const float Time, const FMSFootLockLimb& Limb, UAnimSequence* AnimationSequence, const int32 TrackIndex);
};