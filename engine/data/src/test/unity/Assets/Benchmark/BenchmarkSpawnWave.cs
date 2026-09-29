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

namespace Defold.Data.Benchmarks
{
    public static class SpawnWave
    {
        public static void Run(Store store, Fixture fixture)
        {
            for (int type = 0; type < 6; ++type)
                for (int row = 0; row < fixture.Extras[type]; row += 100)
                {
                    int first = fixture.Offsets[type] + fixture.Counts[type] + row;
                    store.Add(type, fixture.Rows, first, Math.Min(100, fixture.Extras[type] - row), first);
                }
        }
    }
}
