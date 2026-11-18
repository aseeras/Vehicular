using UnrealBuildTool;

public class VehicularTarget : TargetRules
{
	public VehicularTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("Vehicular");
	}
}
