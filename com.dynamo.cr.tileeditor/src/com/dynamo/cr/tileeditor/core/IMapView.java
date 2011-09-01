package com.dynamo.cr.tileeditor.core;

public interface IMapView {

    void setImage(String eq);

    void setTileWidth(int eq);

    void setTileHeight(int eq);

    void setTileCount(int eq);

    void setTileMargin(int eq);

    void setTileSpacing(int eq);

    void setCollision(String eq);

    void setMaterialTag(String eq);
}
