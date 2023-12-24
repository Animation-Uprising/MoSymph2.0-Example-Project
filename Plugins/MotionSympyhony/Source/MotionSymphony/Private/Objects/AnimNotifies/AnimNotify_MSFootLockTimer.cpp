//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#include "AnimNotifies/AnimNotify_MSFootLockTimer.h"
#include "MSFootLockManager.h"

void UAnimNotify_MSFootLockTimer::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
                                       const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);
	
	if(!MeshComp
	   || !Animation)
	{
		return;
	}

	AActor* Owner = MeshComp->GetOwner();
	if(!Owner)
	{
		return;
	}
	
	if(UMSFootLockManager* FootLockManager = Cast<UMSFootLockManager>(Owner->GetComponentByClass(UMSFootLockManager::StaticClass())))
	{
		FootLockManager->LockFoot(FootLockId, GroundingTime);
	}
}