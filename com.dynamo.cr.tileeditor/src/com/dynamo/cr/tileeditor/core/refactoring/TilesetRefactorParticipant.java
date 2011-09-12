package com.dynamo.cr.tileeditor.core.refactoring;

import com.dynamo.cr.editor.core.GenericRefactorParticipant;
import com.dynamo.tile.proto.Tile.TileSet;
import com.google.protobuf.Message.Builder;

public class TilesetRefactorParticipant extends GenericRefactorParticipant {

    public TilesetRefactorParticipant() {
    }

    @Override
    public Builder getBuilder() {
        return TileSet.newBuilder();
    }

}
