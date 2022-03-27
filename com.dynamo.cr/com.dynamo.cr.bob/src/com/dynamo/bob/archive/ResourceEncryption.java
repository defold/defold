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

package com.dynamo.bob.archive;

import com.dynamo.crypt.Crypt;
import com.dynamo.bob.Bob;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.plugin.PluginScanner;

import java.util.List;


public class ResourceEncryption {

	private static class DefaultResourceEncyption extends ResourceEncryptionPlugin {
		private final byte[] KEY = "aQj8CScgNP4VsfXK".getBytes();
	
		@Override
		public byte[] encrypt(byte[] resource) throws Exception {
			return Crypt.encryptCTR(resource, KEY);
		}
	}

	private static ResourceEncryptionPlugin encryptionPlugin;

	/**
	 * Encrypt a resource
	 * @param resource Bytes of resource data to encrypt
	 * @return Bytes of encrypted resource data
	 */
	public static byte[] encrypt(byte[] resource) throws CompileExceptionError {
		try {
			if (encryptionPlugin == null) {
				// find and create plugin instances
				List<ResourceEncryptionPlugin> encryptionPlugins = PluginScanner.createPlugins("com.dynamo.bob.archive", ResourceEncryptionPlugin.class);
				
				// only accept a single encryption instance
				if (encryptionPlugins.size() > 1) {
					throw new CompileExceptionError("Found more than one ResourceEncryptionPlugin");
				}
				
				// default or custom encryption
				encryptionPlugin = encryptionPlugins.isEmpty() ? new DefaultResourceEncyption() : encryptionPlugins.get(0);
			}

			// do the encryption
			return encryptionPlugin.encrypt(resource);
		}
		catch (Exception e) {
			e.printStackTrace();
			throw new CompileExceptionError("Unable to encrypt resource", e);
		}
	}
}