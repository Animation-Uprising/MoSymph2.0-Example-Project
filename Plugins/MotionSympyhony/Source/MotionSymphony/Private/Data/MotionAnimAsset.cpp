//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#include "Data/MotionAnimAsset.h"
#include "MotionDataAsset.h"
#include "Animation/AnimationAsset.h"
#include "Animation/AnimComposite.h"
#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace.h"

#define LOCTEXT_NAMESPACE "MotionAnimAsset"

FMotionAnimAsset::FMotionAnimAsset()
	: AnimId(0),
	MotionAnimAssetType(EMotionAnimAssetType::None),
	bLoop(false),
	PlayRate(1.0f),
	bEnableMirroring(false),
	bFlattenTrajectory(true),
	PastTrajectory(ETrajectoryPreProcessMethod::IgnoreEdges),
	FutureTrajectory(ETrajectoryPreProcessMethod::IgnoreEdges),
	AnimAsset(nullptr),
	PrecedingMotion(nullptr),
	FollowingMotion(nullptr),
	CostMultiplier(1.0f),
	ParentMotionDataAsset(nullptr)
{
}

FMotionAnimAsset::FMotionAnimAsset(const FMotionAnimAsset* Copy)
{
	if(!Copy)
	{
		return;
	}

	AnimId = Copy->AnimId;
	MotionAnimAssetType = Copy->MotionAnimAssetType;
	bLoop = Copy->bLoop;
	PlayRate = Copy->PlayRate;
	bEnableMirroring = Copy->bEnableMirroring;
	bFlattenTrajectory = Copy->bFlattenTrajectory;
	PastTrajectory = Copy->PastTrajectory;
	FutureTrajectory = Copy->FutureTrajectory;
	AnimAsset = Copy->AnimAsset;
	PrecedingMotion = Copy->PrecedingMotion;
	FollowingMotion = Copy->FollowingMotion;
	CostMultiplier = Copy->CostMultiplier;
	MotionTags = Copy->MotionTags;
	
#if WITH_EDITORONLY_DATA
	MotionTagTracks = Copy->MotionTagTracks;
#endif
}

FMotionAnimAsset::FMotionAnimAsset(UAnimationAsset* InAnimAsset, UMotionDataAsset* InParentMotionDataAsset)
	: AnimId(0),
	MotionAnimAssetType(EMotionAnimAssetType::None),
	bLoop(false),
	PlayRate(1.0f),
	bEnableMirroring(false),	
	bFlattenTrajectory(true),
	PastTrajectory(ETrajectoryPreProcessMethod::IgnoreEdges),
	FutureTrajectory(ETrajectoryPreProcessMethod::IgnoreEdges),
	AnimAsset(InAnimAsset),
	PrecedingMotion(nullptr),
	FollowingMotion(nullptr),
	CostMultiplier(1.0f),
	ParentMotionDataAsset(InParentMotionDataAsset)
{
}

FMotionAnimAsset::~FMotionAnimAsset()
{
	
}

float FMotionAnimAsset::GetPlayRate() const
{
	return PlayRate;
}

double FMotionAnimAsset::GetPlayLength() const
{
	return AnimAsset ? AnimAsset->GetPlayLength() : 0.0;
}

double FMotionAnimAsset::GetFrameRate() const
{
	return 30.0;
}

int32 FMotionAnimAsset::GetTickResolution() const
{
	return FMath::RoundToInt(GetFrameRate());
}

double FMotionAnimAsset::GetRateAdjustedPlayLength() const
{
	return GetPlayLength() / FMath::Max(0.01f, PlayRate);
}

double FMotionAnimAsset::GetRateAdjustedFrameRate() const
{
	return GetFrameRate() / FMath::Max(0.01f, PlayRate);
}

int32 FMotionAnimAsset::GetRateAdjustedTickResolution() const
{
	return FMath::RoundToInt(GetRateAdjustedFrameRate());
}

void FMotionAnimAsset::PostLoad()
{
#if WITH_EDITOR
	InitializeTagTrack();
#endif
	RefreshCacheData();
}

void FMotionAnimAsset::SortTags()
{
	Tags.Sort();
}

