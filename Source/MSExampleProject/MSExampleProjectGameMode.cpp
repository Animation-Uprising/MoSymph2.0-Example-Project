// Copyright Epic Games, Inc. All Rights Reserved.

#include "MSExampleProjectGameMode.h"
#include "MSExampleProjectCharacter.h"
#include "UObject/ConstructorHelpers.h"

AMSExampleProjectGameMode::AMSExampleProjectGameMode()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter"));
	if (PlayerPawnBPClass.Class != NULL)
	{
		DefaultPawnClass = PlayerPawnBPClass.Class;
	}
}
