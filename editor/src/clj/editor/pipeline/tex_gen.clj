(ns editor.pipeline.tex-gen
  (:require [editor.protobuf :as protobuf])
  (:import [com.defold.editor.pipeline TextureGenerator]
           [com.dynamo.graphics.proto Graphics$TextureImage Graphics$TextureProfile]
           [java.awt.image BufferedImage]))

(set! *warn-on-reflection* true)

(defn ->bytes [^BufferedImage image texture-profile]
  (let [texture-profile (protobuf/map->pb Graphics$TextureProfile texture-profile)
  		; the trailing boolean in the call below should come from the "Enable Texture Profiles" checkbox
  		; texture profiles does not seem to be applied at all when build+launch, and checkbox has no effect when bundling
  		; (bundling always uses texture profiles)
        texture-image (TextureGenerator/generate ^BufferedImage image ^Graphics$TextureProfile texture-profile true)]
    (protobuf/pb->bytes texture-image)))
