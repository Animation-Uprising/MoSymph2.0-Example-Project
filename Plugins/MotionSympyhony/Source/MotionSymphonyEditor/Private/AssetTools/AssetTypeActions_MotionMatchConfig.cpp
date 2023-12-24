//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#include "AssetTypeActions_MotionMatchConfig.h"
#include "Objects/Assets/MotionMatchConfig.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

FText FAssetTypeActions_MotionMatchConfig::GetName() const
{
	return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_MotionMatchConfig", "Motion Match Config");
}

FColor FAssetTypeActions_MotionMatchConfig::GetTypeColor() const
{
	return FColor::Blue;
}

UClass * FAssetTypeActions_MotionMatchConfig::GetSupportedClass() const
{
	return UMotionMatchConfig::StaticClass();
}

uint32 FAssetTypeActions_MotionMatchConfig::GetCategories()
{
	return EAssetTypeCategories::Animation;
}
//
//void FAssetTypeActions_MotionMatchConfig::GetActions(const TArray<UObject*>& InObjects, FMenuBuilder & MenuBuilder)
//{
//	FAssetTypeActions_Base::GetActions(InObjects, MenuBuilder);
//
//	auto MotionPreProcessors = GetTypedWeakObjectPtrs<UMotionDataAsset>(InObjects);
//
//	MenuBuilder.AddMenuEntry(
//		LOCTEXT("MotionDataAsset_RunPreProcess", "Run Pre-Process"),
//		LOCTEXT("MotionDataAsset_RunPreProcessToolTip", "Runs the pre-processing algorithm on the data in this pre-processor."),
//		FSlateIcon(),
//		FUIAction(
//			FExecuteAction::CreateLambda([=] {
//
//			}),
//			FCanExecuteAction::CreateLambda([=] {
//				return true;
//			})
//		)
//	);
//}

bool FAssetTypeActions_MotionMatchConfig::HasActions(const TArray<UObject*>& InObjects) const
{
	return false;
}

bool FAssetTypeActions_MotionMatchConfig::CanFilter()
{
	return true;
}

#undef LOCTEXT_NAMESPACE