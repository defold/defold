// Copyright 2020-2023 The Defold Foundation
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

import com.dynamo.bob.Project;
import com.dynamo.bob.IClassScanner;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.logging.Logger;


public class PluginScanner {

	private static Logger logger = Logger.getLogger(PluginScanner.class.getName());

	private static HashMap<String, Object> pluginsCache = new HashMap<>();


	public static <T> T getOrCreatePlugin(String packageName, Class<T> pluginBaseClass) throws CompileExceptionError {
		IClassScanner scanner = Project.getClassLoaderScanner();
		if (scanner == null) {
			logger.info("PluginScanner has no class loader scanner");
			return null;
		}
		return getOrCreatePlugin(scanner, packageName, pluginBaseClass);
	}

	/**
	 * Get a previously cached instance or find and create an instance of a class
	 * extending a specific base class and located within a specific package. The
	 * class must:
	 * 
	 * - Not be private
	 * - Not be abstract
	 * - Extend the base class
	 * 
	 * This function will generate a CompileExceptionError if more than one
	 * class in the package path extends the same base class.
	 * 
	 * @param scanner
	 * @param packageName 
	 * @param pluginBaseClass
	 * @return Class instance or null if no class was found
	 */
	public static <T> T getOrCreatePlugin(IClassScanner scanner, String packageName, Class<T> pluginBaseClass) throws CompileExceptionError {

		List<T> plugins = getOrCreatePlugins(scanner, packageName, pluginBaseClass);
		T plugin = null;
		if (plugins != null) {
			if (plugins.size() > 1) {
				throw new CompileExceptionError("PluginScanner found more than one class implementing " + pluginBaseClass + " in package " + packageName);
			}
			// get the plugin (or null if none was found) and cache it
			plugin = (T)plugins.get(0);
		}
		return plugin;
	}


	public static <T> List<T> getOrCreatePlugins(String packageName, Class<T> pluginBaseClass) throws CompileExceptionError {
		IClassScanner scanner = Project.getClassLoaderScanner();
		if (scanner == null) {
			logger.info("PluginScanner has no class loader scanner");
			return null;
		}
		return getOrCreatePlugins(scanner, packageName, pluginBaseClass);
	}
	
	/**
	 * Get a previously cached instances or find and create instances of classes
	 * extending a specific base class and located within a specific package. The
	 * class must:
	 * 
	 * - Not be private
	 * - Not be abstract
	 * - Extend the base class
	 * 
	 * @param scanner
	 * @param packageName 
	 * @param pluginBaseClass
	 * @return List with class instances or null if no class was found
	 */
	public static <T> List<T> getOrCreatePlugins(IClassScanner scanner, String packageName, Class<T> pluginBaseClass) throws CompileExceptionError {

		// check if we've already searched for and cached a plugin for this package path and base class
		// and if that is the case return the cached instance
		// note that HashMap accepts null as a value (and key for that matter) which means that we'll
		// return null if we have searched for and not found a plugin
		String pluginKey = packageName + pluginBaseClass;
		if (pluginsCache.containsKey(pluginKey)) {
			List<T> plugins = (List<T>)pluginsCache.get(pluginKey);
			if (plugins != null)
			{
				logger.info("PluginScanner has %d cached plugins for key %s", plugins.size(), pluginKey);
			}
			return plugins;
		}

		logger.info("PluginScanner searching %s for base class %s", packageName, pluginBaseClass);
		
		List<T> plugins = new ArrayList<>();
		Set<String> classNames = scanner.scan(packageName);
		for (String className : classNames) {
			try {
				Class<?> klass = Class.forName(className, true, scanner.getClassLoader());
				// check that the class extends or is of type pluginBaseClass and that it is not abstract
				boolean isAbstract = Modifier.isAbstract(klass.getModifiers());
				boolean isPrivate = Modifier.isPrivate(klass.getModifiers());
				if (pluginBaseClass.isAssignableFrom(klass) && !isAbstract && !isPrivate) {
					logger.info("Found plugin " + className);
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

		// get the plugin (or null if none was found) and cache it
		plugins = plugins.isEmpty() ? null : plugins;
		pluginsCache.put(pluginKey, plugins);
		return plugins;
	}
}