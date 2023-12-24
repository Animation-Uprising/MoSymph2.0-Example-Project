//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#include "AnimGraphNode_MSDistanceMatching.h"
#include "AnimationGraphSchema.h"
#include "EditorCategoryUtils.h"
#include "Animation/AnimComposite.h"
#include "Animation/AnimSequence.h"
#include "Kismet2/CompilerResultsLog.h"
#include "EdGraphSchema_K2_Actions.h"
#include "Modules/ModuleManager.h"
#include "ToolMenus.h"
#include "GraphEditorActions.h"
#include "AssetRegistry/ARFilter.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "BlueprintActionFilter.h"
#include "BlueprintActionDatabaseRegistrar.h"
#include "BlueprintNodeSpawner.h"
#include "Runtime/Launch/Resources/Version.h"
#include "Animation/AnimComposite.h"

#define LOCTEXT_NAMESPACE "MotionSymphonyNodes"

UAnimGraphNode_MSDistanceMatching::UAnimGraphNode_MSDistanceMatching(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

FLinearColor UAnimGraphNode_MSDistanceMatching::GetNodeTitleColor() const
{
	return FColor(200, 100, 100);
}

FText UAnimGraphNode_MSDistanceMatching::GetTooltipText() const
{
	UAnimSequenceBase* NodeSequence = Node.GetSequence();
	if (!NodeSequence)
	{
		return LOCTEXT("NodeToolTip", "Distance Matching");
	}

	//Additive Not Supported
	return GetTitleGivenAssetInfo(FText::FromString(NodeSequence->GetPathName()), false);
}

FText UAnimGraphNode_MSDistanceMatching::GetNodeTitle(ENodeTitleType::Type TitleType) const
{

	UAnimSequenceBase* NodeSequence = Node.GetSequence();
	if (!NodeSequence)
	{
		// we may have a valid variable connected or default pin value
		UEdGraphPin* SequencePin = FindPin(GET_MEMBER_NAME_STRING_CHECKED(FAnimNode_SequencePlayer, GetSequence()));
		if (SequencePin && SequencePin->LinkedTo.Num() > 0)
		{
			return LOCTEXT("DistanceMatchNodeTitleVariable", "Distance Matching");
		}
		else if (SequencePin && SequencePin->DefaultObject != nullptr)
		{
			return GetNodeTitleForSequence(TitleType, CastChecked<UAnimSequenceBase>(SequencePin->DefaultObject));
		}
		else
		{
			return LOCTEXT("DistanceMatchNullTitle", "Distance Matching (None)");
		}
	}
	else
	{
		return GetNodeTitleForSequence(TitleType, NodeSequence);
	}
}

EAnimAssetHandlerType UAnimGraphNode_MSDistanceMatching::SupportsAssetClass(const UClass* AssetClass) const
{
	if (AssetClass->IsChildOf(UAnimSequence::StaticClass()) || AssetClass->IsChildOf(UAnimComposite::StaticClass()))
	{
		return EAnimAssetHandlerType::Supported;
	}
	else
	{
		return EAnimAssetHandlerType::NotSupported;
	}
}

FString UAnimGraphNode_MSDistanceMatching::GetNodeCategory() const
{
	return FString("Motion Symphony (Experimental)");
}

void UAnimGraphNode_MSDistanceMatching::GetNodeContextMenuActions(class UToolMenu* Menu, class UGraphNodeContextMenuContext* Context) const
{
	if (!Context->bIsDebugging)
	{
		// add an option to convert to single frame
		//FToolMenuSection& Section = Menu->AddSection("AnimGraphNodeDistanceMatching", NSLOCTEXT("A3Nodes", "DistanceMatchHeading", "Distance Matching"));
		//Section.AddMenuEntry(FGraphEditorCommands::Get().OpenRelatedAsset);
	}

}

void UAnimGraphNode_MSDistanceMatching::SetAnimationAsset(UAnimationAsset * Asset)
{
	if (UAnimSequenceBase* Seq = Cast<UAnimSequenceBase>(Asset))
	{
		Node.SetSequence(Seq);
	}
}
void UAnimGraphNode_MSDistanceMatching::OnProcessDuringCompilation(IAnimBlueprintCompilationContext& InCompilationContext, IAnimBlueprintGeneratedClassCompiledData& OutCompiledData)
{
	Node.GetEvaluateGraphExposedInputs();
}

FText UAnimGraphNode_MSDistanceMatching::GetTitleGivenAssetInfo(const FText & AssetName, bool bKnownToBeAdditive)
{
	FFormatNamedArguments Args;
	Args.Add(TEXT("AssetName"), AssetName);

	return FText::Format(LOCTEXT("DistanceMatchNodeTitle", "{AssetName} \n Distance Matching"), Args);
}

FText UAnimGraphNode_MSDistanceMatching::GetNodeTitleForSequence(ENodeTitleType::Type TitleType, UAnimSequenceBase * InSequence) const
{
	const FText BasicTitle = GetTitleGivenAssetInfo(FText::FromName(InSequence->GetFName()), false);

	if (SyncGroup_DEPRECATED.GroupName == NAME_None)
	{
		return BasicTitle;
	}
	else
	{
		const FText SyncGroupName = FText::FromName(SyncGroup_DEPRECATED.GroupName);
		FFormatNamedArguments Args;
		Args.Add(TEXT("Title"), BasicTitle);
		Args.Add(TEXT("SyncGroup"), SyncGroupName);

		if (TitleType == ENodeTitleType::FullTitle)
		{
			return FText::Format(LOCTEXT("DistanceMatchNodeGroupWithSubtitleFull", "{Title}\nSync group {SyncGroup}"), Args);
		}
		else
		{
			return FText::Format(LOCTEXT("DistanceMatchNodeGroupWithSubtitleList", "{Title} (Sync group {SyncGroup})"), Args);
		}
	}
}

FString UAnimGraphNode_MSDistanceMatching::GetControllerDescription() const
{
	return TEXT("Distance Matching Animation Node");
}

//void UAnimGraphNode_DistanceMatching::CreateOutputPins()
//{
//	const UAnimationGraphSchema* Schema = GetDefault<UAnimationGraphSchema>();
//	CreatePin(EGPD_Output, Schema->PC_Struct, TEXT(""), FPoseLink::StaticStruct(), /*bIsArray=*/ false, /*bIsReference=*/ false, TEXT("Pose"));
//}



void UAnimGraphNode_MSDistanceMatching::ValidateAnimNodeDuringCompilation(USkeleton * ForSkeleton, FCompilerResultsLog & MessageLog)
{
	Super::ValidateAnimNodeDuringCompilation(ForSkeleton, MessageLog);
}

void UAnimGraphNode_MSDistanceMatching::PreloadRequiredAssets()
{
	PreloadObject(Node.GetSequence());
	Super::PreloadRequiredAssets();
}

void UAnimGraphNode_MSDistanceMatching::BakeDataDuringCompilation(FCompilerResultsLog & MessageLog)
{
	UAnimBlueprint* AnimBlueprint = GetAnimBlueprint();
	Node.SetGroupName(SyncGroup_DEPRECATED.GroupName);
	Node.SetGroupRole(SyncGroup_DEPRECATED.GroupRole);

}

void UAnimGraphNode_MSDistanceMatching::GetAllAnimationSequencesReferred(TArray<UAnimationAsset*>& AnimationAssets) const
{
	if (UAnimSequenceBase* NodeSequence = Node.GetSequence())
	{
		HandleAnimReferenceCollection(NodeSequence, AnimationAssets);
	}
}

void UAnimGraphNode_MSDistanceMatching::ReplaceReferredAnimations(const TMap<UAnimationAsset*, UAnimationAsset*>& AnimAssetReplacementMap)
{
	UAnimSequenceBase* Sequence = Node.GetSequence();
	HandleAnimReferenceReplacement(Sequence, AnimAssetReplacementMap);
	Node.SetSequence(Sequence);
}

bool UAnimGraphNode_MSDistanceMatching::DoesSupportTimeForTransitionGetter() const
{
	return true;
}

UAnimationAsset * UAnimGraphNode_MSDistanceMatching::GetAnimationAsset() const
{
	UAnimSequenceBase* Sequence = Node.GetSequence();
	UEdGraphPin* SequencePin = FindPin(GET_MEMBER_NAME_STRING_CHECKED(FAnimNode_SequencePlayer, GetSequence()));
	
	if (SequencePin != nullptr && Sequence == nullptr)
	{
		Sequence = Cast<UAnimSequenceBase>(SequencePin->DefaultObject);
	}

	return Sequence;
}

const TCHAR* UAnimGraphNode_MSDistanceMatching::GetTimePropertyName() const
{
	return TEXT("InternalTimeAccumulator");
}

UScriptStruct* UAnimGraphNode_MSDistanceMatching::GetTimePropertyStruct() const
{
	return FAnimNode_MSDistanceMatching::StaticStruct();
}

void UAnimGraphNode_MSDistanceMatching::CustomizePinData(UEdGraphPin* Pin, FName SourcePropertyName, int32 ArrayIndex) const
{
	Super::CustomizePinData(Pin, SourcePropertyName, ArrayIndex);

	if (Pin->PinName == GET_MEMBER_NAME_STRING_CHECKED(FAnimNode_MSDistanceMatching, GetPlayRate()))
	{
		if (!Pin->bHidden)
		{
			// Draw value for PlayRateBasis if the pin is not exposed
			UEdGraphPin* PlayRateBasisPin = FindPin(GET_MEMBER_NAME_STRING_CHECKED(FAnimNode_MSDistanceMatching, GetPlayRateBasis()));
			if (!PlayRateBasisPin || PlayRateBasisPin->bHidden)
			{
				if (Node.GetPlayRateBasis() != 1.f)
				{
					FFormatNamedArguments Args;
					Args.Add(TEXT("PinFriendlyName"), Pin->PinFriendlyName);
					Args.Add(TEXT("PlayRateBasis"), FText::AsNumber(Node.GetPlayRateBasis()));
					Pin->PinFriendlyName = FText::Format(LOCTEXT("FAnimNode_DistanceMatching_PlayRateBasis_Value", "({PinFriendlyName} / {PlayRateBasis})"), Args);
				}
			}
			else // PlayRateBasisPin is visible
				{
				FFormatNamedArguments Args;
				Args.Add(TEXT("PinFriendlyName"), Pin->PinFriendlyName);
				Pin->PinFriendlyName = FText::Format(LOCTEXT("FAnimNode_DistanceMatching_PlayRateBasis_Name", "({PinFriendlyName} / PlayRateBasis)"), Args);
				}

			Pin->PinFriendlyName = Node.GetPlayRateScaleBiasClampConstants().GetFriendlyName(Pin->PinFriendlyName);
		}
	}
}

void UAnimGraphNode_MSDistanceMatching::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	const FName PropertyName = (PropertyChangedEvent.Property ? PropertyChangedEvent.Property->GetFName() : NAME_None);

	if ((PropertyName == GET_MEMBER_NAME_STRING_CHECKED(FAnimNode_MSDistanceMatching, GetPlayRateBasis()))
			|| (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(FInputScaleBiasClamp, bMapRange))
			|| (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(FInputRange, Min))
			|| (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(FInputRange, Max))
			|| (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(FInputScaleBiasClamp, Scale))
			|| (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(FInputScaleBiasClamp, Bias))
			|| (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(FInputScaleBiasClamp, bClampResult))
			|| (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(FInputScaleBiasClamp, ClampMin))
			|| (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(FInputScaleBiasClamp, ClampMax))
			|| (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(FInputScaleBiasClamp, bInterpResult))
			|| (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(FInputScaleBiasClamp, InterpSpeedIncreasing))
			|| (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(FInputScaleBiasClamp, InterpSpeedDecreasing)))
	{
		ReconstructNode();
	}
	
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void UAnimGraphNode_MSDistanceMatching::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	UBlueprintNodeSpawner* NodeSpawner = UBlueprintNodeSpawner::Create(GetClass());
	NodeSpawner->DefaultMenuSignature.MenuName = FText::FromString(TEXT("Distance Matching"));
	NodeSpawner->DefaultMenuSignature.Tooltip = FText::FromString(TEXT("Sequence player which uses distance matching to pick it's starting point"));
	ActionRegistrar.AddBlueprintAction(NodeSpawner);
}


#undef LOCTEXT_NAMESPACE
