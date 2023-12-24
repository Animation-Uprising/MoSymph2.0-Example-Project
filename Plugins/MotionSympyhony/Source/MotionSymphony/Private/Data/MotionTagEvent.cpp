//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#include "Data/MotionTagEvent.h"

FMotionTagEvent::FMotionTagEvent()
	: FAnimLinkableElement()
	, Tag(nullptr)
	, Duration(0.0f)
#if WITH_EDITORONLY_DATA
	, TagColor(FColor::Black)
#endif
	, TrackIndex(0)
	, CachedTagEventName(NAME_None)
	, CachedTagEventBaseName(NAME_None)
{

}

FMotionTagEvent::~FMotionTagEvent()
{

}

#if WITH_EDITORONLY_DATA
bool FMotionTagEvent::Serialize(FArchive& Ar)
{
	//return Super::Serialize();
	return true;
}

void FMotionTagEvent::PostSerialize(const FArchive& Ar)
{
	//Super::PostSerialize(Ar);
}
#endif

float FMotionTagEvent::GetTriggerTime() const
{
	return 0.0f;
}

float FMotionTagEvent::GetEndTriggerTime() const
{
	return 0.0f;
}

float FMotionTagEvent::GetDuration() const
{
	return 0.0f;
}

float FMotionTagEvent::SetDuration(float NewDuration)
{
	return NewDuration;
}

bool FMotionTagEvent::IsBlueprintTag() const
{
	return Tag == nullptr;
}

void FMotionTagEvent::SetTime(float NewTime, EAnimLinkMethod::Type ReferenceFrame /*= EAnimLinkMethod::Absolute*/)
{

}

FName FMotionTagEvent::GetTagEventName() const
{
	return NAME_None;
}

bool FMotionTagEvent::operator<(const FMotionTagEvent& Other) const
{
	return false;
}

bool FMotionTagEvent::operator==(const FMotionTagEvent& Other) const
{
	return false;
}

