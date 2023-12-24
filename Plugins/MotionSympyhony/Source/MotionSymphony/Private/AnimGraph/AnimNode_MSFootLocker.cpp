//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#include "AnimNode_MSFootLocker.h"
#include "Animation/AnimInstanceProxy.h"
#include "DrawDebugHelpers.h"
#include "MSFootLockerMath.h"

static TAutoConsoleVariable<int32> CVarFootLockerDebug(
	TEXT("a.AnimNode.FootLocker.Debug"),
	0,
	TEXT("Turns stride warp debugging on or off.\n")
	TEXT("<=0: off \n")
	TEXT("  1: minimal (Stride Pivot Only)\n")
	TEXT("  2: full\n"),
	ECVF_SetByConsole);

static TAutoConsoleVariable<int32> CVarFootLockerEnabled(
	TEXT("a.AnimNode.FootLocker.Enable"),
	1,
	TEXT("Turns stride warp nodes on or off. \n")
	TEXT("<=0: off \n")
	TEXT("  1: on \n"),
	ECVF_SetByConsole);

FAnimNode_MSFootLocker::FAnimNode_MSFootLocker()
	: bLeftFootLock(false),
	bRightFootLock(false),
	LegHyperExtensionFixMethod(EMSHyperExtensionFixMethod::MoveFootTowardsThigh),
	AllowLegExtensionRatio(0.0f),
	LockReleaseSmoothTime(-1.0f),
	DeltaTime(0.0f),
	bValidCheckResult(false),
	LeftFootLockWeight(0.0f),
	RightFootLockWeight(0.0f),
	bLeftFootLock_LastFrame(false),
	bRightFootLock_LastFrame(false),
	LeftLockLocation_WS(FVector::ZeroVector),
	RightLockLocation_WS(FVector::ZeroVector),
	AnimInstanceProxy(nullptr)
{
}

void FAnimNode_MSFootLocker::EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& Output,
                                                             TArray<FBoneTransform>& OutBoneTransforms)
{
	check(OutBoneTransforms.Num() == 0);
	
	const int32 DebugLevel = CVarFootLockerDebug.GetValueOnAnyThread();
	const FTransform& ComponentTransform_WS = AnimInstanceProxy->GetSkelMeshComponent()->GetComponentTransform();
	EvaluateLimb(OutBoneTransforms, Output, ComponentTransform_WS, LeftFootDefinition, LeftLockLocation_WS, LeftFootLockWeight, bLeftFootLock, bLeftFootLock_LastFrame, DebugLevel);
	EvaluateLimb(OutBoneTransforms, Output, ComponentTransform_WS, RightFootDefinition, RightLockLocation_WS, RightFootLockWeight, bRightFootLock, bRightFootLock_LastFrame, DebugLevel);
	
	if(OutBoneTransforms.Num() > 0)
	{
		OutBoneTransforms.Sort(FCompareBoneTransformIndex());

		if (Alpha < 1.0f)
		{
			Output.Pose.LocalBlendCSBoneTransforms(OutBoneTransforms, Alpha);
		}
	}
}