bool FMotionAnimAsset::RemoveTags(const TArray<FName>& TagsToRemove)
{
	bool bSequenceModifiers = false;
	for (int32 TagIndex = Tags.Num() - 1; TagIndex >= 0; --TagIndex)
	{
		FAnimNotifyEvent& AnimTags = Tags[TagIndex];
		if (TagsToRemove.Contains(AnimTags.NotifyName))
		{
			if (!bSequenceModifiers)
			{
				//Modify(); //This is a struct within a package, can't mark as modified here
				bSequenceModifiers = true;
			}

			Tags.RemoveAtSwap(TagIndex);
		}
	}

	if (bSequenceModifiers)
	{
		//MarkPackageDirty(); //This is a struct within a package. Can't mark it as dirty here
		RefreshCacheData();
	}

	return bSequenceModifiers;
}

void FMotionAnimAsset::GetMotionTags(const float& StartTime, const float& DeltaTime, const bool bAllowLooping, TArray<FAnimNotifyEventReference>& OutActiveNotifies) const
{
	if (DeltaTime == 0.f)
	{
		return;
	}

	// Early out if we have no notifies
	if (!IsTagAvailable())
	{
		return;
	}

	bool const bPlayingBackwards = false;
	float PreviousPosition = StartTime;
	float CurrentPosition = StartTime;
	float DesiredDeltaMove = DeltaTime;

	const float AnimLength = GetPlayLength();

	do
	{
		// Disable looping here. Advance to desired position, or beginning / end of animation 
		const ETypeAdvanceAnim AdvanceType = FAnimationRuntime::AdvanceTime(false, DesiredDeltaMove, CurrentPosition, AnimLength);

		// Verify position assumptions
	/*	ensureMsgf(bPlayingBackwards ? (CurrentPosition <= PreviousPosition) : (CurrentPosition >= PreviousPosition), TEXT("in Animation %s(Skeleton %s) : bPlayingBackwards(%d), PreviousPosition(%0.2f), Current Position(%0.2f)"),
			*GetName(), *GetNameSafe(GetSkeleton()), bPlayingBackwards, PreviousPosition, CurrentPosition);*/

		GetMotionTagsFromDeltaPositions(PreviousPosition, CurrentPosition, OutActiveNotifies);

		// If we've hit the end of the animation, and we're allowed to loop, keep going.
		if ((AdvanceType == ETAA_Finished) && bAllowLooping)
		{
			const float ActualDeltaMove = (CurrentPosition - PreviousPosition);
			DesiredDeltaMove -= ActualDeltaMove;

			PreviousPosition = bPlayingBackwards ? AnimLength : 0.f;
			CurrentPosition = PreviousPosition;
		}
		else
		{
			break;
		}
	} while (true);
}

void FMotionAnimAsset::GetMotionTagsFromDeltaPositions(const float& PreviousPosition, const float& CurrentPosition, TArray<FAnimNotifyEventReference>& OutActiveTags) const
{
	// Early out if we have no notifies
	if ((Tags.Num() == 0) || (PreviousPosition > CurrentPosition))
	{
		return;
	}

	bool const bPlayingBackwards = false;

	for (int32 TagIndex = 0; TagIndex < Tags.Num(); ++TagIndex)
	{
		const FAnimNotifyEvent& AnimNotifyEvent = Tags[TagIndex];
		const float TagStartTime = AnimNotifyEvent.GetTriggerTime();
		const float TagEndTime = AnimNotifyEvent.GetEndTriggerTime();

		if ((TagStartTime <= CurrentPosition) && (TagEndTime > PreviousPosition))
		{
			//OutActiveTags.Emplace(&AnimNotifyEvent, this);
			OutActiveTags.Emplace(&AnimNotifyEvent, AnimAsset);
		}
	}
}

void FMotionAnimAsset::GetRootBoneTransform(FTransform& OutTransform, const float Time, const bool bMirrored) const
{
	OutTransform = FTransform::Identity;
}

void FMotionAnimAsset::CacheTrajectoryPoints(TArray<FVector>& OutTrajectoryPoints, const bool bMirrored) const
{
	OutTrajectoryPoints.Empty();
}

