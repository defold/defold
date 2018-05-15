(ns editor.pipeline.font-gen
  (:require [clojure.java.io :as io]
            [editor.protobuf :as protobuf]
            [editor.pipeline.fontc :as fontc])
  #_(:import [com.defold.editor.pipeline Fontc Fontc$FontResourceResolver]
            [com.dynamo.render.proto Font$FontDesc Font$FontMap]))

(set! *warn-on-reflection* true)

(defn- make-input-stream-resolver [resource-resolver]
  (fn [resource-path]
    (io/input-stream (resource-resolver resource-path))))

(defn generate [font-desc font-resource resolver]
  (when font-resource
    (with-open [font-stream (io/input-stream font-resource)]
      (fontc/compile-font font-desc font-resource (make-input-stream-resolver resolver))
      #_(let [^Font$FontDesc pb-font-desc (protobuf/map->pb Font$FontDesc font-desc)
            font-res-resolver (reify Fontc$FontResourceResolver
                                (getResource [this resource-name]
                                  (io/input-stream (resolver resource-name))))
            fontc (doto (Fontc.)
                    (.compile font-stream pb-font-desc false font-res-resolver))
            ^Font$FontMap font-map (.getFontMap fontc)]
        (protobuf/pb->map font-map)))))
