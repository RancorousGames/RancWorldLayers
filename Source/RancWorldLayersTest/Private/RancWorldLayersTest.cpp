#include "RancWorldLayersTest.h"

#define LOCTEXT_NAMESPACE "FRancWorldLayersTestModule"

void FRancWorldLayersTestModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file
}

void FRancWorldLayersTestModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FRancWorldLayersTestModule, RancWorldLayersTest)