void FMotionAnimAsset::InitializeTagTrack()
{
#if WITH_EDITORONLY_DATA
	if (MotionTagTracks.Num() == 0)
	{
		MotionTagTracks.Add(FAnimNotifyTrack(TEXT("1"), FLinearColor::White));
	}
#endif
}

void FMotionAnimAsset::ClampTagAtEndOfSequence()
{
	const float AnimLength = GetPlayLength();
	const float TagClampTime = AnimLength - 0.01f; //Slight offset so that notify is still draggable
	for (int i = 0; i < Tags.Num(); ++i)
	{
		if (Tags[i].GetTime() >= AnimLength)
		{
			Tags[i].SetTime(TagClampTime);
			Tags[i].TriggerTimeOffset = GetTriggerTimeOffsetForType(EAnimEventTriggerOffsets::OffsetBefore);
		}
	}
}

uint8* FMotionAnimAsset::FindTagPropertyData(int32 TagIndex, FArrayProperty*& ArrayProperty)
{
	ArrayProperty = nullptr;

	if(Tags.IsValidIndex(TagIndex))
	{
		return FindArrayProperty(TEXT("SourceMotionAnims"), ArrayProperty, TagIndex);
	}

	return nullptr;
}

uint8* FMotionAnimAsset::FindArrayProperty(const TCHAR* PropName, FArrayProperty*& ArrayProperty, int32 ArrayIndex)
{
	// find Notifies property start point
	FProperty* Property = FindFProperty<FProperty>(ParentMotionDataAsset->GetClass(), PropName); //Todo: is GetClass() correct here?
	
	// found it and if it is array
	if (Property && Property->IsA(FArrayProperty::StaticClass()))
	{
		// find Property Value from UObject we got
		uint8* PropertyValue = Property->ContainerPtrToValuePtr<uint8>(this);

		// it is array, so now get ArrayHelper and find the raw ptr of the data
		ArrayProperty = CastFieldChecked<FArrayProperty>(Property);
		FScriptArrayHelper ArrayHelper(ArrayProperty, PropertyValue);

		if (ArrayProperty->Inner && ArrayIndex < ArrayHelper.Num())
		{
			//Get property data based on selected index
			return ArrayHelper.GetRawPtr(ArrayIndex);
		}
	}
	return nullptr;
}

//namespace MotionSymphony
// {
// 	bool CanNotifyUseTrack(const FAnimNotifyTrack& Track, const FAnimNotifyEvent& Notify)
// 	{
// 		for (const FAnimNotifyEvent* Event : Track.Notifies)
// 		{
// 			if (FMath::IsNearlyEqual(Event->GetTime(), Notify.GetTime()))
// 			{
// 				return false;
// 			}
// 		}
// 		return true;
// 	}
//
// 	FAnimNotifyTrack& AddNewTrack(TArray<FAnimNotifyTrack>& Tracks)
// 	{
// 		const int32 Index = Tracks.Add(FAnimNotifyTrack(*FString::FromInt(Tracks.Num() + 1), FLinearColor::White));
// 		return Tracks[Index];
// 	}
// }

