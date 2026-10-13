using UnrealBuildTool;

public class LumiStack : ModuleRules
{
	public LumiStack(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Lets sources include "GameCore/LumiCore.h" regardless of their own folder.
		PublicIncludePaths.Add(ModuleDirectory);

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"Slate",
			"SlateCore",
			"ProceduralMeshComponent",
			"RHI",
			"RenderCore",
		});
	}
}
