// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;
using System.Collections.Generic;

[SupportedPlatforms(UnrealPlatformClass.Server)]
public class NeptuneGLServerTarget : TargetRules
{
	public NeptuneGLServerTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Server;
		//BuildEnvironment = TargetBuildEnvironment.Unique;
		DefaultBuildSettings = BuildSettingsVersion.V4;

		ExtraModuleNames.AddRange( new string[] { "NeptuneGL" } );
	}
}
