using UnrealBuildTool;

public class Vehicular : ModuleRules
{
	public Vehicular(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core", "CoreUObject", "Engine", "InputCore",
			"EnhancedInput", "ChaosVehicles", "PhysicsCore", "AIModule", "DeveloperSettings", "Niagara"
		});
	}
}
