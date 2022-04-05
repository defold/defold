// Copyright 2020-2022 The Defold Foundation
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
 * LuaBuilderPlugin. Abstract class for performing a Lua build/processing step.
 * Extend and annotate with LuaBuilderPluginParams.
 */
public abstract class LuaBuilderPlugin {

	/**
	 * Perform a build or processing step on the provided Lua code
	 * @param input String containing Lua code
	 * @return String with processed Lua code
	 */
	public abstract String build(String input) throws Exception;
}