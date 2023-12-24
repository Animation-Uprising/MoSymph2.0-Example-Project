//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#include "AnimGraphNode_PoseMatching.h"
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

#define LOCTEXT_NAMESPACE "MoSymphNodes"

UAnimGraphNode_PoseMatching::UAnimGraphNode_PoseMatching(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

FLinearColor UAnimGraphNode_PoseMatching::GetNodeTitleColor() const
{
	return FColor(200, 100, 100);
}

FText UAnimGraphNode_PoseMatching::GetTooltipText() const
{
	UAnimSequenceBase* Sequence = Node.GetSequence();
	if (!Sequence)
	{
		return LOCTEXT("NodeToolTip", "Pose Matching");
	}

	//Additive Not Supported
	return GetTitleGivenAssetInfo(FText::FromString(Sequence->GetPathName()), false);
}

FText UAnimGraphNode_PoseMatching::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	UAnimSequenceBase* Sequence = Node.GetSequence();
	if (!Sequence)
	{
		// we may have a valid variable connected or default pin value
		UEdGraphPin* SequencePin = FindPin(GET_MEMBER_NAME_STRING_CHECKED(FAnimNode_SequencePlayer, GetSequence()));
		if (SequencePin && SequencePin->LinkedTo.Num() > 0)
		{
			return LOCTEXT("PoseMatchNodeTitleVariable", "Pose Matching");
		}
		else if (SequencePin && SequencePin->DefaultObject != nullptr)
		{
			return GetNodeTitleForSequence(TitleType, CastChecked<UAnimSequenceBase>(SequencePin->DefaultObject));
		}
		else
		{
			return LOCTEXT("PoseMatchNullTitle", "Pose Matching (None)");
		}
	}
	else
	{
		return GetNodeTitleForSequence(TitleType, Sequence);
	}
}

EAnimAssetHandlerType UAnimGraphNode_PoseMatching::SupportsAssetClass(const UClass* AssetClass) const
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

FString UAnimGraphNode_PoseMatching::GetNodeCategory() const
{
	return FString("Motion Symphony");
}

void UAnimGraphNode_PoseMatching::GetNodeContextMenuActions(class UToolMenu* Menu, class UGraphNodeContextMenuContext* Context) const
{
	if (!Context->bIsDebugging)
	{
		// add an option to convert to single frame
		//FToolMenuSection& Section = Menu->AddSection("AnimGraphNodePoseMatching", NSLOCTEXT("A3Nodes", "PoseMatchHeading", "Pose Matching"));
		//Section.AddMenuEntry(FGraphEditorCommands::Get().OpenRelatedAsset);
	}
	
}

void UAnimGraphNode_PoseMatching::SetAnimationAsset(UAnimationAsset * Asset)
{
	if (UAnimSequenceBase* Seq = Cast<UAnimSequenceBase>(Asset))
	{
		Node.SetSequence(Seq);
	}
}

void UAnimGraphNode_PoseMatching::OnProcessDuringCompilation(IAnimBlueprintCompilationContext& InCompilationContext, IAnimBlueprintGeneratedClassCompiledData& OutCompiledData)
{
	Node.GetEvaluateGraphExposedInputs();
}

FText UAnimGraphNode_PoseMatching::GetTitleGivenAssetInfo(const FText & AssetName, bool bKnownToBeAdditive)
{
	FFormatNamedArguments Args;
	Args.Add(TEXT("AssetName"), AssetName);

	return FText::Format(LOCTEXT("PoseMatchNodeTitle", "{AssetName} \n Pose Matching"), Args);
}

FText UAnimGraphNode_PoseMatching::GetNodeTitleForSequence(ENodeTitleType::Type TitleType, UAnimSequenceBase * InSequence) const
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
			return FText::Format(LOCTEXT("PoseMatchNodeGroupWithSubtitleFull", "{Title}\nSync group {SyncGroup}"), Args);
		}
		else
		{
			return FText::Format(LOCTEXT("PoseMatchNodeGroupWithSubtitleList", "{Title} (Sync group {SyncGroup})"), Args);
		}
	}
}

FString UAnimGraphNode_PoseMatching::GetControllerDescription() const
{
	return TEXT("Pose Matching Animation Node");
}

//void UAnimGraphNode_PoseMatching::CreateOutputPins()
//{
//	const UAnimationGraphSchema* Schema = GetDefault<UAnimationGraphSchema>();
//	CreatePin(EGPD_Output, Schema->PC_Struct, TEXT(""), FPoseLink::StaticStruct(), /*bIsArray=*/ false, /*bIsReference=*/ false, TEXT("Pose"));
//}



void UAnimGraphNode_PoseMatching::ValidateAnimNodeDuringCompilation(USkeleton * ForSkeleton, FCompilerResultsLog & MessageLog)
{
	Super::ValidateAnimNodeDuringCompilation(ForSkeleton, MessageLog);
}

void UAnimGraphNode_PoseMatching::ValidateAnimNodePostCompile(FCompilerResultsLog& MessageLog,
	UAnimBlueprintGeneratedClass* CompiledClass, int32 CompiledNodeIndex)
{
	Super::ValidateAnimNodePostCompile(MessageLog, CompiledClass, CompiledNodeIndex);
}

