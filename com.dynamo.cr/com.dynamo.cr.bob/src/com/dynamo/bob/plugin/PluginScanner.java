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

package com.dynamo.bob.plugin;

import java.util.ArrayList;
import java.util.List;
import java.util.Set;
import java.lang.reflect.Modifier;

import com.dynamo.bob.Bob;
import com.dynamo.bob.Project;
import com.dynamo.bob.IClassScanner;
import com.dynamo.bob.CompileExceptionError;


public class PluginScanner {

	/**
	 * Find and create instances of classes extending a specific base class
	 * and located within a specific package. The class must:
	 * 
	 * - Not be private
	 * - Not be abstract
	 * - Extend the base class
	 * 
	 * @param packageName 
	 * @param pluginBaseClass
	 * @return List of instances
	 */
	public static <T> List<T> createPlugins(String packageName, Class<T> pluginBaseClass) throws CompileExceptionError {
		List<T> plugins = new ArrayList<>();

		IClassScanner scanner = Project.getClassLoaderScanner();
		if (scanner == null) {
			Bob.verbose("PluginScanner has no class loader scanner");
			return plugins;
		}

		Bob.verbose("PluginScanner searching %s for base class %s", packageName, pluginBaseClass);
		
		Set<String> classNames = scanner.scan(packageName);
		for (String className : classNames) {
			try {
				Class<?> klass = Class.forName(className, true, scanner.getClassLoader());
				// check that the class extends or is of type pluginBaseClass and that it is not abstract
				boolean isAbstract = Modifier.isAbstract(klass.getModifiers());
				boolean isPrivate = Modifier.isPrivate(klass.getModifiers());
				if (pluginBaseClass.isAssignableFrom(klass) && !isAbstract && !isPrivate) {
					Bob.verbose("Found plugin " + className);
					plugins.add((T)klass.newInstance());
				}
			}
			catch(InstantiationException | IllegalAccessException e) {
				throw new CompileExceptionError("Unable to create plugin " + className, e);
			}
			catch (Exception e) {
				throw new RuntimeException(e);
			}
		}
		return plugins;
	}
}