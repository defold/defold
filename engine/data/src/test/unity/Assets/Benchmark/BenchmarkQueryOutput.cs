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
using Unity.Collections;
using Unity.Entities;

namespace Defold.Data.Benchmarks
{
    public sealed class QueryOutput : IDisposable
    {
        public EntityQuery Query;
        public NativeArray<Stats> Chunks;
        public QueryOutput(EntityQuery query, int capacity = 0)
        {
            Query = query;
            Chunks = new NativeArray<Stats>(Math.Max(capacity, query.CalculateChunkCount()), Allocator.Persistent);
        }
        public Stats Reduce()
        {
            Stats total = default;
            int count = Query.CalculateChunkCount();
            Values.Check(count <= Chunks.Length, "query output capacity");
            for (int i = 0; i < count; ++i)
                total.Add(Chunks[i]);
            return total;
        }
        public void Dispose()
        {
            Query.Dispose();
            Chunks.Dispose();
        }
    }
}