void UAnimGraphNode_PoseMatching::PreloadRequiredAssets()
{
	Super::PreloadRequiredAssets();	
	PreloadObject(Node.GetSequence());
}

void UAnimGraphNode_PoseMatching::BakeDataDuringCompilation(FCompilerResultsLog & MessageLog)
{
	UAnimBlueprint* AnimBlueprint = GetAnimBlueprint();
	AnimBlueprint->FindOrAddGroup(Node.GetGroupName());
	
	Node.SetDirtyForPreProcess();
}

void UAnimGraphNode_PoseMatching::GetAllAnimationSequencesReferred(TArray<UAnimationAsset*>& AnimationAssets) const
{
	if (UAnimSequenceBase* Sequence = Node.GetSequence())
	{
		HandleAnimReferenceCollection(Sequence, AnimationAssets);
	}
}

void UAnimGraphNode_PoseMatching::ReplaceReferredAnimations(const TMap<UAnimationAsset*, UAnimationAsset*>& AnimAssetReplacementMap)
{
	UAnimSequenceBase* Sequence = Node.GetSequence();
	HandleAnimReferenceReplacement(Sequence, AnimAssetReplacementMap);
	Node.SetSequence(Sequence);
}

bool UAnimGraphNode_PoseMatching::DoesSupportTimeForTransitionGetter() const
{
	return true;
}

UAnimationAsset * UAnimGraphNode_PoseMatching::GetAnimationAsset() const
{
	UAnimSequenceBase* Sequence = Node.GetSequence();
	UEdGraphPin* SequencePin = FindPin(GET_MEMBER_NAME_STRING_CHECKED(FAnimNode_SequencePlayer, GetSequence()));
	if (SequencePin != nullptr && Sequence == nullptr)
	{
		Sequence = Cast<UAnimSequenceBase>(SequencePin->DefaultObject);
	}

	return Sequence;
}

const TCHAR* UAnimGraphNode_PoseMatching::GetTimePropertyName() const
{
	return TEXT("InternalTimeAccumulator");
}

UScriptStruct* UAnimGraphNode_PoseMatching::GetTimePropertyStruct() const
{
	return FAnimNode_PoseMatching::StaticStruct();
}

void UAnimGraphNode_PoseMatching::CustomizePinData(UEdGraphPin* Pin, FName SourcePropertyName, int32 ArrayIndex) const
{
	Super::CustomizePinData(Pin, SourcePropertyName, ArrayIndex);
	if (Pin->PinName == GET_MEMBER_NAME_STRING_CHECKED(FAnimNode_PoseMatching, GetPlayRate()))
	{
		if (!Pin->bHidden)
		{
			// Draw value for PlayRateBasis if the pin is not exposed
			const float PlayRateBasis = Node.GetPlayRateBasis();
			UEdGraphPin* PlayRateBasisPin = FindPin(GET_MEMBER_NAME_STRING_CHECKED(FAnimNode_PoseMatching, GetPlayRateBasis()));
			if (!PlayRateBasisPin || PlayRateBasisPin->bHidden)
			{
				if (PlayRateBasis != 1.f)
				{
					FFormatNamedArguments Args;
					Args.Add(TEXT("PinFriendlyName"), Pin->PinFriendlyName);
					Args.Add(TEXT("PlayRateBasis"), FText::AsNumber(PlayRateBasis));
					Pin->PinFriendlyName = FText::Format(LOCTEXT("FAnimNode_PoseMatching_PlayRateBasis_Value", "({PinFriendlyName} / {PlayRateBasis})"), Args);
				}
			}
			else // PlayRateBasisPin is visible
				{
				FFormatNamedArguments Args;
				Args.Add(TEXT("PinFriendlyName"), Pin->PinFriendlyName);
				Pin->PinFriendlyName = FText::Format(LOCTEXT("FAnimNode_PoseMatching_PlayRateBasis_Name", "({PinFriendlyName} / PlayRateBasis)"), Args);
				}

			Pin->PinFriendlyName = Node.GetPlayRateScaleBiasClampConstants().GetFriendlyName(Pin->PinFriendlyName);
		}
	}
}

void UAnimGraphNode_PoseMatching::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	const FName PropertyName = (PropertyChangedEvent.Property ? PropertyChangedEvent.Property->GetFName() : NAME_None);

	// Reconstruct node to show updates to PinFriendlyNames.
	if ((PropertyName == GET_MEMBER_NAME_STRING_CHECKED(FAnimNode_PoseMatching, GetPlayRateBasis()))
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

void UAnimGraphNode_PoseMatching::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	UBlueprintNodeSpawner* NodeSpawner = UBlueprintNodeSpawner::Create(GetClass());
	NodeSpawner->DefaultMenuSignature.MenuName = FText::FromString(TEXT("Pose Matching"));
	NodeSpawner->DefaultMenuSignature.Tooltip = FText::FromString(TEXT("Sequence player which uses pose matching to pick it's starting point"));
	ActionRegistrar.AddBlueprintAction(NodeSpawner);
}

#undef LOCTEXT_NAMESPACE
