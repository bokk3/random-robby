#include "DarkDungeoneerModule.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "ShaderCore.h"

#define LOCTEXT_NAMESPACE "FDarkDungeoneerModule"

void FDarkDungeoneerModule::StartupModule()
{
	const FString ProjectShaderDir = FPaths::Combine(FPaths::ProjectDir(), TEXT("Shaders/Private"));
	if (FPaths::DirectoryExists(ProjectShaderDir))
	{
		AddShaderSourceDirectoryMapping(TEXT("/ProjectShaders"), ProjectShaderDir);
	}
}

void FDarkDungeoneerModule::ShutdownModule()
{
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_PRIMARY_GAME_MODULE(FDarkDungeoneerModule, DarkDungeoneer, "DarkDungeoneer");
