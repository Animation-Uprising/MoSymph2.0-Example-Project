//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#include "MotionPreProcessToolkit.h"
#include "Framework/Docking/TabManager.h"
#include "Framework/Commands/UICommandList.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/MultiBox/MultiBoxExtender.h"
#include "Framework/Commands/UIAction.h"
#include "EditorStyleSet.h"
#include "Widgets/Docking/SDockTab.h"
#include "IDetailsView.h"
#include "Controls/MotionSequenceTimelineCommands.h"
#include "MotionPreProcessorToolkitCommands.h"
#include "GUI/Widgets/SAnimList.h"
#include "GUI/Widgets/SMotionBrowser.h"
#include "GUI/Dialogs/AddNewAnimDialog.h"
#include "Misc/MessageDialog.h"
#include "AnimPreviewInstance.h"
#include "AssetSelection.h"
#include "SScrubControlPanel.h"
#include "../GUI/Widgets/SMotionTimeline.h"
#include "MotionSymphonyEditor.h"
#include "TagPoint.h"
#include "TagSection.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Objects/MotionAnimObject.h"

#define LOCTEXT_NAMESPACE "MotionPreProcessEditor"

const FName MotionPreProcessorAppName = FName(TEXT("MotionPreProcessorEditorApp"));

TArray<TObjectPtr<UMotionSequenceObject>> FMotionPreProcessToolkit::CopiedSequences;
TArray<TObjectPtr<UMotionCompositeObject>> FMotionPreProcessToolkit::CopiedComposites;
TArray<TObjectPtr<UMotionBlendSpaceObject>> FMotionPreProcessToolkit::CopiedBlendSpaces;

struct FMotionPreProcessorEditorTabs
{
	static const FName DetailsID;
	static const FName ViewportID;
	static const FName AnimationsID;
	static const FName AnimationDetailsID;
	static const FName MotionBrowserID;
};

const FName FMotionPreProcessorEditorTabs::DetailsID(TEXT("Details"));
const FName FMotionPreProcessorEditorTabs::ViewportID(TEXT("Viewport"));
const FName FMotionPreProcessorEditorTabs::AnimationsID(TEXT("Animations"));
const FName FMotionPreProcessorEditorTabs::AnimationDetailsID(TEXT("Animation Details"));
const FName FMotionPreProcessorEditorTabs::MotionBrowserID(TEXT("Motion Browser"));

FMotionPreProcessToolkit::~FMotionPreProcessToolkit()
{
	DetailsView.Reset();
	AnimDetailsView.Reset();
}

#if ENGINE_MINOR_VERSION > 2
FString FMotionPreProcessToolkit::GetReferencerName() const
{
	return TEXT("FMotionPreProcessToolkit");
}
#endif

void FMotionPreProcessToolkit::Initialize(class UMotionDataAsset* InPreProcessAsset, const EToolkitMode::Type InMode, const TSharedPtr<IToolkitHost> InToolkitHost)
{
	ActiveMotionDataAsset = InPreProcessAsset;

	UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();

	if(AssetEditorSubsystem)
	{
		AssetEditorSubsystem->CloseOtherEditors(InPreProcessAsset, this);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("MotionPreProcessToolkit: Failed to find AssetEditorSubsystem."))
	}
	
	CurrentAnimIndex = INDEX_NONE;
	CurrentAnimType = EMotionAnimAssetType::None;
	PreviewPoseStartIndex = INDEX_NONE;
	PreviewPoseEndIndex = INDEX_NONE;
	PreviewPoseCurrentIndex = INDEX_NONE;

	//Create the details panel
	const bool bIsUpdateable = false;
	const bool bIsLockable = false;

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FDetailsViewArgs DetailsViewArgs;

	DetailsViewArgs.bUpdatesFromSelection = bIsUpdateable;
	DetailsViewArgs.bLockable = bIsLockable;
	DetailsViewArgs.bAllowSearch = true;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::ObjectsUseNameArea;
	DetailsViewArgs.bHideSelectionTip = false;
	DetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);
	AnimDetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);

	//Setup toolkit commands
	FMotionSequenceTimelineCommands::Register();
	FMotionPreProcessToolkitCommands::Register();
	BindCommands();

	//Register UndoRedo
	ActiveMotionDataAsset->SetFlags(RF_Transactional);
	GEditor->RegisterForUndo(this);

	//Setup Layout for editor toolkit
	TSharedPtr<FMotionPreProcessToolkit> MotionPreProcessToolkitPtr = SharedThis(this);
	ViewportPtr = SNew(SMotionPreProcessToolkitViewport, MotionPreProcessToolkitPtr)
		.MotionDataBeingEdited(this, &FMotionPreProcessToolkit::GetActiveMotionDataAsset);

	const TSharedRef<FTabManager::FLayout> Layout = FTabManager::NewLayout("Standalone_MotionPreProcessorAssetEditor")
	->AddArea
	(
		FTabManager::NewPrimaryArea()
			->SetOrientation(Orient_Horizontal)
			->Split
			(
				FTabManager::NewSplitter()
					->SetOrientation(Orient_Vertical)
					->SetSizeCoefficient(1.0f)
					->Split
					(
						FTabManager::NewSplitter()
							->SetOrientation(Orient_Horizontal)
							->SetSizeCoefficient(0.2f)
							->Split
							(
								FTabManager::NewSplitter()
								->SetOrientation(Orient_Vertical)
								->SetSizeCoefficient(1.0f)
								->Split
								(
									FTabManager::NewStack()
									->AddTab(FMotionPreProcessorEditorTabs::AnimationsID, ETabState::OpenedTab)
									->SetHideTabWell(true)
									->SetSizeCoefficient(0.6f)
								)
								->Split
								(
									FTabManager::NewStack()
									->AddTab(FMotionPreProcessorEditorTabs::AnimationDetailsID, ETabState::OpenedTab)
									->AddTab(FMotionPreProcessorEditorTabs::MotionBrowserID, ETabState::ClosedTab) //Todo: Figure out why this makes the Toolkit host invalid if it's opened
									->SetHideTabWell(false)
									->SetSizeCoefficient(0.4f)
								)
							)
							->Split
							(
								FTabManager::NewStack()
								->AddTab(FMotionPreProcessorEditorTabs::ViewportID, ETabState::OpenedTab)
								->SetHideTabWell(true)
								->SetSizeCoefficient(0.6f)
							)
							->Split
							(
								FTabManager::NewStack()
								->AddTab(FMotionPreProcessorEditorTabs::DetailsID, ETabState::OpenedTab)
								->SetHideTabWell(true)
								->SetSizeCoefficient(0.2f)
							)
					)
			)
	);


	FAssetEditorToolkit::InitAssetEditor(
		InMode,
		InToolkitHost,
		MotionPreProcessorAppName,
		Layout,
		true,
		true,
		InPreProcessAsset
	);

	if (DetailsView.IsValid())
	{
		DetailsView->SetObject(ActiveMotionDataAsset);
	}

	ExtendMenu();
	ExtendToolbar();
	RegenerateMenusAndToolbars();
}

FString FMotionPreProcessToolkit::GetDocumentationLink() const
{
	return FString(TEXT("https://www.wikiful.com/@AnimationUprising/motion-symphony/motion-matching/motion-data-asset"));
}

void FMotionPreProcessToolkit::RegisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	WorkspaceMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(LOCTEXT("WorkspaceMenu_MotionPreProcessorAsset", "MotionPreProcessEditor"));
	const auto WorkspaceMenuCategoryRef = WorkspaceMenuCategory.ToSharedRef();

	FAssetEditorToolkit::RegisterTabSpawners(InTabManager);

	InTabManager->RegisterTabSpawner(FMotionPreProcessorEditorTabs::ViewportID, FOnSpawnTab::CreateSP(this, &FMotionPreProcessToolkit::SpawnTab_Viewport))
		.SetDisplayName(LOCTEXT("ViewportTab", "Viewport"))
		.SetGroup(WorkspaceMenuCategoryRef)
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Viewports"));

	InTabManager->RegisterTabSpawner(FMotionPreProcessorEditorTabs::AnimationsID, FOnSpawnTab::CreateSP(this, &FMotionPreProcessToolkit::SpawnTab_Animations))
		.SetDisplayName(LOCTEXT("AnimationsTab", "Animations"))
		.SetGroup(WorkspaceMenuCategoryRef)
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.ContentBrowser"));

	InTabManager->RegisterTabSpawner(FMotionPreProcessorEditorTabs::AnimationDetailsID, FOnSpawnTab::CreateSP(this, &FMotionPreProcessToolkit::SpawnTab_AnimationDetails))
		.SetDisplayName(LOCTEXT("AnimDetailsTab", "Animation Details"))
		.SetGroup(WorkspaceMenuCategoryRef)
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.ContentBrowser"));

	InTabManager->RegisterTabSpawner(FMotionPreProcessorEditorTabs::MotionBrowserID, FOnSpawnTab::CreateSP(this, &FMotionPreProcessToolkit::SpawnTab_MotionBrowser))
		.SetDisplayName(LOCTEXT("MotionBrowserTab", "Motion Browser"))
		.SetGroup(WorkspaceMenuCategoryRef)
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.ContentBrowser"));

	InTabManager->RegisterTabSpawner(FMotionPreProcessorEditorTabs::DetailsID, FOnSpawnTab::CreateSP(this, &FMotionPreProcessToolkit::SpawnTab_Details))
		.SetDisplayName(LOCTEXT("DetailsTab", "Details"))
		.SetGroup(WorkspaceMenuCategoryRef)
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Details"));

}

