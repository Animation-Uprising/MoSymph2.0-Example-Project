//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#include "AnimGraphNode_MSMotionMatching.h"
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
#include "Animation/AnimComposite.h"
#include "Animation/MirrorDataTable.h"
#include "Objects/MotionAnimObject.h"

#define LOCTEXT_NAMESPACE "MoSymphNodes"

FLinearColor UAnimGraphNode_MSMotionMatching::GetNodeTitleColor() const
{
	return FLinearColor::Green;
}

FText UAnimGraphNode_MSMotionMatching::GetTooltipText() const
{
	if(!Node.MotionData)
	{
		return LOCTEXT("NodeTooltip", "Motion Matching");
	}

	//Additive not supported for motion matching node
	return GetTitleGivenAssetInfo(FText::FromString(Node.MotionData->GetPathName()), false);
}

FText UAnimGraphNode_MSMotionMatching::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return  LOCTEXT("MotionMatchNullTitle", "Motion Matching");
}

FString UAnimGraphNode_MSMotionMatching::GetNodeCategory() const
{
	return FString("Motion Symphony");
}

void UAnimGraphNode_MSMotionMatching::ValidateAnimNodeDuringCompilation(USkeleton* ForSkeleton,  FCompilerResultsLog& MessageLog)
{
	Super::ValidateAnimNodeDuringCompilation(ForSkeleton, MessageLog);
	
	ValidateAnimNodeDuringCompilationHelper(ForSkeleton, MessageLog, Node.GetAnimAsset(), UMotionDataAsset::StaticClass(),
		FindPin(GET_MEMBER_NAME_STRING_CHECKED(FAnimNode_MSMotionMatching, MotionData)), GET_MEMBER_NAME_CHECKED(FAnimNode_MSMotionMatching, MotionData));
	
	//Check that the Motion Data has been set
	// UMotionDataAsset* MotionDataToCheck = Node.MotionData;
	// UEdGraphPin* MotionDataPin = FindPin(GET_MEMBER_NAME_STRING_CHECKED(FAnimNode_MSMotionMatching, MotionData));
	// if(MotionDataPin && !MotionDataToCheck)
	// {
	// 	MotionDataToCheck = Cast<UMotionDataAsset>(MotionDataPin->DefaultObject);
	// }
	//
	//
	// if(!MotionDataToCheck)
	// {
	// 	bool bHasMotionDataBinding = false;
	// 	if(MotionDataPin)
	// 	{
	// 		if(FAnimGraphNodePropertyBinding* BindingPtr = PropertyBindings.Find(MotionDataPin->GetFName()))
	// 		{
	// 			bHasMotionDataBinding = true;
	// 		}
	// 	}
	// 	
	// 	if(!MotionDataPin || MotionDataPin->LinkedTo.Num() == 0 && !bHasMotionDataBinding)
	// 	{
	// 		MessageLog.Warning(TEXT("@@ references an unknown MotionDataAsset."), this);
	// 		return;
	// 	}
	// }
	// else if(SupportsAssetClass(MotionDataToCheck->GetClass()) == EAnimAssetHandlerType::NotSupported)
	// {
	// 	MessageLog.Warning(*FText::Format(LOCTEXT("UnsupportedAssetError", "@@ is trying to play a {0} as a sequence, which is not allowed."),
	// 		MotionDataToCheck->GetClass()->GetDisplayNameText()).ToString(), this);
	// 	return;
	// }
	// else
	// {
	// 	//Check that the skeleton matches
	// 	USkeleton* MotionDataSkeleton = MotionDataToCheck->GetSkeleton();
	// 	if (!MotionDataSkeleton || !MotionDataSkeleton->IsCompatible(ForSkeleton))
	// 	{
	// 		MessageLog.Warning(TEXT("@@ references motion data that uses incompatible skeleton @@"), this, MotionDataSkeleton);
	// 		return;
	// 	}
	// }
	//
	// bool ValidToCompile = true;
	//
	// //Check that the Motion Data is valid
	// if (!MotionDataToCheck->IsSetupValid())
	// {
	// 	MessageLog.Warning(TEXT("@@ MotionDataAsset setup is not valid."), this);
	// 	ValidToCompile = false;
	// }
	//
	// //Check that all sequences are valid
	// if (!MotionDataToCheck->AreSequencesValid())
	// {
	// 	MessageLog.Warning(TEXT("@@ MotionDataAsset contains sequences that are invalid or null."), this);
	// 	ValidToCompile = false;
	// }
	//
	// //Check that the Motion Data has been pre-processed (if not, ask to process it now)
	// if (ValidToCompile)
	// {
	// 	if (!MotionDataToCheck->bIsProcessed)
	// 	{
	// 		MessageLog.Warning(TEXT("@@ MotionDataAsset has not been pre-processed. Pre-processing during animation graph compilation is not optimised."), this);
	// 		
	// 		if (FMessageDialog::Open(EAppMsgType::YesNo, LOCTEXT("Motion Data has not been pre-processed.",
	// 			"The motion data set for this motion matching node has not been pre-processed. Do you want to pre-process it now (fast / un-optimised)?"))
	// 			!= EAppReturnType::Yes)
	// 		{
	// 			MessageLog.Warning(TEXT("@@ Cannot compile motion matching node with un-processed motion data."), this);
	// 		}
	// 	}
	// }
}

