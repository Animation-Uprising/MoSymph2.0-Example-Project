//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#include "MotionDataAssetFactory.h"

UMotionDataAssetFactory::UMotionDataAssetFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SupportedClass = UMotionDataAsset::StaticClass();
	bCreateNew = true;
	bEditAfterNew = true;
}

UObject* UMotionDataAssetFactory::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn, FName CallingContext)
{
	return NewObject<UMotionDataAsset>(InParent, InClass, InName, Flags);
}

bool UMotionDataAssetFactory::ShouldShowInNewMenu() const
{
	return true;
}