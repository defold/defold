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
using System.Globalization;
using Unity.Burst;
using Unity.Collections;
using Unity.Jobs;
using UnityEngine;

namespace Defold.Data.Benchmarks
{
    public sealed class BenchmarkMain : MonoBehaviour
    {
        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.AfterSceneLoad)]
        static void Launch() => new GameObject("ECS Benchmark").AddComponent<BenchmarkMain>();

        void Start()
        {
            try
            {
                Run();
                Application.Quit(0);
            }
            catch (Exception error)
            {
                Debug.LogException(error);
                Application.Quit(1);
            }
        }
        public static void Run()
        {
            CultureInfo.CurrentCulture = CultureInfo.InvariantCulture;
            string mode = Argument("-benchmark-mode", "core");
            string output = Argument("-benchmark-output", "unity-timing.csv");
            int rows = int.Parse(Argument("-benchmark-rows", "1000000"));
            int samples = int.Parse(Argument("-benchmark-samples", "7"));
            Values.Check(rows >= 1000 && rows % 1000 == 0 && samples > 0, "invalid benchmark size");
            using var probe = new NativeArray<int>(1, Allocator.Persistent);
            new BurstProbe { Result = probe }.Run();
            Values.Check(probe[0] == 1, "Burst must be enabled; managed fallback is not a timing result");
#if ENABLE_UNITY_COLLECTIONS_CHECKS
            const bool safety = true;
#else
            const bool safety = false;
#endif
            Debug.Log(
            $"Benchmark Unity={Application.unityVersion}; Burst=active; collections_safety={safety}; development={Debug.isDebugBuild}");
            bool validation = Argument("-benchmark-validation", "false") == "true";
            Values.Check(safety == validation && Debug.isDebugBuild == validation,
                         "player safety/development configuration differs from requested mode");
            if (mode == "core")
                Core.Run(output, rows, samples);
            else if (mode == "threaded")
            {
                int frames = int.Parse(Argument("-benchmark-frames", "32"));
                int workers = int.Parse(Argument("-job-worker-count", "1"));
                Values.Check(frames > 0 && frames <= 1000 &&
                             (workers == 1 || workers == 2 || workers == 4 || workers == 8),
                             "invalid frames/workers");
                Threaded.Run(output, rows, frames, workers);
            }
            else
                throw new ArgumentException("Unknown benchmark mode: " + mode);
            Debug.Log("BENCHMARK PASSED");
        }

        public static string Argument(string name, string fallback)
        {
            string[] args = Environment.GetCommandLineArgs();
            for (int i = 0; i + 1 < args.Length; ++i)
                if (args[i] == name)
                    return args[i + 1];
            return fallback;
        }
    }
    [BurstCompile(CompileSynchronously = true)]
    struct BurstProbe : IJob
    {
        public NativeArray<int> Result;
        [BurstDiscard]
        static void Managed(ref int value)
        {
            value = 0;
        }
        public void Execute()
        {
            int value = 1;
            Managed(ref value);
            Result[0] = value;
        }
    }
}