void FMotionPreProcessToolkit::UnregisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);

	InTabManager->UnregisterTabSpawner(FMotionPreProcessorEditorTabs::ViewportID);
	InTabManager->UnregisterTabSpawner(FMotionPreProcessorEditorTabs::DetailsID);
	InTabManager->UnregisterTabSpawner(FMotionPreProcessorEditorTabs::AnimationsID);
	InTabManager->UnregisterTabSpawner(FMotionPreProcessorEditorTabs::AnimationDetailsID);
	InTabManager->UnregisterTabSpawner(FMotionPreProcessorEditorTabs::MotionBrowserID);
}

FText FMotionPreProcessToolkit::GetBaseToolkitName() const
{
	return LOCTEXT("MotionPreProcessorToolkitAppLabel", "MotionPreProcessor Toolkit");
}

FName FMotionPreProcessToolkit::GetToolkitFName() const
{
	return FName("MotionPreProcessorToolkit");
}

FText FMotionPreProcessToolkit::GetToolkitName() const
{
	const bool bDirtyState = ActiveMotionDataAsset->GetOutermost()->IsDirty();

	FFormatNamedArguments Args;
	Args.Add(TEXT("MotionPreProcessName"), FText::FromString(ActiveMotionDataAsset->GetName()));
	Args.Add(TEXT("DirtyState"), bDirtyState ? FText::FromString(TEXT("*")) : FText::GetEmpty());
	return FText::Format(LOCTEXT("MotionpreProcessorToolkitName", "{MotionPreProcessName}{DirtyState}"), Args);
}

FText FMotionPreProcessToolkit::GetToolkitToolTipText() const
{
	return LOCTEXT("Tooltip", "Motion PreProcessor Editor");
}

FLinearColor FMotionPreProcessToolkit::GetWorldCentricTabColorScale() const
{
	return FLinearColor();
}

FString FMotionPreProcessToolkit::GetWorldCentricTabPrefix() const
{
	return FString();
}

void FMotionPreProcessToolkit::AddReferencedObjects(FReferenceCollector & Collector)
{
}

void FMotionPreProcessToolkit::PostUndo(bool bSuccess)
{
}

void FMotionPreProcessToolkit::PostRedo(bool bSuccess)
{
}

TSharedRef<SDockTab> FMotionPreProcessToolkit::SpawnTab_Viewport(const FSpawnTabArgs & Args)
{
	ViewInputMin = 0.0f;
	ViewInputMax = GetTotalSequenceLength();
	LastObservedSequenceLength = ViewInputMax;

	const TSharedPtr<FMotionPreProcessToolkit> MotionPreProcessToolkitPtr = SharedThis(this);

	TSharedRef<FUICommandList> LocalToolkitCommands = GetToolkitCommands();
	MotionTimelinePtr = SNew(SMotionTimeline, LocalToolkitCommands, TWeakPtr<FMotionPreProcessToolkit>(MotionPreProcessToolkitPtr));

	return SNew(SDockTab)
		.Label(LOCTEXT("ViewportTab_Title", "Viewport"))
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
				[
					ViewportPtr.ToSharedRef()
				]
			+SVerticalBox::Slot()
				.Padding(0, 8, 0, 0)
				.AutoHeight()
				.HAlign(HAlign_Fill)
				[
					MotionTimelinePtr.ToSharedRef()
				]
		];
}

TSharedRef<SDockTab> FMotionPreProcessToolkit::SpawnTab_Details(const FSpawnTabArgs & Args)
{
	return SNew(SDockTab)
		.Label(LOCTEXT("DetailsTab_Title", "Details"))
		.TabColorScale(GetTabColorScale())
		[
			DetailsView.ToSharedRef()
		];
}

TSharedRef<SDockTab> FMotionPreProcessToolkit::SpawnTab_Animations(const FSpawnTabArgs& Args)
{
	TSharedPtr<FMotionPreProcessToolkit> MotionPreProcessToolkitPtr = SharedThis(this);
	SAssignNew(AnimationTreePtr, SAnimTree, MotionPreProcessToolkitPtr);

	return SNew(SDockTab)
		.Label(LOCTEXT("AnimationsTab_Title", "Animations"))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.FillHeight(0.5f)
			[
				SNew(SBorder)
				[
					AnimationTreePtr.ToSharedRef()
				]
			]
		];
}

TSharedRef<SDockTab> FMotionPreProcessToolkit::SpawnTab_AnimationDetails(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.Label(LOCTEXT("DetailsTab_Title", "Details"))
		.TabColorScale(GetTabColorScale())
		[
			AnimDetailsView.ToSharedRef()
		];
}

TSharedRef<SDockTab> FMotionPreProcessToolkit::SpawnTab_MotionBrowser(const FSpawnTabArgs& Args)
{
	TSharedPtr<FMotionPreProcessToolkit> MotionPreProcessToolkitPtr = SharedThis(this);
	SAssignNew(MotionBrowserPtr, SMotionBrowser, MotionPreProcessToolkitPtr);
	
	return SNew(SDockTab)
		.Label(LOCTEXT("MotionBrowser_Title", "Motion Browser"))
		.TabColorScale(GetTabColorScale())
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			[
				SNew(SBorder)
				[
					MotionBrowserPtr.ToSharedRef()
				]
			]
		];
}

void FMotionPreProcessToolkit::BindCommands()
{
	const FMotionPreProcessToolkitCommands& Commands = FMotionPreProcessToolkitCommands::Get();

	const TSharedRef<FUICommandList>& UICommandList = GetToolkitCommands();

	UICommandList->MapAction(Commands.PickAnims,
		FExecuteAction::CreateSP(this, &FMotionPreProcessToolkit::OpenPickAnimsDialog));
	UICommandList->MapAction(Commands.ClearAnims,
		FExecuteAction::CreateSP(this, &FMotionPreProcessToolkit::ClearAnimList));
	UICommandList->MapAction(Commands.LastAnim,
		FExecuteAction::CreateSP(this, &FMotionPreProcessToolkit::SelectPreviousAnim));
	UICommandList->MapAction(Commands.NextAnim,
		FExecuteAction::CreateSP(this, &FMotionPreProcessToolkit::SelectNextAnim));
	UICommandList->MapAction(Commands.PreProcess,
		FExecuteAction::CreateSP(this, &FMotionPreProcessToolkit::PreProcessAnimData));
}

void FMotionPreProcessToolkit::ExtendMenu()
{
}

