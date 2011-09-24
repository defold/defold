package com.dynamo.cr.tileeditor.core.refactoring;

import com.dynamo.cr.editor.core.GenericRefactorParticipant;
import com.dynamo.tile.proto.Tile.TileGrid;
import com.google.protobuf.Message.Builder;

public class TileGridRefactorParticipant extends GenericRefactorParticipant {

    public TileGridRefactorParticipant() {
    }

    @Override
    public Builder getBuilder() {
        return TileGrid.newBuilder();
    }

}
