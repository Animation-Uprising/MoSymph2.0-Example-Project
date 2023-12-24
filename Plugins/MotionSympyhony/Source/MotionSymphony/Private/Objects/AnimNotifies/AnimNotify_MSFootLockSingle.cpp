//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#include "AnimNotifies/AnimNotify_MSFootLockSingle.h"
#include "MSFootLockManager.h"

void UAnimNotify_MSFootLockSingle::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
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
		if(bSetLocked)
		{
			FootLockManager->LockFoot(FootLockId, -1.0f);
		}
		else
		{
			FootLockManager->UnlockFoot(FootLockId); 
		}
	}
}
