//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#include "MotionMatchCalibrationAssetFactory.h"
#include "Objects/Assets/MotionCalibration.h"
#include "Misc/MessageDialog.h"

#define LOCTEXT_NAMESPACE "MotionMatchCalibrationFactory"

UMotionCalibrationFactory::UMotionCalibrationFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SupportedClass = UMotionCalibration::StaticClass();
	bCreateNew = true;
	bEditAfterNew = true;
}

UObject* UMotionCalibrationFactory::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn, FName CallingContext)
{
	UMotionCalibration* NewMMCalibration = NewObject<UMotionCalibration>(InParent, InClass, InName, Flags);

	FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("Bind motion calibration to motion configuration",
		"Motion matching calibration needs to be paired with a motion config asset to function properly. Please edit the calibration."));

	return NewMMCalibration;
}

bool UMotionCalibrationFactory::ShouldShowInNewMenu() const
{
	return true;
}

#undef LOCTEXT_NAMESPACE