//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Enumerations/EDistanceMatchingEnums.h"
#include "DistanceMatching.generated.h"

USTRUCT(BlueprintInternalUseOnly)
struct MOTIONSYMPHONY_API FDistanceMatchingModule
{
	GENERATED_BODY()

public:
	UPROPERTY(Transient)
	UAnimSequenceBase* AnimSequence;

private:
	int32 LastKeyChecked;
	float MaxDistance;
	TArray<FRichCurveKey> CurveKeys;
	
public:
	FDistanceMatchingModule();
	void Setup(UAnimSequenceBase* InAnimSequence, const FName& DistanceCurveName);
	void Initialize();
	float FindMatchingTime(float DesiredDistance, bool bNegateCurve);

};

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class MOTIONSYMPHONY_API UDistanceMatching : public UActorComponent
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings)
	bool bAutomaticTriggers;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta = (ClampMin = 0.0f))
	float DistanceTolerance;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PlantDetection, meta = (ClampMin = 0.0f, ClampMax = 180.0f))
	float MinPlantDetectionAngle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PlantDetection, meta = (ClampMin = 0.0f))
	float MinPlantSpeed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PlantDetection, meta = (ClampMin = 0.0f))
	float MinPlantAccel;

protected:
	EDistanceMatchTrigger TriggeredTransition;
	uint32 CurrentInstanceId;
	bool bDestinationReached;
	float LastFrameAccelSqr;
	float DistanceToMarker;
	float TimeToMarker;
	FVector MarkerVector;
	float MarkerRotationZ;
	EDistanceMatchType DistanceMatchType;
	EDistanceMatchBasis DistanceMatchBasis;

	UPROPERTY(Transient)
	AActor* ParentActor;

	UPROPERTY(Transient)
	UCharacterMovementComponent* MovementComponent;

	//Cumulative Deltas
	FVector LastPosition;

public:	
	// Sets default values for this component's properties
	UDistanceMatching();

	UFUNCTION(BlueprintCallable, Category = "MotionSymphony|DistanceMatching")
	void TriggerStart(float DeltaTime); //Triggered when Idle and there is move input added

	UFUNCTION(BlueprintCallable, Category = "MotionSymphony|DistanceMatching")
	void TriggerStop(float DeltaTime); //Triggered when 

	UFUNCTION(BlueprintCallable, Category = "MotionSymphony|DistanceMatching")
	void TriggerPlant(float DeltaTime);

	UFUNCTION(BlueprintCallable, Category = "MotionSymphony|DistanceMatching")
	void TriggerPivotFrom();

	UFUNCTION(BlueprintCallable, Category = "MotionSymphony|DistanceMatching")
	void TriggerPivotTo();

	UFUNCTION(BlueprintCallable, Category = "MotionSymphony|DistanceMatching")
	void TriggerTurnInPlaceFrom();

	UFUNCTION(BlueprintCallable, Category = "MotionSymphony|DistanceMatching")
	void TriggerTurnInPlaceTo(FVector DesiredDirection);

	UFUNCTION(BlueprintCallable, Category = "MotionSymphony|DistanceMatching")
	void TriggerJump(float DeltaTime);

	UFUNCTION(BlueprintCallable, Category = "MotionSymphony|DistanceMatching")
	void StopDistanceMatching();

	UFUNCTION(BlueprintCallable, Category = "MotionSymphony|DistanceMatching")
	float GetMarkerDistance();

	UFUNCTION(BlueprintCallable, Category = "MotionSymphony|DistanceMatching")
	void DetectTransitions(float DeltaTime);

	UFUNCTION(BlueprintCallable, Category = "MotionSymphony|DistanceMatching")
	EDistanceMatchTrigger GetAndConsumeTriggeredTransition();

	float GetTimeToMarker() const;
	EDistanceMatchType GetDistanceMatchType() const;
	uint32 GetCurrentInstanceId() const;

	bool DoesCurrentStateMatchFeature(class UMatchFeature_Distance* DistanceMatchFeature) const;

protected:
	bool CalculateStartLocation(FVector& OutStartLocation, float DeltaTime, int32 MaxIterations) const;
	bool CalculateStopLocation(FVector& OutStopLocation, const float DeltaTime, int32 MaxIterations);
	float CalculateMarkerDistance() const;
	
	// Called when the game starts
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
};
