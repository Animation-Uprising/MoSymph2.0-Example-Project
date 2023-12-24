//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MotionSymphony.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Runtime/Launch/Resources/Version.h"
#include "AnimNotify_MSFootLockSingle.generated.h"

/**
 * 
 */
UCLASS()
class MOTIONSYMPHONY_API UAnimNotify_MSFootLockSingle : public UAnimNotify
{
	GENERATED_BODY()
	
public:
	/** Which foot should be locked by this notify*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Foot Locker")
	EMSFootLockId FootLockId;

	/** How long should the foot be locked for in seconds*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Foot Locker", meta = (ClampMin = 0.0f))
	bool bSetLocked;

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
			const FAnimNotifyEventReference& EventReference) override;
};