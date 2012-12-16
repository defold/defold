package com.dynamo.bob.pipeline;

import java.io.ByteArrayInputStream;
import java.io.File;
import java.io.IOException;
import java.io.InputStreamReader;

import com.dynamo.bob.Builder;
import com.dynamo.bob.BuilderParams;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.IResource;
import com.dynamo.bob.Task;
import com.dynamo.bob.Task.TaskBuilder;
import com.dynamo.bob.tile.TileSetc;
import com.dynamo.tile.proto.Tile.TileSet;
import com.google.protobuf.TextFormat;

@BuilderParams(name = "TileSet", inExts = {".tileset", ".tilesource"}, outExt = ".tilesetc")
public class TileSetBuilder extends Builder<Void>  {

    @Override
    public Task<Void> create(IResource input) throws IOException, CompileExceptionError {
        TileSet.Builder builder = TileSet.newBuilder();
        TextFormat.merge(new InputStreamReader(new ByteArrayInputStream(input.getContent())), builder);
        TileSet tileSet = builder.build();
        String imgPath = tileSet.getImage();
        String collisionPath = tileSet.getCollision();
        IResource image = this.project.getResource(imgPath);
        IResource collision = this.project.getResource(collisionPath);
        if (image.exists() || collision.exists()) {
            TaskBuilder<Void> taskBuilder = Task.<Void>newBuilder(this)
                    .setName(params.name())
                    .addInput(input)
                    .addOutput(input.changeExt(params.outExt()));
            if (collision.exists()) {
                taskBuilder.addInput(collision);
            }
            return taskBuilder.build();
        } else {
            if (!imgPath.isEmpty()) {
                BuilderUtil.checkFile(this.project, input, "image", imgPath);
            } else if (!collisionPath.isEmpty()) {
                BuilderUtil.checkFile(this.project, input, "collision", collisionPath);
            } else {
                throw new CompileExceptionError(input, 0, Messages.TileSetBuilder_MISSING_IMAGE_AND_COLLISION);
            }
            // will not be reached
            return null;
        }
    }

    static File locateGameProjectDirectory(IResource resource) throws IOException, CompileExceptionError {

        String start = resource.getAbsPath();
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
                throw new CompileExceptionError(resource, 0, "game.project cound not be located");
            }
            current = new File(current.getParent());
        }
    }

    @Override
    public void build(Task<Void> task) throws CompileExceptionError,
            IOException {

        String inFileName = task.input(0).getAbsPath();
        String outFileName = task.output(0).getAbsPath();
        // TODO: Workaround to create the path to the file. TileSetC should be changed
        // to write to and OutputStream instead
        if (!task.output(0).exists()) {
            task.output(0).setContent(new byte[0]);
        }

        File inFile = new File(inFileName);
        File outFile = new File(outFileName);
        File contentRoot = locateGameProjectDirectory(task.input(0));
        TileSetc tileSetC = new TileSetc(contentRoot);
        tileSetC.compile(inFile, outFile);
    }
}
