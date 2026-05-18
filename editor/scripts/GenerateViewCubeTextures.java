// Generates the per-face label textures used by editor.scene-view-cube.
// Run offline whenever the look of the labels needs to change:
//
//   java editor/scripts/GenerateViewCubeTextures.java
//
// PNGs are written to editor/resources/scene/images/view_cube/. Check the
// result into source control so the editor can load them at runtime instead
// of rebuilding them in a Graphics2D pipeline on every launch.

import java.awt.AlphaComposite;
import java.awt.Color;
import java.awt.Font;
import java.awt.FontMetrics;
import java.awt.Graphics2D;
import java.awt.RenderingHints;
import java.awt.image.BufferedImage;
import java.io.File;
import java.io.IOException;
import javax.imageio.ImageIO;

public class GenerateViewCubeTextures {

    private static final int SIZE = 512;
    private static final int FONT_POINTS = 224;
    private static final String OUTPUT_DIR = "editor/resources/scene/images/view_cube";

    private static final String[][] FACES = {
        {"pos_x", "+X"},
        {"neg_x", "-X"},
        {"pos_y", "+Y"},
        {"neg_y", "-Y"},
        {"pos_z", "+Z"},
        {"neg_z", "-Z"},
    };

    public static void main(String[] args) throws IOException {
        File outputDir = new File(OUTPUT_DIR);
        if (!outputDir.isDirectory() && !outputDir.mkdirs()) {
            throw new IOException("Could not create output directory: " + outputDir);
        }

        for (String[] face : FACES) {
            String fileName = face[0];
            String label = face[1];
            BufferedImage image = makeLabelImage(label);
            File outputFile = new File(outputDir, fileName + ".png");
            ImageIO.write(image, "PNG", outputFile);
            System.out.println("Wrote " + outputFile.getPath());
        }
    }

    private static BufferedImage makeLabelImage(String label) {
        BufferedImage image = new BufferedImage(SIZE, SIZE, BufferedImage.TYPE_INT_ARGB);
        Graphics2D g = image.createGraphics();
        try {
            g.setComposite(AlphaComposite.Clear);
            g.fillRect(0, 0, SIZE, SIZE);
            g.setComposite(AlphaComposite.SrcOver);
            g.setRenderingHint(RenderingHints.KEY_ANTIALIASING, RenderingHints.VALUE_ANTIALIAS_ON);
            g.setRenderingHint(RenderingHints.KEY_TEXT_ANTIALIASING, RenderingHints.VALUE_TEXT_ANTIALIAS_ON);
            g.setColor(Color.WHITE);
            g.setFont(new Font(Font.SANS_SERIF, Font.BOLD, FONT_POINTS));
            FontMetrics metrics = g.getFontMetrics();
            int textWidth = metrics.stringWidth(label);
            int ascent = metrics.getAscent();
            int x = (int) ((SIZE / 2.0) - (textWidth / 2.0));
            int y = (int) ((SIZE / 2.0) + (ascent / 3.0));
            g.drawString(label, x, y);
        } finally {
            g.dispose();
        }
        return image;
    }
}
