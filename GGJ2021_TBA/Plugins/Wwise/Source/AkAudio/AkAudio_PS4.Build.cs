// Copyright (c) 2006-2019 Audiokinetic Inc. / All Rights Reserved
using UnrealBuildTool;
using System;
using System.IO;
using System.Collections.Generic;

public class AkUEPlatform_PS4 : AkUEPlatform
{
	public AkUEPlatform_PS4(ReadOnlyTargetRules in_TargetRules, string in_ThirdPartyFolder) : base(in_TargetRules, in_ThirdPartyFolder) {}

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

	public override string AkPlatformLibDir { get { return "PS4"; } }

	public override string DynamicLibExtension { get { return "prx"; } }

	public override List<string> GetAdditionalWwiseLibs()
	{
		return new List<string> 
		{
			"SceAudio3dEngine"
		};
	}

	public override List<string> GetPublicSystemLibraries()
	{
		return new List<string>
		{
			"SceAjm_stub_weak",
			"SceAudio3d_stub_weak",
			"SceMove_stub_weak"
		};
	}

	public override List<string> GetPublicDelayLoadDLLs()
	{
		return new List<string>();
	}

	public override List<string> GetPublicDefinitions()
	{
		return new List<string>
		{
			"__ORBIS__"
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
