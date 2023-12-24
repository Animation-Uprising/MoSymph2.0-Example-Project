//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#include "MotionMatchConfigAssetFactory.h"
#include "Objects/Assets/MotionMatchConfig.h"
#include "Misc/MessageDialog.h"

#define LOCTEXT_NAMESPACE "MotionMatchConfigFactory"

UMotionMatchConfigFactory::UMotionMatchConfigFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SupportedClass = UMotionMatchConfig::StaticClass();
	bCreateNew = true;
	bEditAfterNew = true;
}

UObject* UMotionMatchConfigFactory::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn, FName CallingContext)
{
	UMotionMatchConfig* NewMMConfig = NewObject<UMotionMatchConfig>(InParent, InClass, InName, Flags);

		if (FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("Set Skeleton for Motion Matching Config",
			"Motion matching configurations need to be paired with a skeleton to function properly. Please edit the motion config asset and set the skeleton."))
			== EAppReturnType::Yes)
		{
			//SSkeletonPickerDialog::OnOpenFollowUpWindow.BindSP(NewMMConfig, &UMotionMatchConfig::OpenPickBonesDialog);
			//SSkeletonPickerDialog::ShowWindow(NewMMConfig);
		}

	return NewMMConfig;
}

bool UMotionMatchConfigFactory::ShouldShowInNewMenu() const
{
	return true;
}

#undef LOCTEXT_NAMESPACE