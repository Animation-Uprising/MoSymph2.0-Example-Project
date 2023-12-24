//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#include "Objects/MatchFeatures/MatchFeature_Distance.h"
#include "DistanceMatching.h"
#include "MMPreProcessUtils.h"
#include "Objects/Tags/Tag_DistanceMarker.h"
#include "Animation/AnimComposite.h"
#include "MotionAnimAsset.h"
#include "MotionAnimObject.h"

#if WITH_EDITOR
#include "Animation/DebugSkelMeshComponent.h"
#endif

UMatchFeature_Distance::UMatchFeature_Distance()
	: DistanceMatchTrigger(EDistanceMatchTrigger::None),
	DistanceMatchType(EDistanceMatchType::None),
	DistanceMatchBasis(EDistanceMatchBasis::Positional)
{
	
}

bool UMatchFeature_Distance::IsSetupValid() const
{
	bool bIsValid = true;
	
	if(DistanceMatchTrigger == EDistanceMatchTrigger::None)
	{
		UE_LOG(LogTemp, Error, TEXT("Motion Match Config validity check failed. Distance Match Feature trigger parameter set to 'None'."))
		bIsValid = false;
	}

	if(DistanceMatchType == EDistanceMatchType::None)
	{
		UE_LOG(LogTemp, Error, TEXT("Motion Match Config validity check failed. Distance Match Feature type parameter set to 'None'."))
		bIsValid = false;
	}

	return bIsValid;
}

int32 UMatchFeature_Distance::Size() const
{
	return 1;
}

void UMatchFeature_Distance::EvaluatePreProcess(float* ResultLocation, UAnimSequence* InSequence,
                                                const float Time, const float PoseInterval, const bool bMirror, UMirrorDataTable* MirrorDataTable, ::TObjectPtr<
                                                UMotionAnimObject> InMotionObject)
{
	if(!InSequence)
	{
		*ResultLocation = 0.0f;
		return;
	}

	//Extract User Data
	TObjectPtr<UMotionAnimObject> MotionAnimAsset = nullptr;
	if(InMotionObject)
	{
		MotionAnimAsset = static_cast<UMotionAnimObject*>(InMotionObject);
	}

	if(!MotionAnimAsset)
	{
		*ResultLocation = 0.0f;
        return;
	}
	
	for(FAnimNotifyEvent& NotifyEvent : MotionAnimAsset->Tags)
	{
		if(const UTag_DistanceMarker* Tag = Cast<UTag_DistanceMarker>(NotifyEvent.Notify))
		{
			if(DoesTagMatch(Tag))
			{
				const float TagTime = NotifyEvent.GetTriggerTime();

				switch(DistanceMatchType)
				{
				case EDistanceMatchType::Backward: 
					{
						if(Time > TagTime && Time < TagTime + Tag->Tail)
						{
							*ResultLocation = -ExtractSqrDistanceToMarker(InSequence->ExtractRootMotion(
								TagTime, Time - TagTime, false));

							return;
						}
					} break;
				case EDistanceMatchType::Forward:
					{
						if(Time > TagTime - Tag->Lead && Time < TagTime)
						{
							*ResultLocation = ExtractSqrDistanceToMarker(InSequence->ExtractRootMotion(
								Time, TagTime-Time, false));

							return;
						}
					} break;
				case EDistanceMatchType::Both:
					{
						if(Time > TagTime - Tag->Lead && Time < TagTime) //Forward
						{
							*ResultLocation = ExtractSqrDistanceToMarker(InSequence->ExtractRootMotion(
								Time, TagTime-Time, false));

							return;
						}
						
						if(Time > TagTime && Time < TagTime + Tag->Tail) //Backward
						{
							*ResultLocation = -ExtractSqrDistanceToMarker(InSequence->ExtractRootMotion(
								TagTime, Time - TagTime, false));

							return;
						}
					}break;
				
				default: ;
				}
			}
		}
	}

	*ResultLocation = 0.0f;
}

