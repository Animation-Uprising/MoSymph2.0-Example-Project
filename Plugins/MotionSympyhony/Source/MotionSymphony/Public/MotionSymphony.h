//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FMotionSymphonyModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

/** An enumeration for foot identification for the foot locker node*/
UENUM(BlueprintType)
enum class EMSFootLockId : uint8
{
	LeftFoot,
	RightFoot,
	Foot3,
	Foot4,
	Foot5,
	Foot6,
	Foot7,
	Foot8
};

UENUM()
enum class EMotionCalibrationType : uint8
{
	Multiplier,
	Override
};