void FAnimNode_MSFootLocker::EvaluateFootLocking(FComponentSpacePoseContext& Output, 
	TArray<FBoneTransform>& OutBoneTransforms)
{
	const FTransform& ComponentTransform_WS = AnimInstanceProxy->GetSkelMeshComponent()->GetComponentTransform();
	
	/*******************************************************************************************************************/
	/********************************************** Evaluate the left foot *********************************************/
	/*******************************************************************************************************************/
	FTransform LeftFootTransform_CS = Output.Pose.GetComponentSpaceTransform(LeftFootDefinition.FootBone.CachedCompactPoseIndex);
	const FVector LeftToeLocation_CS = Output.Pose.GetComponentSpaceTransform(LeftFootDefinition.ToeBone.CachedCompactPoseIndex).GetLocation();

#if WITH_EDITOR && ENABLE_ANIM_DEBUG
	const int32 DebugLevel = CVarFootLockerDebug.GetValueOnAnyThread();
	if(DebugLevel > 0)
	{
		const FVector LeftFootLocation_WS = ComponentTransform_WS.TransformPosition(LeftFootTransform_CS.GetLocation());
		AnimInstanceProxy->AnimDrawDebugSphere(LeftFootLocation_WS, 10.0f, 8, FColor::Blue, false, -1.0f, 0.0f);
	}
#endif
	
	if(bLeftFootLock)
	{
		if(!bLeftFootLock_LastFrame)
		{
			//Record the left toe position in world space if it has just been locked
			LeftLockLocation_WS = ComponentTransform_WS.TransformPosition(LeftToeLocation_CS);
		}

		//When a lock starts, the weight is instantly set to 1.0f
		LeftFootLockWeight = 1.0f;
	}
	else
	{
		if(LockReleaseSmoothTime < 0.0001f)
		{
			//Without lock smoothing the weight is set to 0.0f immediately upon the lock being released
			LeftFootLockWeight = 0.0f;
		}
		else
		{
			//Smoothly release the lock weight once the lock has been released.
			LeftFootLockWeight = FMath::Clamp(LeftFootLockWeight - (1.0f/LockReleaseSmoothTime) * DeltaTime, 0.0f, 1.0f);
		}
	}

	//As long as there is weight on the lock, foot locking needs to be evaluated
	if(LeftFootLockWeight > 0.0001f)
	{
		const FVector LeftFootLocation_CS = LeftFootTransform_CS.GetLocation();

		FVector LeftLockLocation_CS = ComponentTransform_WS.InverseTransformPosition(LeftLockLocation_WS)
			+ (LeftFootLocation_CS - LeftToeLocation_CS);

		FVector WeightedLeftLockPosition_CS = FMath::Lerp(LeftFootLocation_CS, LeftLockLocation_CS, LeftFootLockWeight);

		if(LegHyperExtensionFixMethod != EMSHyperExtensionFixMethod::None)
		{
			//Calculate the left leg length if it has not already been done
			if(LeftFootDefinition.Length < 0.0f)
			{
				LeftFootDefinition.CalculateLength(Output.Pose);
			}

			//Determine whether the leg is hyper-extended or not.
			const FTransform LeftThighTransform_CS = Output.Pose.GetComponentSpaceTransform(LeftFootDefinition.Bones.Last().CachedCompactPoseIndex);
			const FVector LeftThighLocation_CS = LeftThighTransform_CS.GetLocation();
		
			const float StartingLegLength = FVector::Distance(LeftThighLocation_CS, LeftFootLocation_CS);
			const float MaxAllowableLegLength = StartingLegLength + ((LeftFootDefinition.Length  - StartingLegLength) * AllowLegExtensionRatio);
			const float LockedLegLength = FVector::Distance(LeftThighLocation_CS, LeftLockLocation_CS);

			//Adjust the legs to avoid hyper-extension
			if(LockedLegLength > MaxAllowableLegLength)
			{
				float ThighToLockDist_XY = FVector2D::Distance(FVector2D(LeftThighLocation_CS.X, LeftThighLocation_CS.Y), FVector2D(LeftLockLocation_CS.X, LeftLockLocation_CS.Y));
			
				switch(LegHyperExtensionFixMethod)
				{
				case EMSHyperExtensionFixMethod::None: break;

				case EMSHyperExtensionFixMethod::MoveFootTowardsThigh:
					{
						FVector ThighToLockVector_CS = LeftLockLocation_CS - LeftThighLocation_CS;
						LeftLockLocation_CS = LeftThighLocation_CS + (ThighToLockVector_CS.GetSafeNormal() * MaxAllowableLegLength);
					} break;
			
				case EMSHyperExtensionFixMethod::MoveFootUnderThigh:
					{
						const FVector FootShiftVector = LeftLockLocation_CS - FVector(LeftThighLocation_CS.X, LeftThighLocation_CS.Y, LeftLockLocation_CS.Z);
						const float ThighHeight = LeftThighLocation_CS.Z - LeftLockLocation_CS.Z/* - DesiredHipAdjust*/;

						if(FMath::IsNearlyZero(FootShiftVector.SizeSquared())) //Edge case where the foot is directly below thigh joint.
						{
							LeftLockLocation_CS = WeightedLeftLockPosition_CS;
						}
						else if(FMath::Abs(MaxAllowableLegLength - ThighHeight) > 0.01f)
						{
							ThighToLockDist_XY = FMath::Sqrt((MaxAllowableLegLength * MaxAllowableLegLength) - (ThighHeight * ThighHeight));
							LeftLockLocation_CS = FVector(LeftThighLocation_CS.X, LeftThighLocation_CS.Y, LeftLockLocation_CS.Z) + (FootShiftVector.GetSafeNormal() * ThighToLockDist_XY);
						}
						else
						{
							ThighToLockDist_XY = 0.0f;
							LeftLockLocation_CS = FVector(LeftThighLocation_CS.X, LeftThighLocation_CS.Y, LeftLockLocation_CS.Z) + (FootShiftVector.GetSafeNormal() * ThighToLockDist_XY);
						}
						
					} break;
				default: break;
				}
				
				WeightedLeftLockPosition_CS = FMath::Lerp(LeftFootLocation_CS, LeftLockLocation_CS, LeftFootLockWeight);
			}
		}

		if(!WeightedLeftLockPosition_CS.ContainsNaN()) //Check for safety
		{
			//Set the location of the left foot Transform to the WS lock position but with the toe offset and weighted lerp
			LeftFootTransform_CS.SetLocation(WeightedLeftLockPosition_CS);
		
			//Perform foot lock with IK target
			OutBoneTransforms.Add(FBoneTransform(LeftFootDefinition.IkTarget.CachedCompactPoseIndex, LeftFootTransform_CS));
		}
	}
	
	bLeftFootLock_LastFrame = bLeftFootLock;
	
#if WITH_EDITOR && ENABLE_ANIM_DEBUG
	if(DebugLevel > 0)
	{
		//Print Debug Title

		if(bLeftFootLock)
		{
			const FString LockDebug = FString::Printf(TEXT("Left Foot is Locked with weight: %f"), LeftFootLockWeight);
			AnimInstanceProxy->AnimDrawDebugOnScreenMessage(LockDebug, FColor::Green);
		}
		else
		{
			const FString LockDebug = FString::Printf(TEXT("Left Foot is Un-Locked with weight: %f"), LeftFootLockWeight);
			AnimInstanceProxy->AnimDrawDebugOnScreenMessage(LockDebug, FColor::Red);
		}

		const FVector LeftFootFinalLocation_WS = ComponentTransform_WS.TransformPosition(LeftFootTransform_CS.GetLocation());

		const FString OriginalPositionDebug = FString::Printf(TEXT("Original Position: X: %04f, Y: %04f, Z: %04f"), 0.0f, 0.0f, 0.0f);
		const FString LockPositionDebug = FString::Printf(TEXT("Lock Position: X: %04f, Y: %04f, Z: %04f"), LeftLockLocation_WS.X, LeftLockLocation_WS.Y, LeftLockLocation_WS.Z);
		const FString FinalPositionDebug = FString::Printf(TEXT("Final Position: X: %04f, Y: %04f, Z: %04f"), LeftFootFinalLocation_WS.X, LeftFootFinalLocation_WS.Y, LeftFootFinalLocation_WS.Z);
		
		AnimInstanceProxy->AnimDrawDebugOnScreenMessage(OriginalPositionDebug, FColor::Black);
		AnimInstanceProxy->AnimDrawDebugOnScreenMessage(LockPositionDebug, FColor::Black);
		AnimInstanceProxy->AnimDrawDebugOnScreenMessage(FinalPositionDebug, FColor::Black);

		if(bLeftFootLock)
		{
			AnimInstanceProxy->AnimDrawDebugSphere(LeftLockLocation_WS, 10.0f, 8, FColor::Yellow, false, -1.0f, 0.0f);
		}
		else
		{
			AnimInstanceProxy->AnimDrawDebugSphere(LeftLockLocation_WS, 10.0f, 8, FColor::Orange, false, -1.0f, 0.0f);
		}
		
		AnimInstanceProxy->AnimDrawDebugSphere(LeftFootFinalLocation_WS, 10.0f, 8, FColor::Green, false, -1.0f, 0.0f);
	}
#endif


	/*******************************************************************************************************************/
	/********************************************** Evaluate the right foot ********************************************/
	/*******************************************************************************************************************/
	FTransform RightFootTransform_CS = Output.Pose.GetComponentSpaceTransform(RightFootDefinition.FootBone.CachedCompactPoseIndex);
	const FVector RightToeLocation_CS = Output.Pose.GetComponentSpaceTransform(RightFootDefinition.ToeBone.CachedCompactPoseIndex).GetLocation();

#if WITH_EDITOR && ENABLE_ANIM_DEBUG
	if(DebugLevel > 0)
	{
		const FVector RightFootLocation_WS = ComponentTransform_WS.TransformPosition(RightFootTransform_CS.GetLocation());
		AnimInstanceProxy->AnimDrawDebugSphere(RightFootLocation_WS, 10.0f, 8, FColor::Blue, false, -1.0f, 0.0f);
	}
#endif
	
	if(bRightFootLock)
	{
		if(!bRightFootLock_LastFrame)
		{
			//Record the toe position in world space if it has just been locked
			RightLockLocation_WS = ComponentTransform_WS.TransformPosition(RightToeLocation_CS);
		}

		//When a lock starts, the weight is instantly set to 1.0f
		RightFootLockWeight = 1.0f;
	}
	else
	{
		if(LockReleaseSmoothTime < 0.0001f)
		{
			//Without lock smoothing the weight is set to 0.0f immediately upon the lock being released
			RightFootLockWeight = 0.0f;
		}
		else
		{
			//Smoothly release the lock weight once the lock has been released.
			RightFootLockWeight = FMath::Clamp(RightFootLockWeight - (1.0f/LockReleaseSmoothTime) * DeltaTime, 0.0f, 1.0f);
		}
	}

	//As long as there is weight on the lock, foot locking needs to be evaluated
	if(RightFootLockWeight > 0.0001f)
	{
		const FVector RightFootLocation_CS = RightFootTransform_CS.GetLocation();

		FVector RightLockLocation_CS = ComponentTransform_WS.InverseTransformPosition(RightLockLocation_WS)
			+ (RightFootLocation_CS - RightToeLocation_CS);

		FVector WeightedRightLockPosition_CS = FMath::Lerp(RightFootLocation_CS, RightLockLocation_CS, RightFootLockWeight);


		if(LegHyperExtensionFixMethod != EMSHyperExtensionFixMethod::None)
		{
			//Calculate the leg length if it has not already been done
			if(RightFootDefinition.Length < 0.0f)
			{
				RightFootDefinition.CalculateLength(Output.Pose);
			}

			//Determine whether the leg is hyper-extended or not.
			const FTransform RightThighTransform_CS = Output.Pose.GetComponentSpaceTransform(RightFootDefinition.Bones.Last().CachedCompactPoseIndex);
			const FVector RightThighLocation_CS = RightThighTransform_CS.GetLocation();
		
			const float StartingLegLength = FVector::Distance(RightThighLocation_CS, RightFootLocation_CS);
			const float MaxAllowableLegLength = StartingLegLength + ((RightFootDefinition.Length  - StartingLegLength) * AllowLegExtensionRatio);
			const float LockedLegLength = FVector::Distance(RightThighLocation_CS, RightLockLocation_CS);

			//Adjust the legs to avoid hyper-extension
			if(LockedLegLength > MaxAllowableLegLength)
			{
				float ThighToLockDist_XY = FVector2D::Distance(FVector2D(RightThighLocation_CS.X, RightThighLocation_CS.Y), FVector2D(RightLockLocation_CS.X, RightLockLocation_CS.Y));
			
				switch(LegHyperExtensionFixMethod)
				{
				default:
				case EMSHyperExtensionFixMethod::MoveFootTowardsThigh:
					{
						FVector ThighToLockVector_CS = RightLockLocation_CS - RightThighLocation_CS;
						RightLockLocation_CS = RightThighLocation_CS + (ThighToLockVector_CS.GetSafeNormal() * MaxAllowableLegLength);
					} break;
			
				case EMSHyperExtensionFixMethod::MoveFootUnderThigh:
					{
						const FVector FootShiftVector = RightLockLocation_CS - FVector(RightThighLocation_CS.X, RightThighLocation_CS.Y, RightLockLocation_CS.Z);
						const float ThighHeight = RightThighLocation_CS.Z - RightLockLocation_CS.Z;

						if(FMath::IsNearlyZero(FootShiftVector.SizeSquared())) //Edge case where the foot is directly below thigh joint.
						{
							RightLockLocation_CS = WeightedRightLockPosition_CS;
						}
						else if(FMath::Abs(MaxAllowableLegLength - ThighHeight) > 0.01f)
						{
							ThighToLockDist_XY = FMath::Sqrt((MaxAllowableLegLength * MaxAllowableLegLength) - (ThighHeight * ThighHeight));
							RightLockLocation_CS = FVector(RightThighLocation_CS.X, RightThighLocation_CS.Y, RightLockLocation_CS.Z) + (FootShiftVector.GetSafeNormal() * ThighToLockDist_XY);
						}
						else
						{
							ThighToLockDist_XY = 0.0f;
							RightLockLocation_CS = FVector(RightThighLocation_CS.X, RightThighLocation_CS.Y, RightLockLocation_CS.Z) + (FootShiftVector.GetSafeNormal() * ThighToLockDist_XY);
						}
						
					} break;
				}
				
				WeightedRightLockPosition_CS = FMath::Lerp(RightFootLocation_CS, RightLockLocation_CS, RightFootLockWeight);
			}
		}

		if(!WeightedRightLockPosition_CS.ContainsNaN()) //Check for safety
		{
			//Set the location of the foot Transform to the WS lock position but with the toe offset and weighted lerp
			RightFootTransform_CS.SetLocation(WeightedRightLockPosition_CS);
		
			//Perform foot lock with IK target
			OutBoneTransforms.Add(FBoneTransform(RightFootDefinition.IkTarget.CachedCompactPoseIndex, RightFootTransform_CS));
		}
	}
	
	bRightFootLock_LastFrame = bRightFootLock;
}

