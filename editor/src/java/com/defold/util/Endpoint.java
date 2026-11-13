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

package com.defold.util;

import clojure.lang.IHashEq;
import clojure.lang.Keyword;
import clojure.lang.Murmur3;
import clojure.lang.Util;

import java.util.Objects;

public record Endpoint(long nodeId, Keyword label) implements Comparable<Endpoint>, IHashEq {
    @Override
    public int compareTo(Endpoint other) {
        int nodeOrder = Long.compare(nodeId, other.nodeId);
        return nodeOrder == 0 ? label.compareTo(other.label) : nodeOrder;
    }

    @Override
    public boolean equals(Object other) {
        return this == other || other instanceof Endpoint endpoint
                && nodeId == endpoint.nodeId && label == endpoint.label;
    }

    @Override
    public int hasheq() {
        return Util.hashCombine(Murmur3.hashLong(nodeId), label.hasheq());
    }

    @Override
    public int hashCode() {
        return hasheq();
    }

    @Override
    public String toString() {
        return "#g/endpoint [" + nodeId + " " + Objects.toString(label, "") + "]";
    }
}
