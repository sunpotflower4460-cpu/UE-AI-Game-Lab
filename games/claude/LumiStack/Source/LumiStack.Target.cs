using UnrealBuildTool;

public class LumiStackTarget : TargetRules
{
	public LumiStackTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("LumiStack");
	}
}
