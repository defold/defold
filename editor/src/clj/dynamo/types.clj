(ns dynamo.types
  "Schema and type definitions. Refer to Prismatic's schema.core for s/* definitions."
  (:require [potemkin.namespaces :refer [import-vars]]
            [schema.core :as s])
  (:import [com.dynamo.graphics.proto Graphics$TextureImage$TextureFormat]
           [com.dynamo.tile.proto Tile$Playback]
           [java.awt.image BufferedImage]
           [java.nio ByteBuffer]
           [javax.vecmath Matrix4d Point3d Quat4d Vector3d Vector4d]))
