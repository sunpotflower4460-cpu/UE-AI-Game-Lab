using UnrealBuildTool;

public class LumiStackEditorTarget : TargetRules
{
	public LumiStackEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("LumiStack");
	}
}
