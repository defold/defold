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
import java.util.HashMap;
import java.lang.reflect.Modifier;

import com.dynamo.bob.Bob;
import com.dynamo.bob.Project;
import com.dynamo.bob.IClassScanner;
import com.dynamo.bob.CompileExceptionError;


public class PluginScanner {

	private static HashMap<String, Object> pluginsCache = new HashMap<>();

	/**
	 * Find and create an instance of a class extending a specific base class
	 * and located within a specific package. The class must:
	 * 
	 * - Not be private
	 * - Not be abstract
	 * - Extend the base class
	 * 
	 * This function will generate a CompileExceptionError if more than one
	 * class in the package path extends the same base class.
	 * 
	 * @param packageName 
	 * @param pluginBaseClass
	 * @return Class instance or null if no class was found
	 */
	public static <T> T createPlugin(String packageName, Class<T> pluginBaseClass) throws CompileExceptionError {

		IClassScanner scanner = Project.getClassLoaderScanner();
		if (scanner == null) {
			Bob.verbose("PluginScanner has no class loader scanner");
			return null;
		}

		// check if we've already searched for and cached a plugin for this package path and base class
		// and if that is the case return the cached instance
		// note that HashMap accepts null as a value (and key for that matter) which means that we'll
		// return null if we have searched for and not found a plugin
		String pluginKey = packageName + pluginBaseClass;
		if (pluginsCache.containsKey(pluginKey)) {
			Bob.verbose("PluginScanner has cached plugin for key %s: %s", pluginKey, pluginsCache.get(pluginKey));
			return (T)pluginsCache.get(pluginKey);
		}

		Bob.verbose("PluginScanner searching %s for base class %s", packageName, pluginBaseClass);
		
		List<T> plugins = new ArrayList<>();
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
					if (plugins.size() > 1) {
						throw new CompileExceptionError("PluginScanner found more than one class implementing " + pluginBaseClass);
					}
				}
			}
			catch(InstantiationException | IllegalAccessException e) {
				throw new CompileExceptionError("Unable to create plugin " + className, e);
			}
			catch (Exception e) {
				throw new RuntimeException(e);
			}
		}

		// get the plugin (or null if none was found) and cache it
		T plugin = plugins.isEmpty() ? null : (T)plugins.get(0);
		pluginsCache.put(pluginKey, plugin);
		return plugin;
	}
}