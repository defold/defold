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

import java.awt.Font;
import java.awt.font.FontRenderContext;
import java.awt.geom.AffineTransform;
import java.awt.geom.PathIterator;
import java.nio.file.Files;
import java.nio.file.Path;
import java.security.MessageDigest;
import java.util.HexFormat;
import java.util.Locale;

// Regenerate from engine/font/src/test/data/font_render with Java 25:
// java ExportFontQualityOutline.java ../WorkSans.ttf ../SourceCodePro-Regular.otf > coverage_outlines.json
public class ExportFontQualityOutline {
    public static void main(String[] args) throws Exception {
        if (args.length != 2)
            throw new IllegalArgumentException("Expected WorkSans.ttf and SourceCodePro-Regular.otf");
        Locale.setDefault(Locale.ROOT);
        System.out.println("{\"description\":\"Independent Java2D H outlines at 16384 px/em, normalized to em; no SDF or renderer output.\",\"generator\":\"ExportFontQualityOutline.java\",\"glyph\":\"H\",\"fonts\":{");
        for (int fontIndex = 0; fontIndex < args.length; ++fontIndex) {
            Path file = Path.of(args[fontIndex]);
            Font font = Font.createFont(Font.TRUETYPE_FONT, file.toFile()).deriveFont(16384f);
            FontRenderContext context = new FontRenderContext(new AffineTransform(), true, true);
            PathIterator path = font.createGlyphVector(context, "H").getGlyphOutline(0).getPathIterator(null);
            String hash = HexFormat.of().formatHex(MessageDigest.getInstance("SHA-256").digest(Files.readAllBytes(file)));
            System.out.printf("%s\"%s\":{\"file\":\"%s\",\"sha256\":\"%s\",\"ascent_em\":%.12f,\"commands\":[", fontIndex == 0 ? "" : ",", fontIndex == 0 ? "ttf" : "otf", file.getFileName(), hash, font.getLineMetrics("H", context).getAscent() / 16384.0);
            double[] points = new double[6];
            boolean first = true;
            while (!path.isDone()) {
                int kind = path.currentSegment(points);
                int count = new int[]{2, 2, 4, 6, 0}[kind];
                System.out.printf("%s[\"%s\"", first ? "" : ",", new String[]{"M", "L", "Q", "C", "Z"}[kind]);
                for (int i = 0; i < count; ++i)
                    System.out.printf(",%.12f", points[i] / 16384.0);
                System.out.print("]");
                first = false;
                path.next();
            }
            System.out.println("]}");
        }
        System.out.println("}}");
    }
}
