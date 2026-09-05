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

package com.dynamo.bob.bundle.test;

import java.io.IOException;
import java.io.InputStream;
import java.net.URL;
import java.nio.charset.StandardCharsets;

import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import org.junit.Test;

import com.dynamo.bob.bundle.HTML5Bundler;

/**
 * Verifies invariants of the dmloader.js bundle template that aren't
 * easy to assert from runtime behaviour because the bob test module
 * has no JS execution harness.
 */
public class DmLoaderTemplateTest {

    private static String readDmLoaderTemplate() throws IOException {
        URL url = HTML5Bundler.class.getResource("resources/web/dmloader.js");
        assertNotNull("dmloader.js template missing from classpath", url);
        try (InputStream in = url.openStream()) {
            return new String(in.readAllBytes(), StandardCharsets.UTF_8);
        }
    }

    /**
     * dmloader.js defines `var Module = {...}` partway through the file,
     * which would discard any host-set Module object loaded earlier on
     * the page. The host opt-out for pthread WASM is therefore captured
     * into a separate variable (DEFOLD_HOST_NO_PTHREAD) before that
     * assignment, and consumed at the probe site below. Both pieces
     * must be present for the opt-out to actually take effect.
     */
    @Test
    public void hostPthreadOptOutIsCapturedBeforeModuleRedefinition() throws IOException {
        String src = readDmLoaderTemplate();

        int captureIdx = src.indexOf("var DEFOLD_HOST_NO_PTHREAD");
        // Anchor on the redefinition's first property (`engineVersion`) so
// the search isn't fooled by documentation comments earlier in the
// file that mention `var Module = {...}` literally.
int redefineIdx = src.indexOf("var Module = {\n    engineVersion");
        int consumeIdx = src.indexOf("if (DEFOLD_HOST_NO_PTHREAD)");

        assertTrue("missing DEFOLD_HOST_NO_PTHREAD capture", captureIdx >= 0);
        assertTrue("missing `var Module = {` redefinition", redefineIdx >= 0);
        assertTrue("missing DEFOLD_HOST_NO_PTHREAD consumer at probe site", consumeIdx >= 0);

        assertTrue(
            "DEFOLD_HOST_NO_PTHREAD must be captured *before* `var Module = {` "
                + "or the host-set property is discarded before the capture runs",
            captureIdx < redefineIdx);

        assertTrue(
            "DEFOLD_HOST_NO_PTHREAD must be consumed *after* the Module "
                + "redefinition so the probe site uses the captured value",
            consumeIdx > redefineIdx);
    }

    /**
     * Five additional host opt-outs share the same capture-before-
     * redefinition pattern: a WebView wrapper pre-sets one or more of
     * `isWebGL2Supported`, `webGLContextAttributes`,
     * `webGLExtensionFilter`, `showButtonStrip`, or
     * `autoReloadOnWebGLContextRestore` on Module before the loader
     * script runs. Each capture must appear before `var Module = {`
     * so the host-set property survives the redefinition.
     */
    @Test
    public void allHostOptOutsCapturedBeforeModuleRedefinition() throws IOException {
        String src = readDmLoaderTemplate();

        // Anchor on the redefinition's first property (`engineVersion`) so
// the search isn't fooled by documentation comments earlier in the
// file that mention `var Module = {...}` literally.
int redefineIdx = src.indexOf("var Module = {\n    engineVersion");
        assertTrue("missing `var Module = {` redefinition", redefineIdx >= 0);

        String[] captureTokens = {
            "var DEFOLD_HOST_NO_PTHREAD",
            "var DEFOLD_HOST_NO_WEBGL2",
            "var DEFOLD_HOST_GL_CTX_ATTRS",
            "var DEFOLD_HOST_GL_EXT_FILTER",
            "var DEFOLD_HOST_NO_BUTTON_STRIP",
            "var DEFOLD_HOST_WEBGL_RELOAD",
        };
        for (String token : captureTokens) {
            int idx = src.indexOf(token);
            assertTrue("missing capture: " + token, idx >= 0);
            assertTrue(
                token + " must be captured *before* `var Module = {` "
                    + "or the host-set property is discarded before the capture runs",
                idx < redefineIdx);
        }
    }

    /**
     * The non-pthread opt-outs (WebGL2 downgrade, ctx attrs, extension
     * filter, CSS, context-loss) only patch DOM / prototypes — they
     * don't touch Module — so they're applied immediately at the
     * capture site rather than deferred to a second consumer block.
     * The IIFE wrapping these applies must therefore appear *before*
     * the Module redefinition.
     */
    @Test
    public void hostOptOutApplyBlockRunsBeforeModuleRedefinition() throws IOException {
        String src = readDmLoaderTemplate();

        int applyIdx = src.indexOf("function applyDefoldHostOptOuts");
        // Anchor on the redefinition's first property (`engineVersion`) so
// the search isn't fooled by documentation comments earlier in the
// file that mention `var Module = {...}` literally.
int redefineIdx = src.indexOf("var Module = {\n    engineVersion");

        assertTrue("missing applyDefoldHostOptOuts IIFE", applyIdx >= 0);
        assertTrue("missing `var Module = {` redefinition", redefineIdx >= 0);
        assertTrue(
            "applyDefoldHostOptOuts must run *before* `var Module = {` "
                + "so the prototype / DOM patches are installed before the "
                + "WASM engine creates the WebGL context",
            applyIdx < redefineIdx);
    }
}
