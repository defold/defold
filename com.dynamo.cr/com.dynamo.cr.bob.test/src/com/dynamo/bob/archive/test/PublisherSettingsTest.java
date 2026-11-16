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

package com.dynamo.bob.archive.test;

import com.dynamo.bob.archive.publisher.PublisherSettings;
import org.junit.Test;

import java.io.StringReader;
import java.util.Locale;

import static org.junit.Assert.assertEquals;

public class PublisherSettingsTest {
    // Settings identifiers must keep their ASCII case folding under a Turkish JVM locale.
    @Test
    public void parsesIdentifiersIndependentlyOfLocale() throws Exception {
        var previousLocale = Locale.getDefault();
        try {
            Locale.setDefault(Locale.forLanguageTag("tr-TR"));
            var folder = PublisherSettings.parse(new StringReader("""
                    [LIVEUPDATE]
                    MODE = Folder
                    OUTPUT-DIRECTORY = Exported
                    """));
            assertEquals(PublisherSettings.PublishMode.Folder, folder.getMode());
            assertEquals("Exported", folder.getOutputDirectory());

            var zip = PublisherSettings.parse(new StringReader("""
                    [liveupdate]
                    ZIP-COMPRESSION-LEVEL = 09
                    """));
            assertEquals(9, zip.getCompressionLevel());
        } finally {
            Locale.setDefault(previousLocale);
        }
    }
}
