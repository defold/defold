;; Copyright 2020-2026 The Defold Foundation
;; Copyright 2014-2020 King
;; Copyright 2009-2014 Ragnar Svensson, Christian Murray
;; Licensed under the Defold License version 1.0 (the "License"); you may not use
;; this file except in compliance with the License.
;;
;; You may obtain a copy of the License, together with FAQs at
;; https://www.defold.com/license
;;
;; Unless required by applicable law or agreed to in writing, software distributed
;; under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
;; CONDITIONS OF ANY KIND, either express or implied. See the License for the
;; specific language governing permissions and limitations under the License.

(ns editor.pipeline.font-gen
  (:require [clojure.java.io :as io]
            [editor.pipeline.fontc :as fontc]
            [editor.texture.engine :refer [compress-rgba-buffer]])
  (:import [com.google.protobuf ByteString]
           [java.nio ByteBuffer]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(defn- make-input-stream-resolver [resource-resolver]
  (fn [resource-path]
    (io/input-stream (resource-resolver resource-path))))

(defn generate [font-desc font-resource resolver]
  (when font-resource
    (fontc/compile-font font-desc font-resource (make-input-stream-resolver resolver))))

(defn- data-size
  ^long [^ByteBuffer byte-buffer]
  (if (nil? byte-buffer)
    0
    (- (.limit byte-buffer)
       (.position byte-buffer))))

(defn- concat-byte-buffers
  ^ByteBuffer [byte-buffers]
  (let [combined-size (transduce (map data-size) + byte-buffers)
        backing-array (byte-array combined-size)
        combined-buffer (ByteBuffer/wrap backing-array)]
    (doseq [^ByteBuffer buffer byte-buffers
            :when (some? buffer)
            :let [position (.position buffer)]]
      (.put combined-buffer buffer) ; Mutates source buffer position.
      (.position buffer position))
    (.flip combined-buffer)))

(defn compress [{:keys [^long glyph-channels ^ByteString glyph-data glyphs] :as font-map}]
  ;; NOTE: The compression is not parallelized over multiple threads here since
  ;; this is currently called only during the build process, which already runs
  ;; build functions on several threads.
  (let [uncompressed-data-source
        (-> (ByteBuffer/allocateDirect (.size glyph-data))
            (.put (.asReadOnlyByteBuffer glyph-data))
            (.flip))

        glyph-byte-buffers
        (map (fn [{:keys [^long glyph-data-offset ^long glyph-data-size] :as glyph}]
               (when (pos? glyph-data-size)
                 (let [{:keys [^int width ^int height]} (:glyph-cell-wh glyph)
                       glyph-data-end (+ glyph-data-offset glyph-data-size)]
                   (.limit uncompressed-data-source glyph-data-end)
                   (.position uncompressed-data-source glyph-data-offset)
                   (compress-rgba-buffer uncompressed-data-source width height glyph-channels))))
             glyphs)

        glyph-data-sizes (map data-size glyph-byte-buffers)
        glyph-data-offsets (reductions + (cons 0 glyph-data-sizes))
        glyph-data-byte-buffer (concat-byte-buffers glyph-byte-buffers)]

    (assoc font-map
      :glyph-data (ByteString/copyFrom glyph-data-byte-buffer)
      :glyphs (map (fn [glyph glyph-data-offset glyph-data-size]
                     (assoc glyph
                       :glyph-data-offset glyph-data-offset
                       :glyph-data-size glyph-data-size))
                   glyphs
                   glyph-data-offsets
                   glyph-data-sizes))))