void UAnimGraphNode_MSMotionMatching::PreloadRequiredAssets()
{
	Super::PreloadRequiredAssets();

	PreloadObject(Node.MotionData);
	
	if(Node.MotionData)
	{

		PreloadObject(Node.MotionData->MotionMatchConfig);
		PreloadObject(Node.MotionData->MirrorDataTable);

		for (TObjectPtr<UMotionSequenceObject> MotionAnim : Node.MotionData->SourceMotionSequenceObjects)
		{
			if(MotionAnim)
			{
				PreloadObject(MotionAnim);
				PreloadObject(MotionAnim->Sequence);
			}
		}

		for (TObjectPtr<UMotionCompositeObject> MotionComposite : Node.MotionData->SourceCompositeObjects)
		{
			if(MotionComposite)
			{
				PreloadObject(MotionComposite);
				PreloadObject(MotionComposite->AnimComposite);
			}
		}
		
		for (TObjectPtr<UMotionBlendSpaceObject> MotionBlendSpace : Node.MotionData->SourceBlendSpaceObjects)
		{
			if(MotionBlendSpace)
			{
				PreloadObject(MotionBlendSpace);
				PreloadObject(MotionBlendSpace->BlendSpace);
			}
			
		}
	}
}

void UAnimGraphNode_MSMotionMatching::BakeDataDuringCompilation(class FCompilerResultsLog& MessageLog)
{
	UAnimBlueprint* AnimBlueprint = GetAnimBlueprint();
	AnimBlueprint->FindOrAddGroup(Node.GetGroupName());
	
	//Pre-Process the pose data here
	//if(Node.MotionData && !Node.MotionData->bIsProcessed)
	//{
	//	bool CacheOptimize = Node.MotionData->bOptimize;
	//	Node.MotionData->bOptimize = false;

	//	Node.MotionData->PreProcess(); //Must be the basic type of pre-processing

	//	UE_LOG(LogTemp, Warning, TEXT("Warning: Motion Matching node data was pre-processed during animation graph compilation. The data is not optimised."))

	//	Node.MotionData->bOptimize = CacheOptimize;
	//}
}

bool UAnimGraphNode_MSMotionMatching::DoesSupportTimeForTransitionGetter() const
{
	return false;
}

UAnimationAsset* UAnimGraphNode_MSMotionMatching::GetAnimationAsset() const
{
	return Node.MotionData;
}

const TCHAR* UAnimGraphNode_MSMotionMatching::GetTimePropertyName() const
{
	return TEXT("InternalTimeAccumulator");
}

UScriptStruct* UAnimGraphNode_MSMotionMatching::GetTimePropertyStruct() const
{
	return FAnimNode_MSMotionMatching::StaticStruct();
}

