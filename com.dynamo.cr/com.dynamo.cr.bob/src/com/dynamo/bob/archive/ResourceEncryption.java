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

package com.dynamo.bob.archive;

import com.dynamo.crypt.Crypt;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.plugin.PluginScanner;


public class ResourceEncryption {

	private static class DefaultResourceEncryption extends ResourceEncryptionPlugin {
		private final byte[] KEY = "aQj8CScgNP4VsfXK".getBytes();
	
		@Override
		public byte[] encrypt(byte[] resource) throws Exception {
			return Crypt.encryptCTR(resource, KEY);
		}
	}

	private static ResourceEncryptionPlugin encryptionPlugin;

	private static DefaultResourceEncryption defaultEncryption = new DefaultResourceEncryption();

	/**
	 * Encrypt a resource
	 * @param resource Bytes of resource data to encrypt
	 * @return Bytes of encrypted resource data
	 */
	public static byte[] encrypt(byte[] resource) throws CompileExceptionError {
		try {
			ResourceEncryptionPlugin encryptionPlugin = PluginScanner.getOrCreatePlugin("com.dynamo.bob.archive", ResourceEncryptionPlugin.class);
				
			// default or custom encryption
			encryptionPlugin = encryptionPlugin == null ? defaultEncryption : encryptionPlugin;

			// do the encryption
			return encryptionPlugin.encrypt(resource);
		}
		catch (Exception e) {
			e.printStackTrace();
			throw new CompileExceptionError("Unable to encrypt resource", e);
		}
	}
}