void FMotionPreProcessToolkit::ExtendToolbar()
{
	struct local
	{
		static void FillToolbar(FToolBarBuilder& ToolbarBuilder)
		{
			ToolbarBuilder.BeginSection("Animations");
			{
				ToolbarBuilder.AddToolBarButton(FMotionPreProcessToolkitCommands::Get().PickAnims, NAME_None,
					TAttribute<FText>(), TAttribute<FText>(),
					FSlateIcon(FMotionSymphonyStyle::GetStyleSetName(), "MotionSymphony.Toolbar.AddAnims"));

				ToolbarBuilder.AddToolBarButton(FMotionPreProcessToolkitCommands::Get().ClearAnims, NAME_None,
					TAttribute<FText>(), TAttribute<FText>(),
					FSlateIcon(FMotionSymphonyStyle::GetStyleSetName(), "MotionSymphony.Toolbar.ClearAnims"));
			}
			ToolbarBuilder.EndSection();

			ToolbarBuilder.BeginSection("Navigation");
			{

				ToolbarBuilder.AddToolBarButton(FMotionPreProcessToolkitCommands::Get().LastAnim, NAME_None, 
					TAttribute<FText>(), TAttribute<FText>(),
					FSlateIcon(FMotionSymphonyStyle::GetStyleSetName(), "MotionSymphony.Toolbar.LastAnim"));

				ToolbarBuilder.AddToolBarButton(FMotionPreProcessToolkitCommands::Get().NextAnim, NAME_None,
					TAttribute<FText>(), TAttribute<FText>(),
					FSlateIcon(FMotionSymphonyStyle::GetStyleSetName(), "MotionSymphony.Toolbar.NextAnim"));
			}
			ToolbarBuilder.EndSection();

			ToolbarBuilder.BeginSection("Processing");
			{
				ToolbarBuilder.AddToolBarButton(FMotionPreProcessToolkitCommands::Get().PreProcess, NAME_None,
					TAttribute<FText>(), TAttribute<FText>(),
					FSlateIcon(FMotionSymphonyStyle::GetStyleSetName(), "MotionSymphony.Toolbar.PreProcess"));
			}
			ToolbarBuilder.EndSection();
		}
	};

	TSharedPtr<FExtender> ToolbarExtender = MakeShareable(new FExtender);

	ToolbarExtender->AddToolBarExtension
	(
		"Asset",
		EExtensionHook::After,
		GetToolkitCommands(),
		FToolBarExtensionDelegate::CreateStatic(&local::FillToolbar)
	);

	AddToolbarExtender(ToolbarExtender);

	//FMotionSymphonyEditorModule* MotionSymphonyEditorModule = &FModuleManager::LoadModuleChecked<FMotionSymphonyEditorModule>("MotionSymphonyEditor");
	//AddToolbarExtender(MotionSymphonyEditorModule->GetPreProcessToolkitToolbarExtensibilityManager()->GetAllExtenders());
}

FReply FMotionPreProcessToolkit::OnClick_Forward()
{
	UDebugSkelMeshComponent* PreviewSkeletonMeshComponent = GetPreviewSkeletonMeshComponent();
	UAnimPreviewInstance* PreviewInstance = PreviewSkeletonMeshComponent->PreviewInstance;

	if (!PreviewSkeletonMeshComponent || !PreviewInstance)
	{
		return FReply::Handled();
	}

	const bool bIsReverse = PreviewInstance->IsReverse();

	if (const bool bIsPlaying = PreviewInstance->IsPlaying())
	{
		if (bIsReverse)
		{
			PreviewInstance->SetReverse(false);
		}
		else
		{
			PreviewInstance->StopAnim();
		}
	}
	else
	{
		PreviewInstance->SetPlaying(true);
	}

	return FReply::Handled();
}

FReply FMotionPreProcessToolkit::OnClick_Forward_Step()
{
	UDebugSkelMeshComponent* PreviewSkeletonMeshComponent = GetPreviewSkeletonMeshComponent();

	if (!PreviewSkeletonMeshComponent)
	{
		return FReply::Handled();
	}
		
	PreviewSkeletonMeshComponent->PreviewInstance->StopAnim();
	SetCurrentFrame(GetCurrentFrame() + 1);

	return FReply::Handled();
}

FReply FMotionPreProcessToolkit::OnClick_Forward_End()
{
	UDebugSkelMeshComponent* PreviewSkeletonMeshComponent = GetPreviewSkeletonMeshComponent();

	if (!PreviewSkeletonMeshComponent)
	{
		return FReply::Handled();
	}

	PreviewSkeletonMeshComponent->PreviewInstance->StopAnim();
	PreviewSkeletonMeshComponent->PreviewInstance->SetPosition(GetTotalSequenceLength(), false);

	return FReply::Handled();
}

FReply FMotionPreProcessToolkit::OnClick_Backward()
{
	UDebugSkelMeshComponent* PreviewSkeletonMeshComponent = GetPreviewSkeletonMeshComponent();
	UAnimPreviewInstance* PreviewInstance = PreviewSkeletonMeshComponent->PreviewInstance;

	if (!PreviewSkeletonMeshComponent || !PreviewInstance)
	{
		return FReply::Handled();
	}

	const bool bIsReverse = PreviewInstance->IsReverse();
	const bool bIsPlaying = PreviewInstance->IsPlaying();

	if (bIsPlaying)
	{
		if (bIsReverse)
		{
			PreviewInstance->StopAnim();
		}
		else
		{
			PreviewInstance->SetReverse(true);
		}
	}
	else
	{
		if (!bIsReverse)
		{
			PreviewInstance->SetReverse(true);
		}

		PreviewInstance->SetPlaying(true);
	}

	return FReply::Handled();
}

FReply FMotionPreProcessToolkit::OnClick_Backward_Step()
{
	UDebugSkelMeshComponent* PreviewSkeletonMeshComponent = GetPreviewSkeletonMeshComponent();

	if (!PreviewSkeletonMeshComponent)
	{
		return FReply::Handled();
	}

	PreviewSkeletonMeshComponent->PreviewInstance->StopAnim();
	SetCurrentFrame(GetCurrentFrame() - 1);

	return FReply::Handled();
}

FReply FMotionPreProcessToolkit::OnClick_Backward_End()
{
	UDebugSkelMeshComponent* PreviewSkeletonMeshComponent = GetPreviewSkeletonMeshComponent();

	if (!PreviewSkeletonMeshComponent)
	{
		return FReply::Handled();
	}

	PreviewSkeletonMeshComponent->PreviewInstance->StopAnim();
	PreviewSkeletonMeshComponent->PreviewInstance->SetPosition(0.0f, false);

	return FReply::Handled();
}

FReply FMotionPreProcessToolkit::OnClick_ToggleLoop()
{
	UDebugSkelMeshComponent* PreviewSkeletonMeshComponent = GetPreviewSkeletonMeshComponent();

	if (!PreviewSkeletonMeshComponent)
	{
		return FReply::Handled();
	}

	PreviewSkeletonMeshComponent->PreviewInstance->SetLooping(
		PreviewSkeletonMeshComponent->PreviewInstance->IsLooping());

	return FReply::Handled();
}

uint32 FMotionPreProcessToolkit::GetTotalFrameCount() const
{
	UAnimSequence* CurrentAnim = GetCurrentAnimation();
	
	if (CurrentAnim)
	{
		return CurrentAnim->GetNumberOfSampledKeys();
	}

	return 0;
}

uint32 FMotionPreProcessToolkit::GetTotalFrameCountPlusOne() const
{
	return GetTotalFrameCount() + 1;
}

float FMotionPreProcessToolkit::GetTotalSequenceLength() const
{
	UAnimSequence* CurrentAnim = GetCurrentAnimation();

	if (!CurrentAnim)
	{
		return 0.0f;
	}

	return CurrentAnim->GetPlayLength();
}

float FMotionPreProcessToolkit::GetPlaybackPosition() const
{
	UDebugSkelMeshComponent* previewSkeletonMeshComponent = GetPreviewSkeletonMeshComponent();

	if (previewSkeletonMeshComponent)
	{
		return previewSkeletonMeshComponent->PreviewInstance->GetCurrentTime();
	}

	return 0.0f;
}

void FMotionPreProcessToolkit::SetPlaybackPosition(float NewTime)
{
	UDebugSkelMeshComponent*  PreviewSkeletonMeshComponent = GetPreviewSkeletonMeshComponent();

	FindCurrentPose(NewTime);
	
	if (PreviewSkeletonMeshComponent)
	{
		NewTime = FMath::Clamp<float>(NewTime, 0.0f, GetTotalSequenceLength());
		PreviewSkeletonMeshComponent->PreviewInstance->SetPosition(NewTime, false);
	}
}

void FMotionPreProcessToolkit::FindCurrentPose(float Time)
{
	//Set the current preview pose if preprocessed
	if (ActiveMotionDataAsset->bIsProcessed
		&& PreviewPoseCurrentIndex != INDEX_NONE
		&& PreviewPoseEndIndex != INDEX_NONE)
	{
		TObjectPtr<UMotionAnimObject> AnimAsset = GetCurrentMotionAnim();

		
		PreviewPoseCurrentIndex = PreviewPoseStartIndex + FMath::RoundToInt(Time / (ActiveMotionDataAsset->PoseInterval * AnimAsset->PlayRate));
		PreviewPoseCurrentIndex = FMath::Clamp(PreviewPoseCurrentIndex, PreviewPoseStartIndex, PreviewPoseEndIndex);
	}
	else
	{
		PreviewPoseCurrentIndex = INDEX_NONE;
	}
}

