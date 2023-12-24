//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#include "MotionSymphony.h"

#define LOCTEXT_NAMESPACE "FMotionSymphonyModule"

void FMotionSymphonyModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	
}

void FMotionSymphonyModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}
	
IMPLEMENT_MODULE(FMotionSymphonyModule, MotionSymphony)

#undef LOCTEXT_NAMESPACE