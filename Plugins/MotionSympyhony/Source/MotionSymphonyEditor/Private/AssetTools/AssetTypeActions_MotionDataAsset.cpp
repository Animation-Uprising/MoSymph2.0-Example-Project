//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#include "AssetTypeActions_MotionDataAsset.h"
#include "Objects/Assets/MotionDataAsset.h"
#include "MotionPreProcessToolkit.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

FText FAssetTypeActions_MotionDataAsset::GetName() const
{
	return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_MotionData", "Motion Data");
}

FColor FAssetTypeActions_MotionDataAsset::GetTypeColor() const
{
	return FColor::Blue;
}

UClass * FAssetTypeActions_MotionDataAsset::GetSupportedClass() const
{
	return UMotionDataAsset::StaticClass();
}

void FAssetTypeActions_MotionDataAsset::OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor)
{
	EToolkitMode::Type Mode = EditWithinLevelEditor.IsValid()
		? EToolkitMode::WorldCentric
		: EToolkitMode::Standalone;

	for (auto ObjIt = InObjects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		if (UMotionDataAsset* MotionDataAsset = Cast<UMotionDataAsset>(*ObjIt))
		{
			TSharedRef<FMotionPreProcessToolkit> EditorToolkit = MakeShareable(new FMotionPreProcessToolkit());
			EditorToolkit->Initialize(MotionDataAsset, Mode, EditWithinLevelEditor);
		}
	}
}

uint32 FAssetTypeActions_MotionDataAsset::GetCategories()
{
	return EAssetTypeCategories::Animation;
}

void FAssetTypeActions_MotionDataAsset::GetActions(const TArray<UObject*>& InObjects, FMenuBuilder & MenuBuilder)
{
	FAssetTypeActions_Base::GetActions(InObjects, MenuBuilder);

	auto MotionPreProcessors = GetTypedWeakObjectPtrs<UMotionDataAsset>(InObjects);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("MotionDataAsset_RunPreProcess", "Run Pre-Process"),
		LOCTEXT("MotionDataAsset_RunPreProcessToolTip", "Runs the pre-processing algorithm on the data in this pre-processor with optimisation if possible."),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda([=] 
				{
					for (auto& MotionData : MotionPreProcessors)
					{
						if (MotionData.IsValid() &&
							MotionData.Get()->CheckValidForPreProcess())
						{
							MotionData.Get()->Modify();
							MotionData.Get()->PreProcess();
							MotionData.Get()->MarkPackageDirty();
						}
					}
				}),
			FCanExecuteAction::CreateLambda([=] 
				{
					return true;
				})
			)
	);
}

bool FAssetTypeActions_MotionDataAsset::HasActions(const TArray<UObject*>& InObjects) const
{
	return true;
}

bool FAssetTypeActions_MotionDataAsset::CanFilter()
{
	return true;
}

#undef LOCTEXT_NAMESPACE