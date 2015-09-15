(ns editor.pipeline.font-gen
  (:require [clojure.java.io :as io]
            [editor.protobuf :as protobuf])
  (:import [com.defold.editor.pipeline Fontc]
           [com.dynamo.render.proto Font$FontDesc]
           [java.io ByteArrayOutputStream]))

(defn ->bytes [font-desc font-path]
  (let [^Font$FontDesc font-desc (protobuf/map->pb Font$FontDesc font-desc)
        out (ByteArrayOutputStream. (* 4 1024))]
    (with-open [font-stream (io/input-stream font-path)]
      (doto (Fontc.) (.run font-stream font-desc out))
      (.close out)
      (.toByteArray out))))