void FAnimNode_MSFootLocker::EvaluateLimb(TArray<FBoneTransform>& OutBoneTransforms, FComponentSpacePoseContext& Output,
                                        const FTransform& ComponentTransform_WS, FMSFootLockLimbDefinition& LimbDefinition, FVector& LockLocation_WS,
                                        float& FootLockWeight, bool bLockFoot, bool& bLockFoot_LastFrame, const int32 DebugLevel)
{
	FTransform FootTransform_CS = Output.Pose.GetComponentSpaceTransform(LimbDefinition.FootBone.CachedCompactPoseIndex);
	const FVector ToeLocation_CS = Output.Pose.GetComponentSpaceTransform(LimbDefinition.ToeBone.CachedCompactPoseIndex).GetLocation();

#if WITH_EDITOR && ENABLE_ANIM_DEBUG
	if(DebugLevel > 0)
	{
		const FVector FootLocation_WS = ComponentTransform_WS.TransformPosition(FootTransform_CS.GetLocation());
		AnimInstanceProxy->AnimDrawDebugSphere(FootLocation_WS, 10.0f, 8, FColor::Blue, false, -1.0f, 0.0f);
	}
#endif
	
	if(bLockFoot)
	{
		if(!bLockFoot_LastFrame)
		{
			//Record the toe position in world space if it has just been locked
			LockLocation_WS = ComponentTransform_WS.TransformPosition(ToeLocation_CS);
		}

		//When a lock starts, the weight is instantly set to 1.0f
		FootLockWeight = 1.0f;
	}
	else
	{
		if(LockReleaseSmoothTime < 0.0001f)
		{
			//Without lock smoothing the weight is set to 0.0f immediately upon the lock being released
			FootLockWeight = 0.0f;
		}
		else
		{
			//Smoothly release the lock weight once the lock has been released.
			FootLockWeight = FMath::Clamp(FootLockWeight - (1.0f/LockReleaseSmoothTime) * DeltaTime, 0.0f, 1.0f);
		}
	}

	//As long as there is weight on the lock, foot locking needs to be evaluated
	if(FootLockWeight > 0.0001f)
	{
		const FVector FootLocation_CS = FootTransform_CS.GetLocation();

		FVector LockLocation_CS = ComponentTransform_WS.InverseTransformPosition(LockLocation_WS)
			+ (FootLocation_CS - ToeLocation_CS);

		FVector WeightedLockPosition_CS = FMath::Lerp(FootLocation_CS, LockLocation_CS, FootLockWeight);

		if(LegHyperExtensionFixMethod != EMSHyperExtensionFixMethod::None)
		{
			//Calculate the leg length if it has not already been done
			if(LimbDefinition.Length < 0.0f)
			{
				LimbDefinition.CalculateLength(Output.Pose);
			}

			//Determine whether the leg is hyper-extended or not.
			const FTransform ThighTransform_CS = Output.Pose.GetComponentSpaceTransform(LimbDefinition.Bones.Last().CachedCompactPoseIndex);
			const FVector ThighLocation_CS = ThighTransform_CS.GetLocation();
		
			const float StartingLegLength = FVector::Distance(ThighLocation_CS, FootLocation_CS);
			const float MaxAllowableLegLength = StartingLegLength + ((LimbDefinition.Length  - StartingLegLength) * AllowLegExtensionRatio);
			const float LockedLegLength = FVector::Distance(ThighLocation_CS, LockLocation_CS);

			//Adjust the legs to avoid hyper-extension
			if(LockedLegLength > MaxAllowableLegLength)
			{
				float ThighToLockDist_XY = FVector2D::Distance(FVector2D(ThighLocation_CS.X, ThighLocation_CS.Y), FVector2D(LockLocation_CS.X, LockLocation_CS.Y));
			
				switch(LegHyperExtensionFixMethod)
				{
				default:
				case EMSHyperExtensionFixMethod::MoveFootTowardsThigh:
					{
						FVector ThighToLockVector_CS = LockLocation_CS - ThighLocation_CS;
						LockLocation_CS = ThighLocation_CS + (ThighToLockVector_CS.GetSafeNormal() * MaxAllowableLegLength);
					} break;
			
				case EMSHyperExtensionFixMethod::MoveFootUnderThigh:
					{
						const FVector FootShiftVector = LockLocation_CS - FVector(ThighLocation_CS.X, ThighLocation_CS.Y, LockLocation_CS.Z); //  _ horiztonal vector
						const float ThighHeight = ThighLocation_CS.Z - LockLocation_CS.Z; //  | vertical length

						if(FMath::IsNearlyZero(FootShiftVector.SizeSquared())) //Edge case where the foot is directly below thigh joint.
						{
							LockLocation_CS = WeightedLockPosition_CS;
						}
						else if(FMath::Abs(MaxAllowableLegLength - ThighHeight) > 0.01f)
						{
							ThighToLockDist_XY = FMath::Sqrt((MaxAllowableLegLength * MaxAllowableLegLength) - (ThighHeight * ThighHeight));
							LockLocation_CS = FVector(ThighLocation_CS.X, ThighLocation_CS.Y, LockLocation_CS.Z) + (FootShiftVector.GetSafeNormal() * ThighToLockDist_XY);
						}
						else
						{
							ThighToLockDist_XY = 0.0f;
							LockLocation_CS = FVector(ThighLocation_CS.X, ThighLocation_CS.Y, LockLocation_CS.Z) + (FootShiftVector.GetSafeNormal() * ThighToLockDist_XY);
						}

					} break;
				}
				
				WeightedLockPosition_CS = FMath::Lerp(FootLocation_CS, LockLocation_CS, FootLockWeight);
			}
		}

		if(!WeightedLockPosition_CS.ContainsNaN()) //Check for safety
		{
			//Set the location of the foot Transform to the WS lock position but with the toe offset and weighted lerp
			FootTransform_CS.SetLocation(WeightedLockPosition_CS);
		
			//Perform foot lock with IK target
			OutBoneTransforms.Add(FBoneTransform(LimbDefinition.IkTarget.CachedCompactPoseIndex, FootTransform_CS));
		}
	}
	
	bLockFoot_LastFrame = bLockFoot;
	
#if WITH_EDITOR && ENABLE_ANIM_DEBUG
	if(DebugLevel > 0)
	{
		//Print Debug Title

		if(bLockFoot)
		{
			const FString LockDebug = FString::Printf(TEXT("Foot is Locked with weight: %f"), FootLockWeight);
			AnimInstanceProxy->AnimDrawDebugOnScreenMessage(LockDebug, FColor::Green);
		}
		else
		{
			const FString LockDebug = FString::Printf(TEXT("Foot is Un-Locked with weight: %f"), FootLockWeight);
			AnimInstanceProxy->AnimDrawDebugOnScreenMessage(LockDebug, FColor::Red);
		}

		const FVector FootFinalLocation_WS = ComponentTransform_WS.TransformPosition(FootTransform_CS.GetLocation());

		const FString OriginalPositionDebug = FString::Printf(TEXT("Original Position: X: %04f, Y: %04f, Z: %04f"), 0.0f, 0.0f, 0.0f);
		const FString LockPositionDebug = FString::Printf(TEXT("Lock Position: X: %04f, Y: %04f, Z: %04f"), LockLocation_WS.X, LockLocation_WS.Y, LockLocation_WS.Z);
		const FString FinalPositionDebug = FString::Printf(TEXT("Final Position: X: %04f, Y: %04f, Z: %04f"), FootFinalLocation_WS.X, FootFinalLocation_WS.Y, FootFinalLocation_WS.Z);
		
		AnimInstanceProxy->AnimDrawDebugOnScreenMessage(OriginalPositionDebug, FColor::Black);
		AnimInstanceProxy->AnimDrawDebugOnScreenMessage(LockPositionDebug, FColor::Black);
		AnimInstanceProxy->AnimDrawDebugOnScreenMessage(FinalPositionDebug, FColor::Black);

		if(bLockFoot)
		{
			AnimInstanceProxy->AnimDrawDebugSphere(LockLocation_WS, 10.0f, 8, FColor::Yellow, false, -1.0f, 0.0f);
		}
		else
		{
			AnimInstanceProxy->AnimDrawDebugSphere(LockLocation_WS, 10.0f, 8, FColor::Orange, false, -1.0f, 0.0f);
		}
		
		AnimInstanceProxy->AnimDrawDebugSphere(FootFinalLocation_WS, 10.0f, 8, FColor::Green, false, -1.0f, 0.0f);
	}
#endif
}

