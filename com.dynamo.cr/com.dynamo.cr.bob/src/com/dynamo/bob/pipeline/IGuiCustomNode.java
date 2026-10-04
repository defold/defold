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

package com.dynamo.bob.pipeline;

/**
 * Marker for GUI custom node build plugins. Plugins may provide static
 * {@code registerProperties(IGuiCustomType)} and {@code migrateProperties(Map)} methods.
 *
 * An optional static {@code validateNodes(Project, IResource, List<NodeDesc>, Map<String, IResource>)}
 * method validates the final nodes. Bob calls it once per node type and GUI build, after
 * migration, template and layout overrides, and generic property validation. The list contains
 * only nodes of this type from the default layout and all other layouts. The resource argument
 * is the source GUI for diagnostics; the map associates GUI resource aliases with their compiled
 * output resources. Referenced resources have already been built. Throw CompileExceptionError
 * to report a build error. Plugins without this method retain their existing behavior.
 * GUI task signatures include the used plugin JARs and registered property definitions.
 */
public interface IGuiCustomNode {
}
