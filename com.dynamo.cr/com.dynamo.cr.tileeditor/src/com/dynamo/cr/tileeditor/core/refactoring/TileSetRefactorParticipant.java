package com.dynamo.cr.tileeditor.core.refactoring;

import com.dynamo.cr.editor.core.GenericRefactorParticipant;
import com.dynamo.tile.proto.Tile.TileSet;
import com.google.protobuf.Message.Builder;

public class TileSetRefactorParticipant extends GenericRefactorParticipant {

    public TileSetRefactorParticipant() {
    }

    @Override
    public Builder getBuilder() {
        return TileSet.newBuilder();
    }

}
