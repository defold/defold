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

package com.dynamo.bob.tile;

public class ConvexHull {
    String collisionGroup = "";
    int index = 0;
    int count = 0;

    public ConvexHull(ConvexHull convexHull) {
        this.collisionGroup = convexHull.collisionGroup;
        this.index = convexHull.index;
        this.count = convexHull.count;
    }

    public ConvexHull(String collisionGroup) {
        this.collisionGroup = collisionGroup;
    }

    public ConvexHull(String collisionGroup, int index, int count) {
        this.collisionGroup = collisionGroup;
        this.index = index;
        this.count = count;
    }

    public String getCollisionGroup() {
        return this.collisionGroup;
    }

    public void setCollisionGroup(String collisionGroup) {
        this.collisionGroup = collisionGroup;
    }

    public int getIndex() {
        return this.index;
    }

    public int getCount() {
        return this.count;
    }

    @Override
    public boolean equals(Object o) {
        if (o instanceof ConvexHull) {
            ConvexHull tile = (ConvexHull)o;
            return ((this.collisionGroup == tile.collisionGroup)
                    || (this.collisionGroup != null && this.collisionGroup.equals(tile.collisionGroup)))
                    && this.index == tile.index
                    && this.count == tile.count;
        } else {
            return super.equals(o);
        }
    }
}