bool FMotionPreProcessToolkit::IsLooping() const
{
	UDebugSkelMeshComponent* previewSkeletonMeshComponent = GetPreviewSkeletonMeshComponent();

	if (previewSkeletonMeshComponent)
	{
		return previewSkeletonMeshComponent->PreviewInstance->IsLooping();
	}

	return false;
}

EPlaybackMode::Type FMotionPreProcessToolkit::GetPlaybackMode() const
{
	UDebugSkelMeshComponent* previewSkeletonMeshComponent = GetPreviewSkeletonMeshComponent();

	if (previewSkeletonMeshComponent)
	{
		if (previewSkeletonMeshComponent->PreviewInstance->IsPlaying())
		{
			return previewSkeletonMeshComponent->PreviewInstance->IsReverse() ? 
				EPlaybackMode::PlayingReverse : EPlaybackMode::PlayingForward;
		}
	}

	return EPlaybackMode::Stopped;
}

float FMotionPreProcessToolkit::GetViewRangeMin() const
{
	return ViewInputMin;
}

float FMotionPreProcessToolkit::GetViewRangeMax() const
{
	const float SequenceLength = GetTotalSequenceLength();
	if (SequenceLength != LastObservedSequenceLength)
	{
		LastObservedSequenceLength = SequenceLength;
		ViewInputMin = 0.0f;
		ViewInputMax = SequenceLength;
	}

	return ViewInputMax;
}

void FMotionPreProcessToolkit::SetViewRange(float NewMin, float NewMax)
{
	ViewInputMin = FMath::Max<float>(NewMin, 0.0f);
	ViewInputMax = FMath::Min<float>(NewMax, GetTotalSequenceLength());
}

UMotionDataAsset* FMotionPreProcessToolkit::GetActiveMotionDataAsset() const
{
	return ActiveMotionDataAsset;
}

FText FMotionPreProcessToolkit::GetAnimationName(const int32 AnimIndex)
{
	if(ActiveMotionDataAsset->GetSourceAnimCount() > 0)
	{
		TObjectPtr<UMotionSequenceObject> MotionAnim = ActiveMotionDataAsset->GetEditableSourceSequenceAtIndex(AnimIndex);

		if (MotionAnim && MotionAnim->Sequence)
		{
			return FText::AsCultureInvariant(MotionAnim->Sequence->GetName());
		}
	}

	return LOCTEXT("NullAnimation", "Null Animation");
}

FText FMotionPreProcessToolkit::GetBlendSpaceName(const int32 BlendSpaceIndex)
{
	if(ActiveMotionDataAsset->GetSourceBlendSpaceCount() > 0)
	{
		TObjectPtr<UMotionBlendSpaceObject> MotionBlendSpace = ActiveMotionDataAsset->GetEditableSourceBlendSpaceAtIndex(BlendSpaceIndex);

		if (MotionBlendSpace && MotionBlendSpace->BlendSpace)
		{
			return FText::AsCultureInvariant(MotionBlendSpace->BlendSpace->GetName());
		}
	}

	return LOCTEXT("NullAnimation", "Null Animation");
}

FText FMotionPreProcessToolkit::GetCompositeName(const int32 CompositeIndex)
{
	if (ActiveMotionDataAsset->GetSourceCompositeCount() > 0)
	{
		const TObjectPtr<UMotionCompositeObject> MotionComposite = ActiveMotionDataAsset->GetEditableSourceCompositeAtIndex(CompositeIndex);

		if (MotionComposite && MotionComposite->AnimComposite)
		{
			return FText::AsCultureInvariant(MotionComposite->AnimComposite->GetName());
		}
	}

	return LOCTEXT("NullAnimation", "Null Animation");
}

void FMotionPreProcessToolkit::SetCurrentAnimation(const int32 AnimIndex, const EMotionAnimAssetType AnimType, const bool bForceRefresh)
{
	if(!ActiveMotionDataAsset->IsSearchPoseMatrixGenerated())
	{
		ActiveMotionDataAsset->GenerateSearchPoseMatrix();
	}
	
	if (IsValidAnim(AnimIndex, AnimType)
		&& ActiveMotionDataAsset->SetAnimPreviewIndex(AnimType, AnimIndex))
	{
		if (AnimIndex != CurrentAnimIndex
			|| CurrentAnimType != AnimType
			|| bForceRefresh)
		{
			CurrentAnimIndex = AnimIndex;
			CurrentAnimType = AnimType;
			const UMotionAnimObject* MotionAnim = ActiveMotionDataAsset->GetEditableSourceAnim(AnimIndex, AnimType);
			const bool bMirrored = ViewportPtr->IsMirror() && (MotionAnim ? MotionAnim->bEnableMirroring : false);

			if (ActiveMotionDataAsset->bIsProcessed)
			{
				const int32 PoseCount = ActiveMotionDataAsset->Poses.Num();
				for (int32 i = 0; i < PoseCount; ++i)
				{
					const FPoseMotionData& StartPose = ActiveMotionDataAsset->Poses[i];

					if (StartPose.AnimId == CurrentAnimIndex
						&& StartPose.AnimType == CurrentAnimType
						&& StartPose.bMirrored == bMirrored)
					{
						PreviewPoseStartIndex = i;
						PreviewPoseCurrentIndex = i;

						for (int32 k = i; k < PoseCount; ++k)
						{
							const FPoseMotionData& EndPose = ActiveMotionDataAsset->Poses[k];

							if (k == PoseCount - 1)
							{
								PreviewPoseEndIndex = k;
								break;
							}

							if (EndPose.AnimId != CurrentAnimIndex
								|| EndPose.AnimType != CurrentAnimType
								|| EndPose.bMirrored != bMirrored)
							{
								PreviewPoseEndIndex = k - 1;
								break;
							}
						}

						break;
					}
				}
			}
			else
			{
				PreviewPoseCurrentIndex = INDEX_NONE;
				PreviewPoseEndIndex = INDEX_NONE;
				PreviewPoseStartIndex = INDEX_NONE;
			}

			switch (CurrentAnimType)
			{
				case EMotionAnimAssetType::Sequence:
				{
					SetPreviewAnimation(ActiveMotionDataAsset->GetEditableSourceSequenceAtIndex(AnimIndex)); 
					CurrentAnimName = GetAnimationName(CurrentAnimIndex);
				} break;
				case EMotionAnimAssetType::BlendSpace: 
				{
					SetPreviewAnimation(ActiveMotionDataAsset->GetEditableSourceBlendSpaceAtIndex(AnimIndex));
					CurrentAnimName = GetBlendSpaceName(CurrentAnimIndex);
				} break;
				case EMotionAnimAssetType::Composite: 
				{
					SetPreviewAnimation(ActiveMotionDataAsset->GetEditableSourceCompositeAtIndex(AnimIndex)); 
					CurrentAnimName = GetCompositeName(CurrentAnimIndex);
				} break;
				default: break;
			}

			CacheTrajectory(bMirrored);

			//Set the anim meta data as the AnimDetailsViewObject
			if (AnimDetailsView.IsValid())
			{
				AnimDetailsView->SetObject(ActiveMotionDataAsset->GetEditableSourceAnim(CurrentAnimIndex, CurrentAnimType));
			}
		}
	}
	else
	{
		CurrentAnimIndex = INDEX_NONE;
		CurrentAnimType = EMotionAnimAssetType::None;
		PreviewPoseCurrentIndex = INDEX_NONE;
		PreviewPoseEndIndex = INDEX_NONE;
		PreviewPoseStartIndex = INDEX_NONE;
		SetPreviewAnimationNull();

		if (AnimDetailsView.IsValid() && AnimIndex != CurrentAnimIndex)
		{
			AnimDetailsView->SetObject(nullptr);
		}
	}
}

void FMotionPreProcessToolkit::SetAnimationSelection(const TArray<TSharedPtr<FAnimTreeItem>>& SelectedItems)
{
	ActiveMotionDataAsset->ClearMotionSelection();
	
	EMotionAnimAssetType AcceptableType = EMotionAnimAssetType::None;
	for(TSharedPtr<FAnimTreeItem> Item : SelectedItems)
	{
		if(Item.IsValid())
		{
			const int32 AnimIndex = Item->GetAnimId();
			const EMotionAnimAssetType AnimType = Item->GetAnimType();

			if(!IsValidAnim(AnimIndex, AnimType))
			{
				continue;
			}

			if(AcceptableType == EMotionAnimAssetType::None)
			{
				AcceptableType = AnimType;
			}

			if(AnimType == AcceptableType)
			{
				//Add it to the selection list
				ActiveMotionDataAsset->AddSelectedMotion(AnimIndex, AnimType);
			}
		}
	}

	if(AnimDetailsView.IsValid())
	{
		AnimDetailsView->SetObjects(ActiveMotionDataAsset->GetSelectedMotions(), true);
	}
}