void UMatchFeature_Distance::EvaluatePreProcess(float* ResultLocation, UAnimComposite* InComposite, const float Time,
                                                const float PoseInterval, const bool bMirror, UMirrorDataTable* MirrorDataTable, TObjectPtr<UMotionAnimObject>
                                                InAnimObject)
{
	if(!InComposite)
	{
		*ResultLocation = 0.0f;
		return;
	}

	//Extract User Data
	TObjectPtr<UMotionAnimObject> MotionAnimAsset = nullptr;
	if(InAnimObject)
	{
		MotionAnimAsset = static_cast<UMotionAnimObject*>(InAnimObject);
	}

	if(!MotionAnimAsset)
	{
		*ResultLocation = 0.0f;
		return;
	}
	
	for(FAnimNotifyEvent& NotifyEvent : MotionAnimAsset->Tags)
	{
		if(const UTag_DistanceMarker* Tag = Cast<UTag_DistanceMarker>(NotifyEvent.Notify))
		{
			if(DoesTagMatch(Tag))
			{
				const float TagTime = NotifyEvent.GetTriggerTime();

				switch(DistanceMatchType)
				{
				case EDistanceMatchType::Backward: 
					{
						if(Time > TagTime && Time < TagTime + Tag->Tail)
						{
							*ResultLocation = -ExtractSqrDistanceToMarker(ExtractCompositeRootMotion(
								InComposite,TagTime, Time - TagTime));

							return;
						}
					} break;
				case EDistanceMatchType::Forward:
					{
						if(Time > TagTime - Tag->Lead && Time < TagTime)
						{
							*ResultLocation = ExtractSqrDistanceToMarker(ExtractCompositeRootMotion(
								InComposite,Time, TagTime-Time));

							return;
						}
					} break;
				case EDistanceMatchType::Both:
					{
						if(Time > TagTime - Tag->Lead && Time < TagTime) //Forward
						{
							*ResultLocation = ExtractSqrDistanceToMarker(ExtractCompositeRootMotion(
								InComposite,Time, TagTime-Time));

							return;
						}

						if(Time > TagTime && Time < TagTime + Tag->Tail) //Backward
						{
							*ResultLocation = -ExtractSqrDistanceToMarker(ExtractCompositeRootMotion(
								InComposite,TagTime, Time - TagTime));

							return;
						}
					}break;
				
				default: ;
				}
			}
		}
	}
	
	*ResultLocation = 0.0f;
}

void UMatchFeature_Distance::EvaluatePreProcess(float* ResultLocation, UBlendSpace* InBlendSpace,
                                                const float Time, const float PoseInterval, const bool bMirror, UMirrorDataTable* MirrorDataTable,
                                                const FVector2D BlendSpacePosition, TObjectPtr<UMotionAnimObject> InAnimObject)
{
	if(!InBlendSpace)
	{
		*ResultLocation = 0.0f;
		return;
	}

	//Extract User Data
	TObjectPtr<UMotionAnimObject> MotionAnimAsset = nullptr;
	if(InAnimObject)
	{
		MotionAnimAsset = static_cast<UMotionAnimObject*>(InAnimObject);
	}

	if(!MotionAnimAsset)
	{
		*ResultLocation = 0.0f;
		return;
	}
	
	for(FAnimNotifyEvent& NotifyEvent : MotionAnimAsset->Tags)
	{
		if(const UTag_DistanceMarker* Tag = Cast<UTag_DistanceMarker>(NotifyEvent.Notify))
		{
			if(DoesTagMatch(Tag))
			{
				const float TagTime = NotifyEvent.GetTriggerTime();

				switch(DistanceMatchType)
				{
				case EDistanceMatchType::Backward: 
					{
						if(Time > TagTime && Time < TagTime + Tag->Tail)
						{
							*ResultLocation = -ExtractSqrDistanceToMarker(ExtractBlendSpaceRootMotion(
								InBlendSpace, BlendSpacePosition, TagTime, Time - TagTime));

							return;
						}
					} break;
				case EDistanceMatchType::Forward:
					{
						if(Time > TagTime - Tag->Lead && Time < TagTime)
						{
							*ResultLocation = ExtractSqrDistanceToMarker(ExtractBlendSpaceRootMotion(
								InBlendSpace, BlendSpacePosition, Time, TagTime - Time));

							return;
						}
					} break;
				case EDistanceMatchType::Both:
					{
						if(Time > TagTime - Tag->Lead && Time < TagTime) //Forward
						{
							*ResultLocation = ExtractSqrDistanceToMarker(ExtractBlendSpaceRootMotion(
								InBlendSpace, BlendSpacePosition, Time, TagTime - Time));

							return;
						}
						
						if(Time > TagTime && Time < TagTime + Tag->Tail) //Backward
						{
							*ResultLocation = -ExtractSqrDistanceToMarker(ExtractBlendSpaceRootMotion(
								InBlendSpace, BlendSpacePosition, TagTime, Time - TagTime));

							return;
						}
					}break;
				
				default: ;
				}
			}
		}
	}

	*ResultLocation = 0.0f;
}

void UMatchFeature_Distance::SourceInputData(TArray<float>& OutFeatureArray, const int32 FeatureOffset, AActor* InActor)
{
	if(!InActor)
	{
		UMatchFeatureBase::SourceInputData(OutFeatureArray, FeatureOffset, nullptr);
		return;
	}

	if(FeatureOffset >= OutFeatureArray.Num())
	{
		UE_LOG(LogTemp, Error, TEXT("UMatchFeature_Distance: SourceInputData(...) - Feature does not fit in FeatureArray"));
		return;
	}

	if(UDistanceMatching* DistanceMatching = InActor->GetComponentByClass<UDistanceMatching>())
	{
		if(DistanceMatching->DoesCurrentStateMatchFeature(this))
		{
			const float Distance = DistanceMatching->GetMarkerDistance();
			const float DistanceSqr = Distance * Distance;
			OutFeatureArray[FeatureOffset] = Distance < 0.0f ? -1.0f * DistanceSqr : DistanceSqr;
		}
		else
		{
			OutFeatureArray[FeatureOffset] = 0.0f;
		}
	}
}

