// Copyright (c) 2006-2019 Audiokinetic Inc. / All Rights Reserved
using UnrealBuildTool;
using System;
using System.IO;
using System.Collections.Generic;

public class AkUEPlatform_Android : AkUEPlatform
{
	private List<string> AndroidSDKFolders;
	public AkUEPlatform_Android(ReadOnlyTargetRules in_TargetRules, string in_ThirdPartyFolder) : base(in_TargetRules, in_ThirdPartyFolder) 
	{
		AndroidSDKFolders = new List<string>
		{
#if UE_4_25_OR_LATER
			Path.Combine(ThirdPartyFolder, "Android", "armeabi-v7a", AkConfigurationDir),
			Path.Combine(ThirdPartyFolder, "Android", "x86", AkConfigurationDir),
			Path.Combine(ThirdPartyFolder, "Android", "arm64-v8a", AkConfigurationDir),
			Path.Combine(ThirdPartyFolder, "Android", "x86_64", AkConfigurationDir),
#else
			Path.Combine(ThirdPartyFolder, "Android_armeabi-v7a", AkConfigurationDir),
			Path.Combine(ThirdPartyFolder, "Android_x86", AkConfigurationDir),
			Path.Combine(ThirdPartyFolder, "Android_arm64-v8a", AkConfigurationDir),
			Path.Combine(ThirdPartyFolder, "Android_x86_64", AkConfigurationDir),
	#endif
		};
	}


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

	public override string AkPlatformLibDir { get { return "Android"; } }

	public override string DynamicLibExtension { get { return "so"; } }

	public override List<string> GetPublicLibraryPaths()
	{
		var LibPaths = new List<string>();
		foreach (var folder in AndroidSDKFolders)
		{
			LibPaths.Add(Path.Combine(folder, "lib"));
		}

		return LibPaths;
	}

	public override List<string> GetAdditionalWwiseLibs()
	{
		return new List<string>();
	}

	public override List<string> GetRuntimeDependencies()
	{
		var Dependencies = new List<string>();
		foreach (var folder in AndroidSDKFolders)
		{
			Dependencies.AddRange(GetAllLibrariesInFolder(Path.Combine(folder, "bin"), DynamicLibExtension, false, true));
		}

		return Dependencies;
	}

	public override List<string> GetPublicSystemLibraries()
	{
		return new List<string>();
	}

	public override List<string> GetPublicDelayLoadDLLs()
	{
		return new List<string>();
	}

	public override List<string> GetPublicDefinitions()
	{
		return new List<string> {"__ANDROID__"};
	}

	public override Tuple<string, string> GetAdditionalPropertyForReceipt(string ModuleDirectory)
	{
		return new Tuple<string, string>("AndroidPlugin", Path.Combine(ModuleDirectory, "Wwise_APL.xml"));
	}

	public override List<string> GetPublicFrameworks()
	{
		return new List<string>();
	}
}
