//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#include "AnimGraphNode_RootMotionDecoupler.h"

#include "DetailLayoutBuilder.h"
#include "Animation/AnimRootMotionProvider.h"

#define LOCTEXT_NAMESPACE "MoSymphNodes"

UAnimGraphNode_RootMotionDecoupler::UAnimGraphNode_RootMotionDecoupler(const FObjectInitializer& ObjectInitializer)
	:Super(ObjectInitializer)
{
	
}

FText UAnimGraphNode_RootMotionDecoupler::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return GetControllerDescription();
}

FLinearColor UAnimGraphNode_RootMotionDecoupler::GetNodeTitleColor() const
{
	return FLinearColor(FColor(153.0f, 0.0f, 0.0f));
}

FText UAnimGraphNode_RootMotionDecoupler::GetTooltipText() const
{
	return LOCTEXT("RootMotionDecouplerTooltip", "Decouples the animation mesh movement from the character controller"
		" movement with root motion. The animation mesh is clamped to the character controller so it cannot stray too far.");
}

void UAnimGraphNode_RootMotionDecoupler::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()
	Super::CustomizeDetails(DetailBuilder);

	TSharedRef<IPropertyHandle> NodeHandle = DetailBuilder.GetProperty(FName(TEXT("Node")), GetClass());

	//Todo: Can hide some properties based on onther settings.
}

void UAnimGraphNode_RootMotionDecoupler::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void UAnimGraphNode_RootMotionDecoupler::GetInputLinkAttributes(FNodeAttributeArray& OutAttributes) const
{
	OutAttributes.Add(UE::Anim::IAnimRootMotionProvider::AttributeName);
}

void UAnimGraphNode_RootMotionDecoupler::GetOutputLinkAttributes(FNodeAttributeArray& OutAttributes) const
{
	OutAttributes.Add(UE::Anim::IAnimRootMotionProvider::AttributeName);
}

void UAnimGraphNode_RootMotionDecoupler::CustomizePinData(UEdGraphPin* Pin, FName SourcePropertyName,
	int32 ArrayIndex) const
{
	Super::CustomizePinData(Pin, SourcePropertyName, ArrayIndex);

	//Todo: Customize Pin Inputs
}

FText UAnimGraphNode_RootMotionDecoupler::GetControllerDescription() const
{
	return LOCTEXT("RootMotionDecoupler", "Root Motion Decoupler");
}


#undef LOCTEXT_NAMESPACE