void FMotionAnimAsset::RefreshCacheData()
{
	SortTags();

// #if WITH_EDITORONLY_DATA
// 	for (int32 TrackIndex = 0; TrackIndex < MotionTagTracks.Num(); ++TrackIndex)
// 	{
// 		MotionTagTracks[TrackIndex].Notifies.Empty();
// 	}
//
// 	for (FAnimNotifyEvent& Tag : Tags)
// 	{
// 		if (!MotionTagTracks.IsValidIndex(Tag.TrackIndex))
// 		{
// 			// This really shouldn't happen (unless we are a cooked asset), but try to handle it
// 			//ensureMsgf(GetOutermost()->bIsCookedForEditor, TEXT("AnimNotifyTrack: Anim (%s) has notify (%s) with track index (%i) that does not exist"), *GetFullName(), *Notify.NotifyName.ToString(), Notify.TrackIndex);
//
// 			if (Tag.TrackIndex < 0 || Tag.TrackIndex > 20)
// 			{
// 				Tag.TrackIndex = 0;
// 			}
//
// 			while (!MotionTagTracks.IsValidIndex(Tag.TrackIndex))
// 			{
// 				MotionSymphony::AddNewTrack(MotionTagTracks);
// 			}
// 		}
//
// 		//Handle overlapping tags
// 		FAnimNotifyTrack* TrackToUse = nullptr;
// 		int32 TrackIndexToUse = INDEX_NONE;
// 		for (int32 TrackOffset = 0; TrackOffset < MotionTagTracks.Num(); ++TrackOffset)
// 		{
// 			const int32 TrackIndex = (Tag.TrackIndex + TrackOffset) % MotionTagTracks.Num();
// 			if (MotionSymphony::CanNotifyUseTrack(MotionTagTracks[TrackIndex], Tag))
// 			{
// 				TrackToUse = &MotionTagTracks[TrackIndex];
// 				TrackIndexToUse = TrackIndex;
// 				break;
// 			}
// 		}
//
// 		if (TrackToUse == nullptr)
// 		{
// 			TrackToUse = &MotionSymphony::AddNewTrack(MotionTagTracks);
// 			TrackIndexToUse = MotionTagTracks.Num() - 1;
// 		}
//
// 		check(TrackToUse);
// 		check(TrackIndexToUse != INDEX_NONE);
//
// 		Tag.TrackIndex = TrackIndexToUse;
// 		TrackToUse->Notifies.Add(&Tag);
// 	}
//
// 	// this is a separate loop of checking if they contains valid notifies
// 	for (int32 TagIndex = 0; TagIndex < Tags.Num(); ++TagIndex)
// 	{
// 		const FAnimNotifyEvent& Tag = Tags[TagIndex];
// 		//make sure if they can be placed in editor
// 		if (Tag.Notify)
// 		{
// 			//if (Tag.Notify->CanBePlaced(this) == false)
// 			//{
// 			//	static FName NAME_LoadErrors("LoadErrors");
// 			//	FMessageLog LoadErrors(NAME_LoadErrors);
//
// 			//	TSharedRef<FTokenizedMessage> Message = LoadErrors.Error();
// 			//	Message->AddToken(FTextToken::Create(LOCTEXT("InvalidMotionTag1", "The Animation ")));
// 			//	//Message->AddToken(FAssetNameToken::Create(GetPathName(), FText::FromString(GetNameSafe(this))));
// 			//	Message->AddToken(FTextToken::Create(LOCTEXT("InvalidMotionTag2", " contains invalid tag ")));
// 			//	Message->AddToken(FAssetNameToken::Create(Tag.Notify->GetPathName(), FText::FromString(GetNameSafe(Tag.Notify))));
// 			//	LoadErrors.Open();
// 			//}
// 		}
//
// 		if (Tag.NotifyStateClass)
// 		{
// 			//if (Tag.NotifyStateClass->CanBePlaced(this) == false)
// 			//{
// 			//	static FName NAME_LoadErrors("LoadErrors");
// 			//	FMessageLog LoadErrors(NAME_LoadErrors);
//
// 			//	TSharedRef<FTokenizedMessage> Message = LoadErrors.Error();
// 			//	Message->AddToken(FTextToken::Create(LOCTEXT("InvalidMotionTag1", "The Animation ")));
// 			//	//Message->AddToken(FAssetNameToken::Create(GetPathName(), FText::FromString(GetNameSafe(this))));
// 			//	Message->AddToken(FTextToken::Create(LOCTEXT("InvalidMotionTag2", " contains invalid Tag ")));
// 			//	Message->AddToken(FAssetNameToken::Create(Tag.NotifyStateClass->GetPathName(), FText::FromString(GetNameSafe(Tag.NotifyStateClass))));
// 			//	LoadErrors.Open();
// 			//}
// 		}
// 	}
// 	//tag broadcast
// 	OnTagChanged.Broadcast();
// #endif //WITH_EDITORONLY_DATA
}


#if WITH_EDITOR
void FMotionAnimAsset::RegisterOnTagChanged(const FOnTagChanged& Delegate)
{
	OnTagChanged.Add(Delegate);
}

