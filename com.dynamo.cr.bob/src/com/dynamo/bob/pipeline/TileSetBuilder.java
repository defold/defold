package com.dynamo.bob.pipeline;

import java.io.File;
import java.io.IOException;

import com.dynamo.bob.Builder;
import com.dynamo.bob.BuilderParams;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.IResource;
import com.dynamo.bob.Task;
import com.dynamo.tile.TileSetc;

@BuilderParams(name = "TileSet", inExts = ".tileset", outExt = ".tilesetc")
public class TileSetBuilder extends Builder<Void>  {

    @Override
    public Task<Void> create(IResource input) throws IOException {
        return defaultTask(input);
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

    @Override
    public void build(Task<Void> task) throws CompileExceptionError,
            IOException {

        String inFileName = task.input(0).getAbsPath();
        String outFileName = task.output(0).getAbsPath();

        File inFile = new File(inFileName);
        File outFile = new File(outFileName);
        File contentRoot = locateGameProjectDirectory(inFileName);
        TileSetc tileSetC = new TileSetc(contentRoot);
        tileSetC.compile(inFile, outFile);
    }
}