TObjectPtr<UMotionAnimObject> FMotionPreProcessToolkit::GetCurrentMotionAnim() const
{
	if(!ActiveMotionDataAsset)
	{
		UE_LOG(LogTemp, Error, TEXT("Cannot get current motion anim as the ActiveMotionDataAsset is null."))
		return nullptr;
	}

	return ActiveMotionDataAsset->GetEditableSourceAnim(CurrentAnimIndex, CurrentAnimType);
}

UAnimSequence* FMotionPreProcessToolkit::GetCurrentAnimation() const
{
	if (!ActiveMotionDataAsset)
	{
		UE_LOG(LogTemp, Error, TEXT("Cannot get current animation as the ActiveMotionDataAsset is null."))
		return nullptr;
	}

	if (ActiveMotionDataAsset->IsValidSourceAnimIndex(CurrentAnimIndex))
	{
		if (UAnimSequence* CurrentAnim = ActiveMotionDataAsset->GetEditableSourceSequenceAtIndex(CurrentAnimIndex)->Sequence)
		{
			check(CurrentAnim);
			return(CurrentAnim);
		}
	}

	return nullptr;
}

void FMotionPreProcessToolkit::DeleteAnimSequence(const int32 AnimIndex)
{
	if (AnimIndex == CurrentAnimIndex)
	{
		CurrentAnimIndex = INDEX_NONE;
		CurrentAnimType = EMotionAnimAssetType::None;
		SetPreviewAnimationNull();
		AnimDetailsView->SetObject(nullptr, true);
	}

	ActiveMotionDataAsset->DeleteSourceAnim(AnimIndex);
	AnimationTreePtr.Get()->RebuildAnimTree();

	if (ActiveMotionDataAsset->GetSourceAnimCount() == 0)
	{
		CurrentAnimIndex = INDEX_NONE;
		CurrentAnimType = EMotionAnimAssetType::None;
		SetPreviewAnimationNull();
		AnimDetailsView->SetObject(nullptr, true);
	}
}

void FMotionPreProcessToolkit::DeleteBlendSpace(const int32 BlendSpaceIndex)
{
	if (BlendSpaceIndex == CurrentAnimIndex)
	{
		CurrentAnimIndex = INDEX_NONE;
		CurrentAnimType = EMotionAnimAssetType::None;
		SetPreviewAnimationNull();
		AnimDetailsView->SetObject(nullptr, true);
	}

	ActiveMotionDataAsset->DeleteSourceBlendSpace(BlendSpaceIndex);
	//AnimationListPtr.Get()->Rebuild();
	AnimationTreePtr.Get()->RebuildAnimTree();

	if (ActiveMotionDataAsset->GetSourceBlendSpaceCount() == 0)
	{
		CurrentAnimIndex = INDEX_NONE;
		CurrentAnimType = EMotionAnimAssetType::None;
		SetPreviewAnimationNull();
		AnimDetailsView->SetObject(nullptr, true);
	}
}

void FMotionPreProcessToolkit::DeleteComposite(const int32 CompositeIndex)
{
	if (CompositeIndex == CurrentAnimIndex)
	{
		CurrentAnimIndex = INDEX_NONE;
		CurrentAnimType = EMotionAnimAssetType::None;
		SetPreviewAnimationNull();
		AnimDetailsView->SetObject(nullptr, true);
	}

	ActiveMotionDataAsset->DeleteSourceComposite(CompositeIndex);
	AnimationTreePtr.Get()->RebuildAnimTree();

	if (ActiveMotionDataAsset->GetSourceCompositeCount() == 0)
	{
		CurrentAnimIndex = INDEX_NONE;
		CurrentAnimType = EMotionAnimAssetType::None;
		SetPreviewAnimationNull();
		AnimDetailsView->SetObject(nullptr, true);
	}
}

void FMotionPreProcessToolkit::CopyAnim(const int32 CopyAnimIndex, const EMotionAnimAssetType AnimType) const
{
	switch(AnimType)
	{
		case EMotionAnimAssetType::Sequence:
		{
			if(TObjectPtr<const UMotionSequenceObject> MotionSequence = ActiveMotionDataAsset->GetSourceSequenceAtIndex(CopyAnimIndex))
			{
				CopiedSequences.Emplace(NewObject<UMotionSequenceObject>());
				CopiedSequences.Last()->Initialize(MotionSequence);
				for(const FAnimNotifyEvent& CopyNotify : MotionSequence->Tags)
				{
					CopyPasteNotify(CopiedSequences.Last(), CopyNotify);
				}
			}
				
		} break;
		case EMotionAnimAssetType::Composite:
		{
			if(TObjectPtr<const UMotionCompositeObject> MotionComposite = ActiveMotionDataAsset->GetSourceCompositeAtIndex(CopyAnimIndex))
			{
				if(!MotionComposite)
				{
					return;
				}
				
				CopiedComposites.Emplace(NewObject<UMotionCompositeObject>());
				CopiedComposites.Last()->Initialize(MotionComposite);
				for(const FAnimNotifyEvent& CopyNotify : MotionComposite->Tags)
				{
					CopyPasteNotify(CopiedComposites.Last(), CopyNotify);
				}
			}
		} break;
		case EMotionAnimAssetType::BlendSpace:
		{
			if(TObjectPtr<const UMotionBlendSpaceObject> MotionBlendSpace = ActiveMotionDataAsset->GetSourceBlendSpaceAtIndex(CopyAnimIndex))
			{
				CopiedBlendSpaces.Emplace(NewObject<UMotionBlendSpaceObject>());
				CopiedBlendSpaces.Last()->Initialize(MotionBlendSpace);
				for(const FAnimNotifyEvent& CopyNotify : MotionBlendSpace->Tags)
				{
					CopyPasteNotify(CopiedBlendSpaces.Last(), CopyNotify);
				}
			}
		} break;
		default: ;
	}
}

void FMotionPreProcessToolkit::PasteAnims()
{
	if(!ActiveMotionDataAsset)
	{
		return;
	}

	ActiveMotionDataAsset->Modify();
	
	for(TObjectPtr<UMotionSequenceObject> MotionSequence : CopiedSequences)
	{
		if(!MotionSequence)
		{
			continue;
		}
		
		TArray<TObjectPtr<UMotionSequenceObject>>& SequenceArray = ActiveMotionDataAsset->SourceMotionSequenceObjects;
		
		SequenceArray.Emplace(NewObject<UMotionSequenceObject>(ActiveMotionDataAsset, UMotionSequenceObject::StaticClass()));
		if(TObjectPtr<UMotionSequenceObject> NewMotionAnim = SequenceArray.Last())
		{
			NewMotionAnim->Initialize(MotionSequence);
			NewMotionAnim->ParentMotionDataAsset = ActiveMotionDataAsset;
			NewMotionAnim->AnimId = SequenceArray.Num() - 1;

			for(const FAnimNotifyEvent& CopyNotify : MotionSequence->Tags)
			{
				CopyPasteNotify(NewMotionAnim, CopyNotify);
			}
		}
	}
	
	for(TObjectPtr<UMotionCompositeObject> MotionComposite : CopiedComposites)
	{
		if(!MotionComposite)
		{
			continue;
		}
		
		TArray<TObjectPtr<UMotionCompositeObject>> CompositeArray = ActiveMotionDataAsset->SourceCompositeObjects;
		
		CompositeArray.Emplace(NewObject<UMotionCompositeObject>(ActiveMotionDataAsset, UMotionCompositeObject::StaticClass()));
		if(TObjectPtr<UMotionCompositeObject> NewMotionComposite = CompositeArray.Last())
		{
			NewMotionComposite->Initialize(MotionComposite);
			NewMotionComposite->ParentMotionDataAsset = ActiveMotionDataAsset;
			NewMotionComposite->AnimId = CompositeArray.Num() - 1;

			for(FAnimNotifyEvent CopyNotify : MotionComposite->Tags)
			{
				CopyPasteNotify(NewMotionComposite, CopyNotify);
			}
		}
	}
	
	for(TObjectPtr<UMotionBlendSpaceObject> MotionBlendSpace : CopiedBlendSpaces)
	{	
		if(!MotionBlendSpace)
		{
			continue;
		}
		
		TArray<TObjectPtr<UMotionBlendSpaceObject>> BlendSpaceArray = ActiveMotionDataAsset->SourceBlendSpaceObjects;
		
		BlendSpaceArray.Emplace(NewObject<UMotionBlendSpaceObject>(ActiveMotionDataAsset, UMotionBlendSpaceObject::StaticClass()));
		if(TObjectPtr<UMotionBlendSpaceObject> NewMotionBlendSpace = BlendSpaceArray.Last())
		{
			NewMotionBlendSpace->Initialize(NewMotionBlendSpace);
			NewMotionBlendSpace->ParentMotionDataAsset = ActiveMotionDataAsset;
			NewMotionBlendSpace->AnimId = BlendSpaceArray.Num() - 1;

			for(FAnimNotifyEvent CopyNotify : MotionBlendSpace->Tags)
			{
				CopyPasteNotify(NewMotionBlendSpace, CopyNotify);
			}
		}
	}
	
	ActiveMotionDataAsset->bIsProcessed = false;
}