bool FAnimNode_MSFootLocker::IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones)
{
	return CheckValidBones(RequiredBones)
		   && Alpha > UE_KINDA_SMALL_NUMBER
		   && (CVarFootLockerEnabled.GetValueOnAnyThread() == 1)
		   && IsLODEnabled(AnimInstanceProxy);
}

void FAnimNode_MSFootLocker::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	FAnimNode_SkeletalControlBase::Initialize_AnyThread(Context);
	AnimInstanceProxy = Context.AnimInstanceProxy;

	bLeftFootLock = bRightFootLock = bLeftFootLock_LastFrame = bRightFootLock_LastFrame = false;
}

void FAnimNode_MSFootLocker::CacheBones_AnyThread(const FAnimationCacheBonesContext& Context)
{
	FAnimNode_SkeletalControlBase::CacheBones_AnyThread(Context);
}

void FAnimNode_MSFootLocker::UpdateInternal(const FAnimationUpdateContext& Context)
{
	FAnimNode_SkeletalControlBase::UpdateInternal(Context);
	DeltaTime = Context.GetDeltaTime();
}

void FAnimNode_MSFootLocker::GatherDebugData(FNodeDebugData& DebugData)
{
	ComponentPose.GatherDebugData(DebugData);
}

bool FAnimNode_MSFootLocker::HasPreUpdate() const
{
	return false;
}

void FAnimNode_MSFootLocker::InitializeBoneReferences(const FBoneContainer& RequiredBones)
{
	LeftFootDefinition.Initialize(RequiredBones, AnimInstanceProxy);
	RightFootDefinition.Initialize(RequiredBones, AnimInstanceProxy);
	
	bValidCheckResult = LeftFootDefinition.IsValid(RequiredBones) && RightFootDefinition.IsValid(RequiredBones);
}

bool FAnimNode_MSFootLocker::CheckValidBones(const FBoneContainer& RequiredBones)
{
	bValidCheckResult = LeftFootDefinition.IsValid(RequiredBones)
		&& RightFootDefinition.IsValid(RequiredBones);

	return bValidCheckResult;
}