void FMotionAnimAsset::UnRegisterOnTagChanged(void* Unregister)
{
	if (!Unregister)
	{
		return;
	}

	OnTagChanged.RemoveAll(Unregister);
}
#endif

bool FMotionAnimAsset::IsTagAvailable() const
{
	return (Tags.Num() != 0) && (GetPlayLength() > 0.0f);
}

FMotionAnimSequence::FMotionAnimSequence()
	: FMotionAnimAsset(),
	Sequence(nullptr)
{
	MotionAnimAssetType = EMotionAnimAssetType::Sequence;
}

FMotionAnimSequence::FMotionAnimSequence(const FMotionAnimSequence* Copy)
	: FMotionAnimAsset(Copy),
	Sequence(nullptr)
{
	if(!Copy)
	{
		return;
	}

	Sequence = Copy->Sequence;
}

FMotionAnimSequence::FMotionAnimSequence(UAnimSequence* InSequence, UMotionDataAsset* InParentMotionData)
	: FMotionAnimAsset(InSequence, InParentMotionData),
	Sequence(InSequence)
{
	MotionAnimAssetType = EMotionAnimAssetType::Sequence;
}


FMotionAnimSequence::~FMotionAnimSequence()
{
}

double FMotionAnimSequence::GetPlayLength() const
{
	return Sequence ? Sequence->GetPlayLength() : 0.0f;
}

double FMotionAnimSequence::GetFrameRate() const
{
	return Sequence ? Sequence->GetSamplingFrameRate().AsDecimal() : 30.0;
}

void FMotionAnimSequence::GetRootBoneTransform(FTransform& OutTransform, const float Time, const bool bMirrored) const
{
	if (!Sequence)
	{
		OutTransform = FTransform::Identity;
		return;
	}

	Sequence->GetBoneTransform(OutTransform, FSkeletonPoseBoneIndex(0), Time, false);

	if(bMirrored)
	{
		OutTransform.Mirror(EAxis::X, EAxis::X);
	}
}

void FMotionAnimSequence::CacheTrajectoryPoints(TArray<FVector>& OutTrajectoryPoints, const bool bMirrored) const
{
	for (float Time = 0.1f; Time < Sequence->GetPlayLength(); Time += 0.1f)
	{
		FTransform TrajectoryPointTransform = Sequence->ExtractRootMotion(0.0f, Time, false);

		if(bMirrored)
		{
			TrajectoryPointTransform.Mirror(EAxis::X, EAxis::X);
		}
		
		OutTrajectoryPoints.Add(TrajectoryPointTransform.GetLocation());
	}
}

FMotionBlendSpace::FMotionBlendSpace()
	: FMotionAnimAsset(),
	BlendSpace(nullptr),
	SampleSpacing(0.1f, 0.1f)
{
	MotionAnimAssetType = EMotionAnimAssetType::BlendSpace;
}

FMotionBlendSpace::FMotionBlendSpace(const FMotionBlendSpace* Copy)
	: FMotionAnimAsset(Copy),
	BlendSpace(nullptr),
	SampleSpacing(0.1f, 0.1f)
{
	if(!Copy)
	{
		return;
	}

	BlendSpace = Copy->BlendSpace;
	SampleSpacing = Copy->SampleSpacing;
}

FMotionBlendSpace::FMotionBlendSpace(UBlendSpace* InBlendSpace, UMotionDataAsset* InParentMotionData)
	: FMotionAnimAsset(InBlendSpace, InParentMotionData),
	BlendSpace(InBlendSpace),
	SampleSpacing(0.1f, 0.1f)
{
	MotionAnimAssetType = EMotionAnimAssetType::BlendSpace;
}

FMotionBlendSpace::~FMotionBlendSpace()
{
}

double FMotionBlendSpace::GetPlayLength() const
{
	if(!BlendSpace /*|| BlendSpace->GetNumberOfBlendSamples() == 0*/)
	{
		return 0.0f;
	}

	if(UAnimSequence* Anim = BlendSpace->GetBlendSample(0).Animation)
	{
		return Anim->GetPlayLength();
	}

	return 0.0f;
}