void FMotionPreProcessToolkit::ClearCopyClipboard()
{
	CopiedSequences.Empty();
	CopiedComposites.Empty();
	CopiedBlendSpaces.Empty();
}

void FMotionPreProcessToolkit::ClearAnimList()
{
	if (FMessageDialog::Open(EAppMsgType::YesNo, LOCTEXT("Clear Source Anim List",
		"Are you sure you want to remove all animations from the anim list?"))
		== EAppReturnType::No)
	{
		return;
	}

	CurrentAnimIndex = INDEX_NONE;
	CurrentAnimType = EMotionAnimAssetType::None;
	SetPreviewAnimationNull();

	AnimDetailsView->SetObject(nullptr, true);

	//Delete All Sequences
	for (int32 AnimIndex = ActiveMotionDataAsset->GetSourceAnimCount() - 1; AnimIndex > -1; --AnimIndex)
	{
		DeleteAnimSequence(AnimIndex);
	}

	//Delete All Composites
	for (int32 AnimIndex = ActiveMotionDataAsset->GetSourceCompositeCount() - 1; AnimIndex > -1; --AnimIndex)
	{
		DeleteComposite(AnimIndex);
	}

	//Delete All BlendSpaces
	for (int32 AnimIndex = ActiveMotionDataAsset->GetSourceBlendSpaceCount() - 1; AnimIndex > -1; --AnimIndex)
	{
		DeleteBlendSpace(AnimIndex);
	}
	
	AnimationTreePtr.Get()->RebuildAnimTree();
}

void FMotionPreProcessToolkit::ClearBlendSpaceList()
{
	if (FMessageDialog::Open(EAppMsgType::YesNo, LOCTEXT("Clear Source BlendSpace List",
		"Are you sure you want to remove all blend spaces from the anim list?"))
		== EAppReturnType::No)
	{
		return;
	}
	SetPreviewAnimationNull();
	AnimDetailsView->SetObject(nullptr, true);

	//Delete All BlendSpaces
	for (int32 AnimIndex = ActiveMotionDataAsset->GetSourceBlendSpaceCount() - 1; AnimIndex > -1; --AnimIndex)
	{
		DeleteBlendSpace(AnimIndex);
	}
	
	AnimationTreePtr.Get()->RebuildAnimTree();
}

void FMotionPreProcessToolkit::ClearCompositeList()
{
	if (FMessageDialog::Open(EAppMsgType::YesNo, LOCTEXT("Clear Source Composite List",
		"Are you sure you want to remove all composites from the anim list?"))
		== EAppReturnType::No)
	{
		return;
	}

	SetPreviewAnimationNull();
	AnimDetailsView->SetObject(nullptr, true);

	//Delete All Composites
	for (int32 AnimIndex = ActiveMotionDataAsset->GetSourceCompositeCount() - 1; AnimIndex > -1; --AnimIndex)
	{
		DeleteComposite(AnimIndex);
	}
	
	AnimationTreePtr.Get()->RebuildAnimTree();
}

void FMotionPreProcessToolkit::AddNewAnimSequences(TArray<UAnimSequence*> SequenceList)
{
	SAnimTree* AnimTree = AnimationTreePtr.Get();
	for (int32 i = 0; i < SequenceList.Num(); ++i)
	{
		UAnimSequence* AnimSequence = SequenceList[i];

		if (AnimSequence)
		{
			ActiveMotionDataAsset->AddSourceAnim(AnimSequence);
		
			if(AnimTree)
			{
				const int32 AnimId = ActiveMotionDataAsset->SourceMotionSequenceObjects.Num() - 1;
				AnimTree->AddAnimSequence(AnimSequence, AnimId);
			}
		}
	}
}
void FMotionPreProcessToolkit::AddNewBlendSpaces(TArray<UBlendSpace*> BlendSpaceList)
{
	SAnimTree* AnimTree = AnimationTreePtr.Get();
	for (int32 i = 0; i < BlendSpaceList.Num(); ++i)
	{
		UBlendSpace* BlendSpace = BlendSpaceList[i];

		if (BlendSpace)
		{
			ActiveMotionDataAsset->AddSourceBlendSpace(BlendSpace);

			if(AnimTree)
			{
				const int32 AnimId = ActiveMotionDataAsset->SourceBlendSpaceObjects.Num() - 1;
				AnimTree->AddBlendSpace(BlendSpace, AnimId);
			}
		}
	}
}

void FMotionPreProcessToolkit::AddNewComposites(TArray<UAnimComposite*> CompositeList)
{
	SAnimTree* AnimTree = AnimationTreePtr.Get();
	for (int32 i = 0; i < CompositeList.Num(); ++i)
	{
		UAnimComposite* Composite = CompositeList[i];

		if (Composite)
		{
			ActiveMotionDataAsset->AddSourceComposite(Composite);

			if(AnimTree)
			{
				const int32 AnimId = ActiveMotionDataAsset->SourceCompositeObjects.Num() - 1;
				AnimTree->AddAnimComposite(Composite, AnimId);
			}
		}
	}
}

void FMotionPreProcessToolkit::SelectPreviousAnim()
{
	if (CurrentAnimIndex < 1)
	{
		switch (CurrentAnimType)
		{
			case EMotionAnimAssetType::Sequence:
			{
				SetCurrentAnimation(ActiveMotionDataAsset->GetSourceBlendSpaceCount() - 1, EMotionAnimAssetType::BlendSpace, false);
			} break;
			case EMotionAnimAssetType::BlendSpace:
			{
				SetCurrentAnimation(ActiveMotionDataAsset->GetSourceCompositeCount() - 1, EMotionAnimAssetType::Composite, false);
			} break;
			case EMotionAnimAssetType::Composite:
			{
				SetCurrentAnimation(ActiveMotionDataAsset->GetSourceAnimCount() - 1, EMotionAnimAssetType::Sequence, false);
			} break;
		default: break;
		}
	}
	else
	{
		SetCurrentAnimation(CurrentAnimIndex - 1, CurrentAnimType, false);
	}
}

void FMotionPreProcessToolkit::SelectNextAnim()
{
	switch (CurrentAnimType)
	{
		case EMotionAnimAssetType::Sequence:
		{
			if (CurrentAnimIndex >= ActiveMotionDataAsset->GetSourceAnimCount() - 1)
			{
				SetCurrentAnimation(0, EMotionAnimAssetType::Composite, false);
				return;
			}
		} break;
		case EMotionAnimAssetType::BlendSpace:
		{
			if(CurrentAnimIndex >= ActiveMotionDataAsset->GetSourceBlendSpaceCount() - 1)
			{
				SetCurrentAnimation(0, EMotionAnimAssetType::Sequence, false);
				return;
			}
		} break;
		case EMotionAnimAssetType::Composite:
		{
			if(CurrentAnimIndex >= ActiveMotionDataAsset->GetSourceCompositeCount() -1)
			{
				SetCurrentAnimation(0, EMotionAnimAssetType::BlendSpace, false);
				return;
			}
		} break;
	default: break;
	}

	SetCurrentAnimation(CurrentAnimIndex + 1, CurrentAnimType, false);
}

void FMotionPreProcessToolkit::RefreshAnim()
{
	SetCurrentAnimation(CurrentAnimIndex, CurrentAnimType, true);
}

FString FMotionPreProcessToolkit::GetSkeletonName() const
{
	USkeleton* Skeleton = GetSkeleton();

	if(!Skeleton)
	{
		UE_LOG(LogTemp, Error, TEXT("Cannot get skeleton name as the motion match config does not have a skeleton set."));
		return FString();
	}
	
	return Skeleton->GetName();
}

