//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#include "Objects/Tags/Tag_DistanceMarker.h"

UTag_DistanceMarker::UTag_DistanceMarker(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer),
	DistanceMatchTrigger(EDistanceMatchTrigger::None),
	DistanceMatchType(EDistanceMatchType::None),
	DistanceMatchBasis(EDistanceMatchBasis::Positional),
	Lead(0.0f),
	Tail(0.0f)
{
#if WITH_EDITORONLY_DATA
	NotifyColor = FColor::Green;
#endif

}