bool UMatchFeature_Distance::NextPoseToleranceTest(const TArray<float>& DesiredInputArray,
                                                   const TArray<float>& PoseMatrix, const int32 MatrixStartIndex, const int32 FeatureOffset,
                                                   const float PositionTolerance, const float RotationTolerance)
{
	return Super::NextPoseToleranceTest(DesiredInputArray, PoseMatrix, MatrixStartIndex, FeatureOffset,
	                                    PositionTolerance,
	                                    RotationTolerance);
}

bool UMatchFeature_Distance::CanBeResponseFeature() const
{
	return true;
}

#if WITH_EDITOR
void UMatchFeature_Distance::DrawPoseDebugEditor(UMotionDataAsset* MotionData,
	UDebugSkelMeshComponent* DebugSkeletalMesh, const int32 PreviewIndex, const int32 FeatureOffset,
	const UWorld* World, FPrimitiveDrawInterface* DrawInterface)
{
	if(!MotionData
		|| !DebugSkeletalMesh
		|| !World)
	{
		return;
	}

	TArray<float>& PoseArray = MotionData->LookupPoseMatrix.PoseArray;
	const int32 StartIndex = PreviewIndex * MotionData->LookupPoseMatrix.AtomCount + FeatureOffset;

	if(PoseArray.Num() < StartIndex + Size())
	{
		return;
	}

	const float DistanceSqr = PoseArray[StartIndex];
	const float Distance = DistanceSqr < 0.0f ? (-1.0f * FMath::Sqrt(FMath::Abs(DistanceSqr))) : FMath::Sqrt(DistanceSqr);
	if(FMath::Abs(Distance) < 0.001f)
	{
		return;
	}
	
	const FTransform PreviewTransform = DebugSkeletalMesh->GetComponentTransform();

	FVector DistancePoint = PreviewTransform.TransformPosition(FVector(Distance, 0.0f, 0.0f));
	if(MotionData->MotionMatchConfig->ForwardAxis == EAllAxis::Y)
	{
		DistancePoint = PreviewTransform.TransformPosition(FVector(0.0f, Distance, 0.0f));
	}
	
	
	FColor DebugColor = FColor::Red;

	switch(DistanceMatchType)
	{
		case EDistanceMatchType::Forward: DebugColor = FColor::Green; break;
		case EDistanceMatchType::Backward: DebugColor = FColor::Blue; break;
		case EDistanceMatchType::Both: DebugColor = FColor::Purple; break;
		default: break;
	}

	DrawDebugBox(World, DistancePoint, FVector(30.0f), PreviewTransform.GetRotation(),
		DebugColor, true, -1, -1);

	DrawInterface->DrawLine(PreviewTransform.GetLocation(), DistancePoint, DebugColor, SDPG_Foreground, 2.0f);
}
#endif

bool UMatchFeature_Distance::DoesTagMatch(const UTag_DistanceMarker* InTag) const
{
	return InTag->DistanceMatchTrigger == DistanceMatchTrigger
		&& InTag->DistanceMatchType == DistanceMatchType
		&& InTag->DistanceMatchBasis == DistanceMatchBasis;
}

float UMatchFeature_Distance::ExtractSqrDistanceToMarker(const FTransform& InTagTransform) const
{
	if(DistanceMatchBasis == EDistanceMatchBasis::Positional) //Positional
	{
		const FVector TagLocation = InTagTransform.GetLocation();
		return TagLocation.SquaredLength();
	}
	else //Rotational
	{
		const FRotator TagRotation = InTagTransform.GetRotation().GetNormalized().Rotator();
		return TagRotation.Yaw;
	}
}

FTransform UMatchFeature_Distance::ExtractBlendSpaceRootMotion(const UBlendSpace* InBlendSpace,
	const FVector2D InBlendSpacePosition, const float InBaseTime, const float InDeltaTime)
{
	if(!InBlendSpace)
	{
		return FTransform();
	}
	
	TArray<FBlendSampleData> SampleDataList;
	int32 CachedTriangulationIndex = -1;
	InBlendSpace->GetSamplesFromBlendInput(FVector(InBlendSpacePosition.X, InBlendSpacePosition.Y, 0.0f),
		SampleDataList, CachedTriangulationIndex, false);

	FRootMotionMovementParams RootMotionParams;
	RootMotionParams.Clear();
	
	FMMPreProcessUtils::ExtractRootMotionParams(RootMotionParams, SampleDataList, InBaseTime, InDeltaTime, false);
	
	return RootMotionParams.GetRootMotionTransform();
}

FTransform UMatchFeature_Distance::ExtractCompositeRootMotion(const UAnimComposite* InAnimComposite,
	const float InBaseTime, const float InDeltaTime)
{
	if(!InAnimComposite)
	{
		return FTransform();
	}

	FRootMotionMovementParams RootMotionParams;
	RootMotionParams.Clear();
	InAnimComposite->ExtractRootMotionFromTrack(InAnimComposite->AnimationTrack, InBaseTime, InBaseTime + InDeltaTime, RootMotionParams);

	return RootMotionParams.GetRootMotionTransform();
}