USkeleton* FMotionPreProcessToolkit::GetSkeleton() const
{
	if(!ActiveMotionDataAsset
		|| !ActiveMotionDataAsset->MotionMatchConfig)
	{
		UE_LOG(LogTemp, Error, TEXT("Cannot get skeleton motion match config could not be found."));
		return nullptr;
	}

	
	return ActiveMotionDataAsset->MotionMatchConfig->GetSourceSkeleton();
}

void FMotionPreProcessToolkit::SetSkeleton(USkeleton* Skeleton) const
{
	if(!ActiveMotionDataAsset)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to set skeleton as the ActiveMotionDataAsset was null"));
	}
	
	ActiveMotionDataAsset->SetSkeleton(Skeleton);
}

bool FMotionPreProcessToolkit::AnimationAlreadyAdded(const FName SequenceName) const
{
	const int32 SequenceCount = ActiveMotionDataAsset->GetSourceAnimCount();
	for (int32 i = 0; i < SequenceCount; ++i)
	{
		TObjectPtr<UMotionSequenceObject> MotionAnim = ActiveMotionDataAsset->GetEditableSourceSequenceAtIndex(i);
		if (MotionAnim && MotionAnim->Sequence && MotionAnim->Sequence->GetFName() == SequenceName)
		{
			return true;
		}
	}

	const int32 BlendSpaceCount = ActiveMotionDataAsset->GetSourceBlendSpaceCount();
	for(int32 i = 0; i < BlendSpaceCount; ++i)
	{
		TObjectPtr<UMotionBlendSpaceObject> MotionBlendSpace = ActiveMotionDataAsset->GetEditableSourceBlendSpaceAtIndex(i);
		if (MotionBlendSpace && MotionBlendSpace->BlendSpace && MotionBlendSpace->BlendSpace->GetFName() == SequenceName)
		{
			return true;
		}
	}

	const int32 CompositeCount = ActiveMotionDataAsset->GetSourceCompositeCount();
	for(int32 i = 0; i < CompositeCount; ++i)
	{
		TObjectPtr<UMotionCompositeObject> MotionComposite = ActiveMotionDataAsset->GetEditableSourceCompositeAtIndex(i);
		if(MotionComposite && MotionComposite->AnimComposite && MotionComposite->AnimComposite->GetFName() == SequenceName)
		{
			return true;
		}
	}

	return false;
}

bool FMotionPreProcessToolkit::IsValidAnim(const int32 AnimIndex) const
{
	if (ActiveMotionDataAsset->IsValidSourceAnimIndex(AnimIndex)
		&& ActiveMotionDataAsset->GetSourceSequenceAtIndex(AnimIndex)->Sequence)
	{
			return true;
	}

	return false;
}

bool FMotionPreProcessToolkit::IsValidAnim(const int32 AnimIndex, const EMotionAnimAssetType AnimType) const
{
	switch(AnimType)
	{
		case EMotionAnimAssetType::Sequence:
		{
			if (ActiveMotionDataAsset->IsValidSourceAnimIndex(AnimIndex)
				&& ActiveMotionDataAsset->GetSourceSequenceAtIndex(AnimIndex)->Sequence)
			{
				return true;
			}
		} break;
		case EMotionAnimAssetType::BlendSpace:
		{
			if (ActiveMotionDataAsset->IsValidSourceBlendSpaceIndex(AnimIndex)
				&& ActiveMotionDataAsset->GetSourceBlendSpaceAtIndex(AnimIndex)->BlendSpace)
			{
				return true;
			}
		} break;
		case EMotionAnimAssetType::Composite:
		{
			if (ActiveMotionDataAsset->IsValidSourceCompositeIndex(AnimIndex)
				&& ActiveMotionDataAsset->GetSourceCompositeAtIndex(AnimIndex)->AnimComposite)
			{
				return true;
			}
		} break;
	}

	return false;
}

bool FMotionPreProcessToolkit::IsValidBlendSpace(const int32 BlendSpaceIndex) const
{
	if (ActiveMotionDataAsset->IsValidSourceBlendSpaceIndex(BlendSpaceIndex)
		&& ActiveMotionDataAsset->GetSourceBlendSpaceAtIndex(BlendSpaceIndex)->BlendSpace)
	{
		return true;
	}

	return false;
}

bool FMotionPreProcessToolkit::IsValidComposite(const int32 CompositeIndex) const
{
	if (ActiveMotionDataAsset->IsValidSourceCompositeIndex(CompositeIndex)
		&& ActiveMotionDataAsset->GetSourceCompositeAtIndex(CompositeIndex)->AnimComposite)
	{
		return true;
	}

	return false;
}

bool FMotionPreProcessToolkit::SetPreviewAnimation(TObjectPtr<UMotionAnimObject> MotionAnimAsset) const
{
	if(!MotionAnimAsset)
	{
		return false;
	}
	
	ViewportPtr->SetupAnimatedRenderComponent();
	
	UDebugSkelMeshComponent* DebugMeshComponent = GetPreviewSkeletonMeshComponent();
	if (!DebugMeshComponent || !DebugMeshComponent->GetSkinnedAsset())
	{
		return false;
	}

	MotionTimelinePtr->SetAnimation(MotionAnimAsset, DebugMeshComponent);

	if(MotionAnimAsset->bEnableMirroring
		&& ViewportPtr->IsMirror()
		&& ActiveMotionDataAsset->MirrorDataTable)
	{
		DebugMeshComponent->PreviewInstance->SetMirrorDataTable(ActiveMotionDataAsset->MirrorDataTable);
	}
	else
	{
		DebugMeshComponent->PreviewInstance->SetMirrorDataTable(nullptr);
	}

	if(UAnimationAsset* AnimAsset = MotionAnimAsset->AnimAsset)
	{
		if (AnimAsset->GetSkeleton() == DebugMeshComponent->GetSkinnedAsset()->GetSkeleton())
		{
			DebugMeshComponent->EnablePreview(true, AnimAsset);
			DebugMeshComponent->SetAnimation(AnimAsset);
			DebugMeshComponent->SetWorldTransform(FTransform::Identity);
			DebugMeshComponent->SetPlayRate(MotionAnimAsset->PlayRate);
			return true;
		}
		
		DebugMeshComponent->EnablePreview(true, nullptr);
		return false;
	}
	
	SetPreviewAnimationNull();
	return true;
}

void FMotionPreProcessToolkit::SetPreviewAnimationNull() const
{
	UDebugSkelMeshComponent* DebugMeshComponent = GetPreviewSkeletonMeshComponent();

	if (!DebugMeshComponent || !DebugMeshComponent->GetSkinnedAsset())
	{
		return;
	}

	MotionTimelinePtr->SetAnimation(nullptr, DebugMeshComponent);

	DebugMeshComponent->EnablePreview(true, nullptr);
}

UDebugSkelMeshComponent* FMotionPreProcessToolkit::GetPreviewSkeletonMeshComponent() const
{
	UDebugSkelMeshComponent* PreviewComponent = ViewportPtr->GetPreviewComponent();
	
	return PreviewComponent;
}

bool FMotionPreProcessToolkit::SetPreviewComponentSkeletalMesh(USkeletalMesh* SkeletalMesh) const
{
	UDebugSkelMeshComponent* PreviewSkelMeshComponent = GetPreviewSkeletonMeshComponent();

	if (!PreviewSkelMeshComponent)
	{
		return false;
	}

	if (SkeletalMesh)
	{
		USkeletalMesh* PreviewSkelMesh = PreviewSkelMeshComponent->GetSkeletalMeshAsset();

		if (PreviewSkelMesh)
		{
			if (PreviewSkelMesh->GetSkeleton() != SkeletalMesh->GetSkeleton())
			{
				SetPreviewAnimationNull();
				PreviewSkelMeshComponent->SetSkeletalMesh(SkeletalMesh, true);
				ViewportPtr->OnFocusViewportToSelection();
				return false;
			}
			else
			{
				PreviewSkelMeshComponent->SetSkeletalMesh(SkeletalMesh, false);
				ViewportPtr->OnFocusViewportToSelection();
				return true;
			}
		}
		
		SetPreviewAnimationNull();

		PreviewSkelMeshComponent->SetSkeletalMesh(SkeletalMesh, true);
		ViewportPtr->OnFocusViewportToSelection();
	}
	else
	{
		SetPreviewAnimationNull();
		PreviewSkelMeshComponent->SetSkeletalMesh(nullptr, true);
	}

	return false;
}

