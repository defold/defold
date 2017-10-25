(ns editor.pipeline.font-gen
  (:require [clojure.java.io :as io]
            [editor.protobuf :as protobuf])
  (:import [com.defold.editor.pipeline Fontc Fontc$FontResourceResolver]
           [com.dynamo.render.proto Font$FontDesc Font$FontMap]
           [java.awt.image BufferedImage]))

(set! *warn-on-reflection* true)

(defn generate [font-desc font-path resolver]
  (when font-path
    (let [^Font$FontDesc font-desc (protobuf/map->pb Font$FontDesc font-desc)
          font-map-builder (Font$FontMap/newBuilder)
          font-res-resolver (reify Fontc$FontResourceResolver
                              (getResource [this resource-name]
                                (io/input-stream (resolver resource-name))))]
      (with-open [font-stream (io/input-stream font-path)]
        (let [^Font$FontMap font-map (-> (doto (Fontc.)
                                           (.compile font-stream font-desc false font-res-resolver))
                                       (.getFontMap))]
          (protobuf/pb->map font-map))))))
