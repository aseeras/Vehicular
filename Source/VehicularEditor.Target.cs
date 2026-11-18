using UnrealBuildTool;

public class VehicularEditorTarget : TargetRules
{
	public VehicularEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("Vehicular");
	}
}
