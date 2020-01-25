// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;

public class NeptuneGL : ModuleRules
{
	public NeptuneGL(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"PhysicsCore",     // Needed for GetPhysicsLinearVelocity()
			"Niagara",         // Niagara VFX systems and function library
			// "EnhancedInput",   // Optional but recommended for modern input
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
		
		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
