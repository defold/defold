// Copyright 2026 The Defold Foundation
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

using System;
using System.IO;
using UnityEditor;
using UnityEditor.Build;
using UnityEditor.Build.Reporting;
using UnityEditor.SceneManagement;
using UnityEngine;

namespace Defold.Data.Benchmarks
{
    public static class BenchmarkBuild
    {
        public static void Build()
        {
            string output = BenchmarkMain.Argument("-benchmark-build", "Builds/Benchmark.app");
            bool safety = BenchmarkMain.Argument("-benchmark-validation", "false") == "true";
            PlayerSettings.companyName = "Defold";
            PlayerSettings.productName = "ECS Benchmark";
            PlayerSettings.SetScriptingBackend(NamedBuildTarget.Standalone, ScriptingImplementation.IL2CPP);
            PlayerSettings.SetIl2CppCodeGeneration(NamedBuildTarget.Standalone, Il2CppCodeGeneration.OptimizeSpeed);
            PlayerSettings.SetIl2CppCompilerConfiguration(NamedBuildTarget.Standalone,
                                                          Il2CppCompilerConfiguration.Release);
            PlayerSettings.SetManagedStrippingLevel(NamedBuildTarget.Standalone, ManagedStrippingLevel.Low);
            PlayerSettings.runInBackground = true;
            UnityEditor.OSXStandalone.UserBuildSettings.architecture = OSArchitecture.ARM64;
            var scene = EditorSceneManager.NewScene(NewSceneSetup.EmptyScene, NewSceneMode.Single);
            EditorSceneManager.SaveScene(scene, "Assets/Benchmark.unity");
            AssetDatabase.SaveAssets();
            var options =
            new BuildPlayerOptions { scenes = new[] { "Assets/Benchmark.unity" },
                                     locationPathName = Path.GetFullPath(output),
                                     target = BuildTarget.StandaloneOSX,
                                     extraScriptingDefines = new[] { "UNITY_DISABLE_AUTOMATIC_SYSTEM_BOOTSTRAP" },
                                     options = safety ? BuildOptions.Development : BuildOptions.None };
            BuildReport result = BuildPipeline.BuildPlayer(options);
            if (result.summary.result != BuildResult.Succeeded)
                throw new Exception("Unity benchmark build failed: " + result.summary.result);
            var packages = UnityEditor.PackageManager.PackageInfo.GetAllRegisteredPackages();
            using (var versions =
                   new StreamWriter(Path.GetFullPath(output) + ".packages.txt")) foreach (var package in packages)
            versions.WriteLine(package.name + "=" + package.version);
            Debug.Log($"BENCHMARK BUILD PASSED: {output}; safety={safety}");
        }
    }
}
