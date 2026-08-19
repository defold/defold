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

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;

import org.junit.Test;

import com.dynamo.bob.bundle.HTML5Bundler;

/**
 * Tests for HTML5Bundler.getUrlOrigin(), which decides if index.html should contain a
 * preconnect hint for the origin hosting the game archive (html5.archive_location_prefix).
 * A null result means "no hint", ie the archive is loaded from the same origin as index.html.
 */
public class HTML5BundlerTest {

    // The default value of html5.archive_location_prefix and other relative prefixes must not
    // produce a preconnect hint, since they are served from the same origin as index.html.
    @Test
    public void testRelativeArchiveLocationPrefixHasNoOrigin() {
        assertNull(HTML5Bundler.getUrlOrigin("archive"));
        assertNull(HTML5Bundler.getUrlOrigin("/archive"));
        assertNull(HTML5Bundler.getUrlOrigin("./archive"));
        assertNull(HTML5Bundler.getUrlOrigin("../foo/archive"));
        assertNull(HTML5Bundler.getUrlOrigin("some/nested/archive"));
        assertNull(HTML5Bundler.getUrlOrigin(""));
    }

    @Test
    public void testAbsoluteArchiveLocationPrefix() {
        assertEquals("https://cdn.example.com", HTML5Bundler.getUrlOrigin("https://cdn.example.com/games/mygame/archive"));
        assertEquals("http://cdn.example.com", HTML5Bundler.getUrlOrigin("http://cdn.example.com/archive"));
    }

    // A prefix without any path is a valid url and should still yield an origin
    @Test
    public void testAbsoluteArchiveLocationPrefixWithoutPath() {
        assertEquals("https://cdn.example.com", HTML5Bundler.getUrlOrigin("https://cdn.example.com"));
        assertEquals("https://cdn.example.com", HTML5Bundler.getUrlOrigin("https://cdn.example.com/"));
    }

    // A non default port is part of the origin and must be kept, or the browser would warm up
    // a connection to the wrong endpoint
    @Test
    public void testAbsoluteArchiveLocationPrefixWithPort() {
        assertEquals("https://cdn.example.com:8443", HTML5Bundler.getUrlOrigin("https://cdn.example.com:8443/archive"));
        assertEquals("http://localhost:8080", HTML5Bundler.getUrlOrigin("http://localhost:8080/archive"));
    }

    // A protocol relative prefix inherits the scheme of the page, and the hint should do the same
    @Test
    public void testProtocolRelativeArchiveLocationPrefix() {
        assertEquals("//cdn.example.com", HTML5Bundler.getUrlOrigin("//cdn.example.com/archive"));
        assertEquals("//cdn.example.com:8443", HTML5Bundler.getUrlOrigin("//cdn.example.com:8443/archive"));
    }

    // Everything the browser cannot preconnect to should be ignored rather than throw, so that a
    // surprising archive_location_prefix never breaks bundling
    @Test
    public void testUnsupportedSchemeHasNoOrigin() {
        assertNull(HTML5Bundler.getUrlOrigin("ftp://cdn.example.com/archive"));
        assertNull(HTML5Bundler.getUrlOrigin("file:///Users/me/archive"));
    }

    @Test
    public void testMalformedUrlHasNoOrigin() {
        assertNull(HTML5Bundler.getUrlOrigin("https://cdn.example.com/a b c"));
        assertNull(HTML5Bundler.getUrlOrigin("https://"));
        assertNull(HTML5Bundler.getUrlOrigin("::not a url::"));
    }

    @Test
    public void testNullUrlHasNoOrigin() {
        assertNull(HTML5Bundler.getUrlOrigin(null));
    }
}
