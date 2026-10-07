// Copyright 2020-2026 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
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

#ifndef DM_BENCHMARK_DATA_BOIDS_H
#define DM_BENCHMARK_DATA_BOIDS_H

#include "benchmark_data_common.h"

struct Boid
{
    Vector3 m_Position;
    Vector3 m_Velocity;
};

struct BoidCell
{
    int32_t  m_X, m_Y, m_Z; // Compare coordinates as well as hashes.
    uint32_t m_Count;
    Vector3  m_PositionSum;
    Vector3  m_VelocitySum;
    uint32_t m_Target;
    bool     m_Avoid;
};

struct BoidsScratch
{
    Boid*     m_Snapshot;
    BoidCell* m_Cells;
    uint32_t* m_RowCells;
    uint32_t* m_Slots; // Cell index + 1, zero means empty.
    uint32_t  m_Count, m_SlotCount, m_CellCount;
};

// Caller owns scratch; allocation and reference validation are outside timing.
BoidsScratch CreateBoidsScratch(uint32_t num_rows);
void         DestroyBoidsScratch(BoidsScratch* scratch);
void         BuildBoidCells(BoidsScratch* scratch);
Boid         SteerBoid(const Boid& row, const BoidCell& cell);
double       BoidChecksum(const Boid& row);
BoidsScratch CreateBoidsReference(const TypeInput* type);
void         ValidateBoid(const Boid& actual, const Boid& expected);
void         ValidateBoidsCells();
Query        CreateBoidsQuery(Backend* store, uint32_t flock);
void         MeasureBoids(Backend* store, const Fixture* input, Query* queries, uint32_t sample);
void         ProfileBoids(const Fixture* input, uint32_t kind, uint32_t samples, uint32_t passes);

#endif
