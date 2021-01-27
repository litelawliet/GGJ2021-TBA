// Copyright (c) 2006-2019 Audiokinetic Inc. / All Rights Reserved
using UnrealBuildTool;
using System;
using System.IO;
using System.Collections.Generic;

public class AkUEPlatform_Switch : AkUEPlatform
{
	public AkUEPlatform_Switch(ReadOnlyTargetRules in_TargetRules, string in_ThirdPartyFolder) : base(in_TargetRules, in_ThirdPartyFolder) {}

#if !UE_4_24_OR_LATER
	public override string SanitizeLibName(string in_libName)
	{
		return in_libName;
	}
#endif

	public override string GetLibraryFullPath(string LibName, string LibPath)
	{
		return Path.Combine(LibPath, "lib" + LibName + ".a");
	}

	public override bool SupportsAkAutobahn { get { return false; } }

	public override bool SupportsCommunication { get { return true; } }

	public override string AkPlatformLibDir { get { return "NX64"; } }

	public override string DynamicLibExtension { get { return string.Empty; } }

	public override List<string> GetAdditionalWwiseLibs()
	{
		var libs = GetAllLibrariesInFolder(Path.Combine(ThirdPartyFolder, AkPlatformLibDir, AkConfigurationDir, "lib"), "a", true);
		libs.Add("AkOpusNXDecoder");
		return libs;
	}

	public override List<string> GetRuntimeDependencies()
	{
		return new List<string>();
	}

	public override List<string> GetPublicDelayLoadDLLs()
	{
		return new List<string>();
	}

	public override List<string> GetPublicSystemLibraries()
	{
		return new List<string>();
	}

	public override List<string> GetPublicDefinitions()
	{
		return new List<string>
		{
			"NN_NINTENDO_SDK"
		};
	}

	public override Tuple<string, string> GetAdditionalPropertyForReceipt(string ModuleDirectory)
	{
		return null;
	}

	public override List<string> GetPublicFrameworks()
	{
		return new List<string>();
	}
}
