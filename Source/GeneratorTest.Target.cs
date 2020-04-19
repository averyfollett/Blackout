// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;
using System.Collections.Generic;

public class GeneratorTestTarget : TargetRules
{
	public GeneratorTestTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V2;
		bUsesSteam = true;

		ExtraModuleNames.AddRange( new string[] { "GeneratorTest" } );
	}
}
