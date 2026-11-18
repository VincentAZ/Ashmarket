// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Ashmarket : ModuleRules
{
	public Ashmarket(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate",
			"SlateCore",
			"Niagara",
			"IKRig",
			"PerformanceCaptureCore",
			"AudioMixer",
			"AudioMixerCore",
			"NonRealtimeAudioRenderer"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"Ashmarket",
			"Ashmarket/Gut",
			"Ashmarket/Town",
			"Ashmarket/Variant_Platforming",
			"Ashmarket/Variant_Platforming/Animation",
			"Ashmarket/Variant_Combat",
			"Ashmarket/Variant_Combat/AI",
			"Ashmarket/Variant_Combat/Animation",
			"Ashmarket/Variant_Combat/Gameplay",
			"Ashmarket/Variant_Combat/Interfaces",
			"Ashmarket/Variant_Combat/UI",
			"Ashmarket/Variant_SideScrolling",
			"Ashmarket/Variant_SideScrolling/AI",
			"Ashmarket/Variant_SideScrolling/Gameplay",
			"Ashmarket/Variant_SideScrolling/Interfaces",
			"Ashmarket/Variant_SideScrolling/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
