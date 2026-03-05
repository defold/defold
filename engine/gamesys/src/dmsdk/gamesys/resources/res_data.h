// Copyright 2020-2026 The Defold Foundation
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

#ifndef DMSDK_GAMESYS_RES_DATA_H
#define DMSDK_GAMESYS_RES_DATA_H

#include <gamesys/data_ddf.h>

namespace dmGameSystem
{
    /*# Data resource access
     *
     * Helper types and accessors for the data resource type (`.datac`), which
     * is the compiled form of the `dmGameSystemDDF::Data` message (from
     * `data_ddf.proto`). This resource type is used together with the
     * `game.data` API in scripts.
     *
     * @document
     * @namespace dmGameSystem
     * @name Data Resource
     * @language C++
     */

    /*# Opaque handle to a loaded data resource
     *
     * This struct is an opaque handle managed by the engine. Use
     * `GetDDFData()` to access the underlying protobuf message for read-only
     * inspection.
     *
     * @struct
     * @name DataResource
     */
    struct DataResource;

    /*# Get the underlying DDF data
     *
     * Returns a pointer to the decoded `dmGameSystemDDF::Data` protobuf
     * message contained in the given data resource.
     *
     * The lifetime of the returned pointer is tied to the lifetime of the
     * resource. It must not be freed by the caller.
     *
     * @name GetDDFData
     * @param res [type: DataResource*] Data resource handle
     * @return data [type: const dmGameSystemDDF::Data*] Pointer to the DDF
     *         data message, or `0` if the resource is invalid.
     */
    const dmGameSystemDDF::Data* GetDDFData(DataResource* res);
}

#endif // DMSDK_GAMESYS_RES_DATA_H

