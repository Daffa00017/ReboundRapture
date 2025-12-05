// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ReboundRapture : ModuleRules
{
	public ReboundRapture(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] { "Core", 
															"CoreUObject", 
															"Engine", 
															"InputCore", 
															"EnhancedInput",
															"UMG", 
															"Slate",
															"SlateCore",
                                                            "Paper2D",
                                                            "PaperZD"});

		PrivateDependencyModuleNames.AddRange(new string[] {  });

	}
}
