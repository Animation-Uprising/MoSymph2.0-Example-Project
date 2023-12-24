//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AnimDecouplerComponent.generated.h"

class USkeletalMeshComponent;
class UAnimInstance;
class AActor;

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class MOTIONSYMPHONY_API UAnimDecouplerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = Settings)
	FName YawCurveName;

	UPROPERTY(EditAnywhere, Category = Settings)
	bool bNegateCurve;
	
protected:
	USkeletalMeshComponent* CharacterMesh;
	UAnimInstance* AnimInstance;
	AActor* OwnerActor;
	
	float LastWorldActorYaw;
	float LastWorldMeshYaw;
	
public:	
	// Sets default values for this component's properties
	UAnimDecouplerComponent();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:	
	void RealignMesh();

	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
};