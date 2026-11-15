using UnrealBuildTool;

public class DarkDungeoneer : ModuleRules
{
	public DarkDungeoneer(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		CppStandard = CppStandardVersion.Cpp20;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"PhysicsCore",
			"InputCore",
			"EnhancedInput",
			"GameplayTags",
			"NetCore",
			"RenderCore",
			"Renderer",
			"RHI",
			"Projects",
			"Niagara",
			"AIModule",
			"NavigationSystem",
			"AudioMixer",
			"AudioExtensions",
			"MetasoundEngine",
			"MetasoundFrontend",
			"OnlineSubsystem",
			"OnlineSubsystemUtils",
			"UMG",
			"Slate",
			"SlateCore",
			"Json",
			"JsonUtilities"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"ApplicationCore",
			"ImageWrapper"
		});
	}
}
