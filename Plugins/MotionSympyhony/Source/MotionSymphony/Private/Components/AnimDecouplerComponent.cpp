//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#include "Components/AnimDecouplerComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "GameFramework/Actor.h"

// Sets default values for this component's properties
UAnimDecouplerComponent::UAnimDecouplerComponent()
	: bNegateCurve(false),
	CharacterMesh(nullptr),
	OwnerActor(nullptr),
	LastWorldActorYaw(0.0f),
	LastWorldMeshYaw(0.0f)
{
	PrimaryComponentTick.bCanEverTick = true;
}


// Called when the game starts	
void UAnimDecouplerComponent::BeginPlay()
{
	Super::BeginPlay();

	OwnerActor = GetOwner();

	CharacterMesh = Cast<USkeletalMeshComponent>(
		OwnerActor->GetComponentByClass(USkeletalMeshComponent::StaticClass()));

	AnimInstance = CharacterMesh->GetAnimInstance();
	
	if(!CharacterMesh)
	{
		UE_LOG(LogTemp, Error, TEXT("Using an Anim Decoupler Component but it could not find a mesh component. The component will not function."))
	}

	LastWorldActorYaw = OwnerActor->GetActorRotation().Yaw;
}

void UAnimDecouplerComponent::RealignMesh()
{
	
}


// Called every frame
void UAnimDecouplerComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if(!AnimInstance || !CharacterMesh)
	{
		UE_LOG(LogTemp, Error, TEXT("AnimInstance or CharacterMesh is null. cannot tick AnimDecouplerComponent"));
		return;
	}

	const float ActorYaw = OwnerActor->GetActorRotation().Yaw;
	const float Delta = ActorYaw - LastWorldActorYaw;

	const float ModelDelta = (AnimInstance->GetCurveValue(YawCurveName) * DeltaTime * (bNegateCurve ? -1.0f : 1.0f)) - Delta;

	CharacterMesh->AddWorldRotation(FRotator(0.0f, ModelDelta, 0.0f));
	
	LastWorldActorYaw = ActorYaw;
}
