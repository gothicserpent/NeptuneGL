// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;
using System.Collections.Generic;

public class NeptuneGLEditorTarget : TargetRules
{
	public NeptuneGLEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
        //BuildEnvironment = TargetBuildEnvironment.Unique;
		DefaultBuildSettings = BuildSettingsVersion.V4;

        ExtraModuleNames.AddRange( new string[] { "NeptuneGL" } );
	}
}