void FMotionPreProcessToolkit::SetCurrentFrame(int32 NewIndex)
{
	const int32 TotalLengthInFrames = GetTotalFrameCount();
	const int32 ClampedIndex = FMath::Clamp<int32>(NewIndex, 0, TotalLengthInFrames);
	SetPlaybackPosition(ClampedIndex / GetFramesPerSecond());
}

int32 FMotionPreProcessToolkit::GetCurrentFrame() const
{
	const int32 TotalLengthInFrames = GetTotalFrameCount();

	if (TotalLengthInFrames == 0)
	{
		return INDEX_NONE;
	}
	else
	{
		return FMath::Clamp<int32>((int32)(GetPlaybackPosition() 
			* GetFramesPerSecond()), 0, TotalLengthInFrames);
	}

}

float FMotionPreProcessToolkit::GetFramesPerSecond() const
{
	return 30.0f;
}

void FMotionPreProcessToolkit::PreProcessAnimData() const
{
	if (!ActiveMotionDataAsset)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to pre-process motion matching animation data. The Motion Data asset is null."));
		return;
	}

	if (!ActiveMotionDataAsset->CheckValidForPreProcess())
	{
		return;
	}

	ActiveMotionDataAsset->Modify();
	ActiveMotionDataAsset->PreProcess();
	//ActiveMotionDataAsset->MarkPackageDirty();
}

void FMotionPreProcessToolkit::OpenPickAnimsDialog()
{
	if (!ActiveMotionDataAsset)
	{
		return;
	}

	if (!ActiveMotionDataAsset->MotionMatchConfig)
	{
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("Cannot Add Anims With Null MotionMatchConfig",
			"Not 'MotionMatchConfig' has been set for this anim data. Please set it in the details panel before picking anims."));

		return;
	}

	if (ActiveMotionDataAsset->MotionMatchConfig->GetSourceSkeleton() == nullptr)
	{
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("Cannot Add Anims With Null Skeleton On MotionMatchConfig",
			"The MotionMatchConfig does not have a valid skeleton set. Please set up your 'MotionMatchConfig' with a valid skeleton."));

		return;
	}

	SAddNewAnimDialog::ShowWindow(AnimationTreePtr->MotionPreProcessToolkitPtr.Pin());
}

void FMotionPreProcessToolkit::CacheTrajectory(const bool bMirrored)
{
	if (!ActiveMotionDataAsset)
	{
		return;
	}

	const UMotionAnimObject* MotionAnim = ActiveMotionDataAsset->GetEditableSourceAnim(CurrentAnimIndex, CurrentAnimType);

	if (!MotionAnim)
	{
		CachedTrajectoryPoints.Empty();
		return;
	}

	CachedTrajectoryPoints.Empty(FMath::CeilToInt(MotionAnim->GetPlayLength() / 0.1f) + 1);

	//Step through the animation 0.1s at a time and record a trajectory point
	CachedTrajectoryPoints.Add(FVector(0.0f));

	MotionAnim->CacheTrajectoryPoints(CachedTrajectoryPoints, bMirrored);
}

void FMotionPreProcessToolkit::CopyPasteNotify(TObjectPtr<UMotionAnimObject> MotionAnimAsset, const FAnimNotifyEvent& InNotify) const
{
	if(!MotionAnimAsset)
	{
		return;
	}
	
	const int32 NewNotifyIndex = MotionAnimAsset->Tags.Add(FAnimNotifyEvent());
	FAnimNotifyEvent& NewEvent = MotionAnimAsset->Tags[NewNotifyIndex];
	
	NewEvent.NotifyName = InNotify.NotifyName;
	NewEvent.Guid = FGuid::NewGuid();

	UAnimSequenceBase* MotionSequence = Cast<UAnimSequenceBase>(MotionAnimAsset->AnimAsset);

	const float StartTime = InNotify.GetTriggerTime();
	const float EndTime = InNotify.GetEndTriggerTime();
	
	NewEvent.Link(MotionSequence, StartTime);
	NewEvent.TriggerTimeOffset = GetTriggerTimeOffsetForType(MotionSequence->CalculateOffsetForNotify(StartTime));
	NewEvent.EndTriggerTimeOffset = GetTriggerTimeOffsetForType(MotionSequence->CalculateOffsetForNotify(EndTime));
	NewEvent.TrackIndex = InNotify.TrackIndex;

	TSubclassOf<UObject> NotifyClass = nullptr;
	if(InNotify.Notify)
	{
		NotifyClass = InNotify.Notify.GetClass();
	}
	else if(InNotify.NotifyStateClass)
	{
		NotifyClass = InNotify.NotifyStateClass.GetClass();
	}
	
	if(NotifyClass)
	{
		UObject* AnimNotifyClass = NewObject<UObject>(ActiveMotionDataAsset, NotifyClass, NAME_None, RF_Transactional); //The outer object should probably be the MotionAnimData

		UTagPoint* TagPoint = Cast<UTagPoint>(AnimNotifyClass);
		UTagSection* TagSection = Cast<UTagSection>(AnimNotifyClass);

		if(TagPoint)
		{
			TagPoint->CopyTagData(Cast<UTagPoint>(InNotify.Notify));
		}
		else if(TagSection)
		{
			TagSection->CopyTagData(Cast<UTagSection>(InNotify.Notify));
		}
		
		NewEvent.NotifyStateClass = Cast<UAnimNotifyState>(AnimNotifyClass);
		NewEvent.Notify = Cast<UAnimNotify>(AnimNotifyClass);
		
		// Set default duration to 1 frame for AnimNotifyState.
		if (NewEvent.NotifyStateClass)
		{
			NewEvent.SetDuration(InNotify.GetDuration());
			NewEvent.EndLink.Link(Cast<UAnimSequenceBase>(MotionAnimAsset->AnimAsset), InNotify.EndLink.GetTime());
		}
	}
	else
	{
		NewEvent.Notify = NULL;
		NewEvent.NotifyStateClass = NULL;
	}

	if(NewEvent.Notify)
	{
		TArray<FAssetData> SelectedAssets;
		AssetSelectionUtils::GetSelectedAssets(SelectedAssets);

		for (TFieldIterator<FObjectProperty> PropIt(NewEvent.Notify->GetClass()); PropIt; ++PropIt)
		{
			if (PropIt->GetBoolMetaData(TEXT("ExposeOnSpawn")))
			{
				FObjectProperty* Property = *PropIt;
				const FAssetData* Asset = SelectedAssets.FindByPredicate([Property](const FAssetData& Other)
				{
					return Other.GetAsset()->IsA(Property->PropertyClass);
				});
			
				if (Asset)
				{
					uint8* Offset = (*PropIt)->ContainerPtrToValuePtr<uint8>(NewEvent.Notify);
					(*PropIt)->ImportText_Direct(*Asset->GetAsset()->GetPathName(), Offset, NewEvent.Notify, 0);
					break;
				}
			}
		}

		NewEvent.Notify->OnAnimNotifyCreatedInEditor(NewEvent);
	}
	else if(NewEvent.NotifyStateClass)
	{
		NewEvent.NotifyStateClass->OnAnimNotifyCreatedInEditor(NewEvent);
	}
}

void FMotionPreProcessToolkit::DrawCachedTrajectoryPoints(FPrimitiveDrawInterface* DrawInterface) const
{
	FVector lastPoint = FVector(0.0f);
	
	FLinearColor color = FLinearColor::Green;

	for (auto& point : CachedTrajectoryPoints)
	{
		DrawInterface->DrawLine(lastPoint, point, color, ESceneDepthPriorityGroup::SDPG_Foreground, 3.0f);
		lastPoint = point;
	}
}

bool FMotionPreProcessToolkit::GetPendingTimelineRebuild()
{
	return PendingTimelineRebuild;
}

void FMotionPreProcessToolkit::SetPendingTimelineRebuild(const bool IsPendingRebuild)
{
	PendingTimelineRebuild = IsPendingRebuild;
}

void FMotionPreProcessToolkit::HandleTagsSelected(const TArray<UObject*>& SelectedObjects)
{
	//Set the anim meta data as the AnimDetailsViewObject
	if (AnimDetailsView.IsValid() && SelectedObjects.Num() > 0)
	{
		AnimDetailsView->SetObjects(SelectedObjects, true);
	}
	
}

bool FMotionPreProcessToolkit::HasCopiedAnims()
{
	if(CopiedSequences.Num() > 0 
		|| CopiedComposites.Num() > 0 
		|| CopiedBlendSpaces.Num() > 0)
	{
		return true;
	}
	
	return false;
}

#undef LOCTEXT_NAMESPACE
