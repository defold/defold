package com.dynamo.bob.tile;

import java.awt.Graphics2D;
import java.awt.image.BufferedImage;
import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.FileReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.Reader;

import javax.imageio.ImageIO;

import com.dynamo.bob.tile.TileSetUtil.ConvexHulls;
import com.dynamo.tile.proto.Tile;
import com.dynamo.tile.proto.Tile.ConvexHull.Builder;
import com.dynamo.tile.proto.Tile.TileSet;
import com.google.protobuf.TextFormat;

public class TileSetc {

    private File contentRoot;

    public TileSetc(File contentRoot) {
        this.contentRoot = contentRoot;
    }

    BufferedImage loadImageFile(String fileName) throws IOException {
        File file = new File(this.contentRoot.getCanonicalPath()
                + File.separator + fileName);
        InputStream is = new BufferedInputStream(new FileInputStream(file));
        try {
            BufferedImage origImage = ImageIO.read(is);
            BufferedImage image = new BufferedImage(origImage.getWidth(), origImage.getHeight(), BufferedImage.TYPE_4BYTE_ABGR);
            Graphics2D g2d = image.createGraphics();
            g2d.drawImage(origImage, 0, 0, null);
            g2d.dispose();
            return image;
        } finally {
            is.close();
        }
    }

    public void compile(File inFile, File outFile) throws IOException {
        Reader reader = new BufferedReader(new FileReader(inFile));
        OutputStream output = new BufferedOutputStream(new FileOutputStream(outFile));

        try {
            TileSet.Builder builder = TileSet.newBuilder();
            TextFormat.merge(reader, builder);
            TileSet tileSet = builder.build();

            TileSet.Builder outBuilder = TileSet.newBuilder();
            outBuilder.mergeFrom(tileSet);

            outBuilder.clearConvexHulls();
            outBuilder.clearConvexHullPoints();

            String collisionPath = tileSet.getCollision();
            BufferedImage collisionImage = null;
            if (!collisionPath.equals("")) {
                collisionImage = loadImageFile(collisionPath);
                int width = collisionImage.getWidth();
                int height = collisionImage.getHeight();

                if (collisionImage.getAlphaRaster() == null) {
                    throw new RuntimeException(String.format("Collision image '%s' is missing alpha channel", collisionPath));
                }

                ConvexHulls convexHulls = TileSetUtil.calculateConvexHulls(
                        collisionImage.getAlphaRaster(), 16, width, height,
                        tileSet.getTileWidth(), tileSet.getTileHeight(),
                        tileSet.getTileMargin(), tileSet.getTileSpacing());

                for (int i = 0; i < convexHulls.hulls.length; ++i) {
                    ConvexHull convexHull = convexHulls.hulls[i];
                    Builder hullBuilder = Tile.ConvexHull.newBuilder();
                    String collisionGroup = "";
                    if (i < tileSet.getConvexHullsCount()) {
                        collisionGroup = tileSet.getConvexHulls(i).getCollisionGroup();
                    }
                    hullBuilder
                        .setCollisionGroup(collisionGroup)
                        .setCount(convexHull.getCount())
                        .setIndex(convexHull.getIndex());

                    outBuilder.addConvexHulls(hullBuilder);
                }

                for (int i = 0; i < convexHulls.points.length; ++i) {
                    outBuilder.addConvexHullPoints(convexHulls.points[i]);
                }
            }

            String imagePath = tileSet.getImage();
            BufferedImage image = loadImageFile(imagePath);
            if (image != null && (image.getWidth() < tileSet.getTileWidth() || image.getHeight() < tileSet.getTileHeight())) {
                throw new RuntimeException("Invalid tile dimensions");
            }
            if (image != null && collisionImage != null && (image.getWidth() != collisionImage.getWidth() || image.getHeight() != collisionImage.getHeight())) {
                throw new RuntimeException("Image dimensions differ");
            }
            String compiledImageName = imagePath;
            int index = compiledImageName.lastIndexOf('.');
            compiledImageName = compiledImageName.substring(0, index) + ".texturec";
            outBuilder.setImage(compiledImageName);

            TileSet outTileSet = outBuilder.build();
            outTileSet.writeTo(output);
        } finally {
            reader.close();
            output.close();
        }
    }

    static File locateGameProjectDirectory(String start) throws IOException {

        File current = new File(start).getCanonicalFile();
        File game_project;
        while (true) {
            game_project = new File(current.getPath() + File.separator
                    + "game.project");
            if (game_project.exists()) {
                return current;
            }
            String parent = current.getParent();
            if (parent == null) {
                System.err.println("game.project cound not be located");
                System.exit(5);
            }
            current = new File(current.getParent());
        }
    }

    public static void main(String[] args) throws IOException {
        System.setProperty("java.awt.headless", "true");

        String inFileName = args[0];
        String outFileName = args[1];

        File inFile = new File(inFileName);
        File outFile = new File(outFileName);
        File contentRoot = locateGameProjectDirectory(inFileName);
        TileSetc tileSetC = new TileSetc(contentRoot);
        tileSetC.compile(inFile, outFile);
    }
}
