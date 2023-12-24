//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#include "AssetTypeActions_MotionMatchCalibration.h"
#include "Objects/Assets/MotionCalibration.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

FText FAssetTypeActions_MotionCalibration::GetName() const
{
	return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_MotionCalibration", "Motion Calibration");
}

FColor FAssetTypeActions_MotionCalibration::GetTypeColor() const
{
	return FColor::Blue;
}

UClass* FAssetTypeActions_MotionCalibration::GetSupportedClass() const
{
	return UMotionCalibration::StaticClass();
}

uint32 FAssetTypeActions_MotionCalibration::GetCategories()
{
	return EAssetTypeCategories::Animation;
}

void FAssetTypeActions_MotionCalibration::GetActions(const TArray<UObject*>& InObjects, FMenuBuilder & MenuBuilder)
{
	FAssetTypeActions_Base::GetActions(InObjects, MenuBuilder);

	const auto MotionCalibrationAssets = GetTypedWeakObjectPtrs<UMotionCalibration>(InObjects);
	MenuBuilder.AddMenuEntry(
		LOCTEXT("MotionMatchCalibration_Initialize", "Initialize"),
		LOCTEXT("MotionMatchCalibration", "Initializes the motion match calibration so that it complies with the configuration"),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda([=] 
				{
					for (auto& MotionMatchCalibration : MotionCalibrationAssets)
					{
						if (MotionMatchCalibration.IsValid() &&
							MotionMatchCalibration->MotionMatchConfig.Get())
						{
							MotionMatchCalibration->Modify();
							MotionMatchCalibration->Initialize();
							MotionMatchCalibration->MarkPackageDirty();
						}
					}
				})
			)
	);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("MotionMatchCalibration_Reset", "Reset"),
		LOCTEXT("MotionMatchCalibration", "Reset all calibration values to 1.0f and ratio to 0.5"),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda([=] 
				{
					if (FMessageDialog::Open(EAppMsgType::YesNo, LOCTEXT("Reset Motion Calibration",
						"Are you sure you want to reset the motion calibration so all weight values are 1.0?"))
						== EAppReturnType::Yes)
					{
						for (auto& MotionMatchCalibration : MotionCalibrationAssets)
						{
							if (MotionMatchCalibration.IsValid() &&
								MotionMatchCalibration->MotionMatchConfig.Get())
							{
								MotionMatchCalibration->Modify();
								MotionMatchCalibration->Reset();
								MotionMatchCalibration->MarkPackageDirty();
							}
						}
					}
				})
			)
	);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("MotionMatchCalibration_Reset", "Clear"),
		LOCTEXT("MotionMatchCalibration", "Clear the calibration data to have no multiplier values"),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda([=] 
				{
					if (FMessageDialog::Open(EAppMsgType::YesNo, LOCTEXT("Clear Motion Calibration",
						"Are you sure you want to clear the motion calibration?"))
						== EAppReturnType::Yes)
					{
						for (auto& MotionMatchCalibration : MotionCalibrationAssets)
						{
							if (MotionMatchCalibration.IsValid() &&
								MotionMatchCalibration->MotionMatchConfig.Get())
							{
								MotionMatchCalibration->Modify();
								MotionMatchCalibration->Clear();
								MotionMatchCalibration->MarkPackageDirty();
							}
						}
					}
				})
			)
	);
	
}

bool FAssetTypeActions_MotionCalibration::HasActions(const TArray<UObject*>& InObjects) const
{
	return false;
}

bool FAssetTypeActions_MotionCalibration::CanFilter()
{
	return true;
}

#undef LOCTEXT_NAMESPACE