double FMotionBlendSpace::GetFrameRate() const
{
	return BlendSpace ? 30.0 : 30.0f; //Todo: Extract frame rate from the highest weight sample?
}

FMotionComposite::FMotionComposite()
	: FMotionAnimAsset(),
	 AnimComposite(nullptr)
{
	MotionAnimAssetType = EMotionAnimAssetType::Composite;
}

FMotionComposite::FMotionComposite(const FMotionComposite* Copy)
	: FMotionAnimAsset(Copy),
	AnimComposite(nullptr)
{
	if(!Copy)
	{
		return;
	}

	AnimComposite = Copy->AnimComposite;
}

FMotionComposite::FMotionComposite(class UAnimComposite* InComposite, UMotionDataAsset* InParentMotionData)
	: FMotionAnimAsset(InComposite, InParentMotionData),
	AnimComposite(InComposite)
{
	MotionAnimAssetType = EMotionAnimAssetType::Composite;
}

FMotionComposite::~FMotionComposite()
{
}

double FMotionComposite::GetPlayLength() const
{
	return AnimComposite ? AnimComposite->GetPlayLength() : 0.0;
}

double FMotionComposite::GetFrameRate() const
{
	return AnimComposite ? 30.0 : 30.0;
}

void FMotionComposite::GetRootBoneTransform(FTransform& OutTransform, const float Time, const bool bMirrored) const
{
	if (!AnimComposite)
	{
		OutTransform = FTransform::Identity;
		return;
	}

	float RemainingTime = Time;
	for (int32 i = 0; i < AnimComposite->AnimationTrack.AnimSegments.Num(); ++i)
	{
		UAnimSequence* AnimSequence = Cast<UAnimSequence>(AnimComposite->AnimationTrack.AnimSegments[i].GetAnimReference());
		
		if (!AnimSequence)
		{
			break;
		}

		FTransform LocalBoneTransform = FTransform::Identity;
		const float SequenceLength = AnimSequence->GetPlayLength();
		if (SequenceLength >= RemainingTime)
		{
			AnimSequence->GetBoneTransform(LocalBoneTransform, FSkeletonPoseBoneIndex(0), RemainingTime, false);
			OutTransform = LocalBoneTransform * OutTransform;
			break;
		}

		AnimSequence->GetBoneTransform(LocalBoneTransform, FSkeletonPoseBoneIndex(0), SequenceLength, false);
		OutTransform = LocalBoneTransform * OutTransform;
		RemainingTime -= SequenceLength;
	}

	if(bMirrored)
	{
		OutTransform.Mirror(EAxis::X, EAxis::X);
	}	
}

void FMotionComposite::CacheTrajectoryPoints(TArray<FVector>& OutTrajectoryPoints, const bool bMirrored) const
{
	FTransform CumulativeTransform = FTransform::Identity;
	for (int32 i = 0; i < AnimComposite->AnimationTrack.AnimSegments.Num(); ++i)
	{
		UAnimSequence* AnimSequence = Cast<UAnimSequence>(AnimComposite->AnimationTrack.AnimSegments[i].GetAnimReference());
		
		FTransform LocalRootMotionTransform = FTransform::Identity;
		const float SequenceLength = AnimSequence->GetPlayLength();
		for (float Time = 0.1f; Time <= SequenceLength; Time += 0.1f)
		{
			LocalRootMotionTransform = AnimSequence->ExtractRootMotion(0.0f, Time, false);
			if(bMirrored)
			{
				LocalRootMotionTransform.Mirror(EAxis::X, EAxis::X);
			}
			
			LocalRootMotionTransform = LocalRootMotionTransform * CumulativeTransform;
			OutTrajectoryPoints.Add(LocalRootMotionTransform.GetLocation());
		}

		FTransform ThisAnimFullRootMotion = AnimSequence->ExtractRootMotion(0.0f, SequenceLength, false);
		if(bMirrored)
		{
			ThisAnimFullRootMotion.Mirror(EAxis::X, EAxis::X);
		}

		CumulativeTransform = ThisAnimFullRootMotion * CumulativeTransform;
	}
}

#undef LOCTEXT_NAMESPACE