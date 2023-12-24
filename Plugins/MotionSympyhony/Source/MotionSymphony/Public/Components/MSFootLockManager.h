//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MotionSymphony.h"
#include "MSFootLockManager.generated.h"

UENUM(BlueprintType)
enum class EMSFootLockState : uint8
{
	Unlocked,
	Locked,
	TimeLocked
};


USTRUCT()
struct MOTIONSYMPHONY_API FMSFootLockData
{
	GENERATED_BODY()

public:
	UPROPERTY(Transient)
	EMSFootLockState LockState = EMSFootLockState::Unlocked;

	UPROPERTY(Transient)
	float RemainingLockTime = 0.0f;

public:
	FMSFootLockData();
	FMSFootLockData(const EMSFootLockState InLockState, const float InRemainingLockTime);
};


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class MOTIONSYMPHONY_API UMSFootLockManager : public UActorComponent
{
	GENERATED_BODY()
	
public:
	UPROPERTY(Transient)
	TMap<EMSFootLockId, FMSFootLockData> FootLockMap;

public:	
	UMSFootLockManager();
	
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category = "FootLocker")
	void LockFoot(const EMSFootLockId FootId, const float Duration);

	UFUNCTION(BlueprintCallable, Category = "FootLocker")
	void UnlockFoot(const EMSFootLockId FootId);

	UFUNCTION(BlueprintCallable, Category = "FootLocker")
	bool IsFootLocked(const EMSFootLockId FootId) const;

	UFUNCTION(BlueprintCallable, Category = "FootLocker")
	void ResetLockingState();
};
