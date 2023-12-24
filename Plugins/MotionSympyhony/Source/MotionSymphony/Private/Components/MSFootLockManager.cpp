//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#include "MSFootLockManager.h"

FMSFootLockData::FMSFootLockData()
{
}

FMSFootLockData::FMSFootLockData(const EMSFootLockState InLockState, const float InRemainingLockTime)
	: LockState(InLockState), RemainingLockTime(InRemainingLockTime)
{
	
}

// Sets default values for this component's properties
UMSFootLockManager::UMSFootLockManager()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	FootLockMap.Empty(static_cast<int32>(EMSFootLockId::Foot8) + 1);
}

// Called every frame
void UMSFootLockManager::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	//Update the lock timers on on valid feet and unlock them if necessary
	for(TPair<EMSFootLockId, FMSFootLockData>& FootLockPair : FootLockMap)
	{
		FMSFootLockData& FootLockData = FootLockPair.Value;

		if(FootLockData.LockState == EMSFootLockState::TimeLocked)
		{
			FootLockData.RemainingLockTime -= DeltaTime;

			if(FootLockData.RemainingLockTime < 0.0f)
			{
				FootLockData.LockState = EMSFootLockState::Unlocked;
				FootLockData.RemainingLockTime = 0.0f;
			}
		}
	}
}

void UMSFootLockManager::LockFoot(const EMSFootLockId FootId, const float Duration)
{
	FMSFootLockData& FootLockData = FootLockMap.FindOrAdd(FootId);

	if(Duration > 0.0f)
	{
		FootLockData.LockState = EMSFootLockState::TimeLocked;
	}
	else
	{
		FootLockData.LockState = EMSFootLockState::Locked;
	}

	FootLockData.RemainingLockTime = Duration;
}

void UMSFootLockManager::UnlockFoot(const EMSFootLockId FootId)
{
	if(FMSFootLockData* FootLockData = FootLockMap.Find(FootId))
	{
		FootLockData->LockState = EMSFootLockState::Unlocked;
		FootLockData->RemainingLockTime = 0.0f;
	}
}

bool UMSFootLockManager::IsFootLocked(const EMSFootLockId FootId) const
{
	if(const FMSFootLockData* FootLockData = FootLockMap.Find(FootId))
	{
		return FootLockData->LockState > EMSFootLockState::Unlocked;
	}

	return false;
}

void UMSFootLockManager::ResetLockingState()
{
	for(TPair<EMSFootLockId, FMSFootLockData>& FootLockPair : FootLockMap)
	{
		FMSFootLockData& FootLockData = FootLockPair.Value;

		FootLockData.LockState = EMSFootLockState::Unlocked;
		FootLockData.RemainingLockTime = 0.0f;
	}
}