void UAnimGraphNode_MSMotionMatching::GetAllAnimationSequencesReferred(TArray<UAnimationAsset*>& AnimationAssets) const
{
	if (Node.MotionData)
	{
		for (TObjectPtr<UMotionSequenceObject> MotionAnim : Node.MotionData->SourceMotionSequenceObjects)
		{
			if (MotionAnim && MotionAnim->Sequence)
			{
				MotionAnim->Sequence->HandleAnimReferenceCollection(AnimationAssets, true);
			}
		}

		for(TObjectPtr<UMotionBlendSpaceObject> MotionBlendSpace : Node.MotionData->SourceBlendSpaceObjects)
		{
			if(MotionBlendSpace && MotionBlendSpace->BlendSpace)
			{
				MotionBlendSpace->BlendSpace->HandleAnimReferenceCollection(AnimationAssets, true);
			}
		}

		for(TObjectPtr<UMotionCompositeObject> MotionComposite : Node.MotionData->SourceCompositeObjects)
		{
			if(MotionComposite && MotionComposite->AnimComposite)
			{
				MotionComposite->AnimComposite->HandleAnimReferenceCollection(AnimationAssets, true);
			}
		}
	}
}

void UAnimGraphNode_MSMotionMatching::ReplaceReferredAnimations(const TMap<UAnimationAsset*, UAnimationAsset*>& AnimAssetReplacementMap)
{
	HandleAnimReferenceReplacement(Node.MotionData, AnimAssetReplacementMap);
}

void UAnimGraphNode_MSMotionMatching::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	UBlueprintNodeSpawner* NodeSpawner = UBlueprintNodeSpawner::Create(GetClass());
	NodeSpawner->DefaultMenuSignature.MenuName = FText::FromString(TEXT("Motion Matching"));
	NodeSpawner->DefaultMenuSignature.Tooltip = FText::FromString(TEXT("Animation synthesis via motion matching."));
	ActionRegistrar.AddBlueprintAction(NodeSpawner);
}

EAnimAssetHandlerType UAnimGraphNode_MSMotionMatching::SupportsAssetClass(const UClass* AssetClass) const
{
	if (AssetClass->IsChildOf(UMotionDataAsset::StaticClass()))
	{
		return EAnimAssetHandlerType::Supported;
	}
	else
	{
		return EAnimAssetHandlerType::NotSupported;
	}
}

void UAnimGraphNode_MSMotionMatching::GetNodeContextMenuActions(class UToolMenu* Menu, class UGraphNodeContextMenuContext* Context) const
{
	if (!Context->bIsDebugging)
	{
		/*FToolMenuSection& Section = Menu->AddSection("AnimGraphNodeMotionMatching", NSLOCTEXT("MoSymphNodes", "MotionMatchHeading", "Motion Matching"));
		Section.AddMenuEntry(FGraphEditorCommands::Get().OpenRelatedAsset);*/
	}
}

void UAnimGraphNode_MSMotionMatching::SetAnimationAsset(UAnimationAsset* Asset)
{
	if (UMotionDataAsset* MotionDataAsset = Cast<UMotionDataAsset>(Asset))
	{
		Node.MotionData = MotionDataAsset;
	}
}
void UAnimGraphNode_MSMotionMatching::OnProcessDuringCompilation(IAnimBlueprintCompilationContext& InCompilationContext, IAnimBlueprintGeneratedClassCompiledData& OutCompiledData)
{
	Node.GetEvaluateGraphExposedInputs();
}

FText UAnimGraphNode_MSMotionMatching::GetTitleGivenAssetInfo(const FText& AssetName, bool bKnownToBeAdditive)
{
	FFormatNamedArguments Args;
	Args.Add(TEXT("AssetName"), AssetName);

	return FText::Format(LOCTEXT("MotionMatchNodeTitle", "Motion Matching \n {AssetName}"), Args);
}

FString UAnimGraphNode_MSMotionMatching::GetControllerDescription() const
{
	return TEXT("Motion Matching Animation Node");
}

//Node Output Pin(Output is in Component Space, Change at own RISK!)
//void UAnimGraphNode_MotionMatching::CreateOutputPins()
//{
//	const UAnimationGraphSchema* Schema = GetDefault<UAnimationGraphSchema>();
//	CreatePin(EGPD_Output, Schema->PC_Struct, TEXT(""), FPoseLink::StaticStruct(), /*bIsArray=*/ false, /*bIsReference=*/ false, TEXT("Pose"));
//}

#undef LOCTEXT_NAMESPACE