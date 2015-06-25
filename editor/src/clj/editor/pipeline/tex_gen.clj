(ns editor.pipeline.tex-gen
  (:require [editor.protobuf :as protobuf])
  (:import [com.defold.editor.pipeline TextureGenerator]
           [com.dynamo.graphics.proto Graphics$TextureImage Graphics$TextureProfile]
           [java.awt.image BufferedImage]))

(defn ->bytes [^BufferedImage image texture-profile]
  (let [texture-profile (protobuf/map->pb Graphics$TextureProfile texture-profile)
        texture-image (TextureGenerator/generate ^BufferedImage image ^Graphics$TextureProfile texture-profile)]
    (protobuf/pb->bytes texture-image)))
