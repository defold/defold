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

(ns editor.gl.texture-test
  (:require [clojure.test :refer [deftest is testing]]
            [editor.gl.texture :as texture]
            [editor.gl.types :as gl.types]
            [editor.scene-cache :as scene-cache]
            [support.gl-support :as gl-support]
            [util.coll :as coll])
  (:import [clojure.lang ExceptionInfo]
           [com.jogamp.opengl GL3]
           [com.jogamp.opengl.util.texture Texture TextureData]
           [editor.gl.texture TextureRequestData]
           [java.nio Buffer ByteBuffer]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(defn- byte-buffer
  ^ByteBuffer [values]
  (let [buffer (ByteBuffer/allocateDirect (count values))]
    (doseq [value values]
      (.put buffer (unchecked-byte value)))
    (.flip buffer)))

(defn- solid-pixels [^long width ^long height color]
  (coll/into-> (range (* width height)) [] (mapcat (constantly color))))

(defn- solid-layer [width height color version mipmap]
  (texture/make-texture-request-data
    (byte-buffer (solid-pixels width height color)) version :rgba width height mipmap))

(defn- wrap-texture-data [^TextureData texture-data version]
  (TextureRequestData. texture-data version (texture/texture-data-topology-hash texture-data)))

(defn- mipmapped-layer [width height level-pixels version]
  (let [^"[Ljava.nio.Buffer;" mipmap-buffers (into-array Buffer (mapv byte-buffer level-pixels))]
    (wrap-texture-data
      (TextureData. nil GL3/GL_RGBA (int width) (int height) 0 GL3/GL_RGBA GL3/GL_UNSIGNED_BYTE
                    false false mipmap-buffers nil)
      version)))

(defn- gl-integer
  ^long [^GL3 gl ^long parameter]
  (let [value (int-array 1)]
    (.glGetIntegerv gl parameter value 0)
    (aget value 0)))

(defn- texture-level-shape [^GL3 gl ^Texture gpu-texture ^long level]
  (.bind gpu-texture gl)
  (mapv (fn [^long parameter]
          (let [value (int-array 1)]
            (.glGetTexLevelParameteriv gl (.getTarget gpu-texture) level parameter value 0)
            (aget value 0)))
        [GL3/GL_TEXTURE_WIDTH GL3/GL_TEXTURE_HEIGHT GL3/GL_TEXTURE_DEPTH]))

(defn- texture-level-pixels [^GL3 gl ^Texture gpu-texture ^long level]
  (let [[width height depth] (texture-level-shape gl gpu-texture level)
        size (* 4 (long width) (long height) (long depth))
        buffer (ByteBuffer/allocateDirect size)
        pack-alignment (gl-integer gl GL3/GL_PACK_ALIGNMENT)]
    (.glPixelStorei gl GL3/GL_PACK_ALIGNMENT 1)
    (try
      (.glGetTexImage gl (.getTarget gpu-texture) level GL3/GL_RGBA GL3/GL_UNSIGNED_BYTE buffer)
      (mapv (fn [^long index] (bit-and 255 (.get buffer index))) (range size))
      (finally
        (.glPixelStorei gl GL3/GL_PACK_ALIGNMENT pack-alignment)))))

(defn- texture-max-level
  ^long [^GL3 gl ^Texture gpu-texture]
  (.bind gpu-texture gl)
  (let [value (int-array 1)]
    (.glGetTexParameteriv gl (.getTarget gpu-texture) GL3/GL_TEXTURE_MAX_LEVEL value 0)
    (aget value 0)))

;; Verifies an array consumes one texture unit, including after rebasing,
;; and rejects malformed requests before they reach OpenGL.
(deftest array-texture-construction-test
  (let [layers [(solid-layer 2 2 [255 0 0 255] 0 false)
                (solid-layer 2 2 [0 255 0 255] 0 false)]
        lifecycle (texture/make-gpu-array-texture ::construction layers 3 {})]
    (is (texture/texture-lifecycle? lifecycle))
    (is (= [3] (:texture-units lifecycle)))
    (is (= :int (coll/primitive-vector-type (:texture-units lifecycle))))
    (is (= [5] (:texture-units (texture/set-base-unit lifecycle 5))))
    (is (thrown? AssertionError (texture/make-gpu-array-texture ::empty [] 0 {})))
    (is (thrown? AssertionError (texture/make-gpu-array-texture ::not-vector (list (first layers)) 0 {})))
    (is (thrown? AssertionError (texture/make-gpu-array-texture ::not-layer [nil] 0 {})))
    (is (thrown? AssertionError (texture/make-gpu-array-texture ::negative-unit layers -1 {})))
    (is (thrown? AssertionError (texture/make-gpu-array-texture ::not-params layers 0 nil)))
    (is (thrown? AssertionError (texture/update-sub-image! lifecycle nil 0 nil :rgba 0 0 1 1)))
    (is (thrown? AssertionError (texture/update-image! lifecycle nil nil :rgba 1 1)))))

;; Verifies incompatible layers and unsupported upload metadata fail at the API
;; boundary, rather than producing incomplete textures or OpenGL errors later.
(deftest array-texture-layer-validation-test
  (let [layer (solid-layer 2 2 [255 0 0 255] 0 false)
        level-0 (solid-pixels 2 2 [255 0 0 255])
        level-1 [0 255 0 255]]
    (doseq [[description layers]
            [["width mismatch" [layer (solid-layer 1 2 [255 0 0 255] 0 false)]]
             ["height mismatch" [layer (solid-layer 2 1 [255 0 0 255] 0 false)]]
             ["generated mipmap mode mismatch" [layer (solid-layer 2 2 [255 0 0 255] 0 true)]]
             ["explicit mip count mismatch" [(mipmapped-layer 2 2 [level-0] 0)
                                             (mipmapped-layer 2 2 [level-0 level-1] 0)]]]]
      (testing description
        (is (thrown? ExceptionInfo (texture/make-gpu-array-texture ::invalid-layers layers 0 {})))))
    (doseq [[description change-data]
            [["nonpositive width" #(.setWidth ^TextureData % 0)]
             ["nonpositive height" #(.setHeight ^TextureData % 0)]
             ["internal format mismatch" #(.setInternalFormat ^TextureData % GL3/GL_RGB)]
             ["pixel format mismatch" #(.setPixelFormat ^TextureData % GL3/GL_RGB)]
             ["pixel type mismatch" #(.setPixelType ^TextureData % GL3/GL_UNSIGNED_INT_8_8_8_8)]
             ["compressed data" #(.setIsDataCompressed ^TextureData % true)]
             ["border" #(.setBorder ^TextureData % 1)]
             ["vertical flip metadata" #(.setMustFlipVertically ^TextureData % true)]]]
      (testing description
        (let [^TextureRequestData other-layer (solid-layer 2 2 [255 0 0 255] 0 false)
              texture-data (.-texture-data other-layer)]
          (change-data texture-data)
          (is (thrown? ExceptionInfo
                       (texture/make-gpu-array-texture
                         ::invalid-metadata [layer (wrap-texture-data texture-data 0)] 0 {}))))))))

;; Verifies hardware limits reject oversized arrays before changing existing
;; storage or GL state, and report the requested dimensions and actual limits.
(deftest array-texture-hardware-limits-test
  (gl-support/with-offscreen-gl
    (let [max-size (gl-integer gl GL3/GL_MAX_TEXTURE_SIZE)
          max-layers (gl-integer gl GL3/GL_MAX_ARRAY_TEXTURE_LAYERS)
          layer (solid-layer 1 1 [17 33 65 255] 0 false)
          lifecycle (texture/make-gpu-array-texture ::limits [layer] 0 {})
          gpu-texture (texture/->texture lifecycle gl 0)
          binding (gl-integer gl GL3/GL_TEXTURE_BINDING_2D_ARRAY)]
      (doseq [[width height layer-count] [[(inc max-size) 1 1]
                                          [1 (inc max-size) 1]
                                          [1 1 (inc max-layers)]]]
        (let [oversized-layer (texture/make-texture-request-data nil 1 :rgba width height false)
              oversized (texture/make-gpu-array-texture ::limits (vec (repeat layer-count oversized-layer)) 0 {})
              error (try
                      (texture/->texture oversized gl 0)
                      nil
                      (catch ExceptionInfo error error))]
          (is (= {:width width :height height :layer-count layer-count
                  :max-texture-size max-size :max-layers max-layers}
                 (ex-data error)))
          (is (= binding (gl-integer gl GL3/GL_TEXTURE_BINDING_2D_ARRAY)))
          (is (= [1 1 1] (texture-level-shape gl gpu-texture 0)))
          (is (= [17 33 65 255] (texture-level-pixels gl gpu-texture 0)))
          (is (= GL3/GL_NO_ERROR (.glGetError gl)))
          (.glBindTexture gl GL3/GL_TEXTURE_2D_ARRAY binding))))))

;; Verifies actual array storage, including one-page arrays and layers beyond
;; the ES2 sampler limit, while preserving row order and alpha bytes.
(deftest array-texture-layer-upload-test
  (gl-support/with-offscreen-gl
    (doseq [layer-count [1 9]]
      (let [pixels-by-layer (mapv (fn [^long layer]
                                    [layer 17 33 0 49 layer 65 127
                                     81 97 layer 191 layer 113 129 255])
                                  (range layer-count))
            layers (mapv #(texture/make-texture-request-data (byte-buffer %) 0 :rgba 2 2 false)
                         pixels-by-layer)
            lifecycle (texture/make-gpu-array-texture [::upload layer-count] layers 0 {})
            ^Texture gpu-texture (texture/->texture lifecycle gl 0)]
        (is (= GL3/GL_TEXTURE_2D_ARRAY (.getTarget gpu-texture)))
        (is (= [2 2 layer-count] (texture-level-shape gl gpu-texture 0)))
        (is (= [2 2] [(.getWidth gpu-texture) (.getHeight gpu-texture)]))
        (is (false? (.getMustFlipVertically gpu-texture)))
        (is (= (coll/into-> pixels-by-layer [] cat) (texture-level-pixels gl gpu-texture 0)))
        (is (= GL3/GL_NO_ERROR (.glGetError gl)))))))

;; Verifies lifecycle binding uses the selected unit and array target, then
;; releases that binding and restores the default active unit on unbind.
(deftest array-texture-binding-test
  (gl-support/with-offscreen-gl
    (let [lifecycle (-> (texture/make-gpu-array-texture
                          ::binding [(solid-layer 1 1 [255 255 255 255] 0 false)] 1 {})
                        (texture/set-base-unit 3))]
      (gl.types/bind! lifecycle gl {})
      (let [^Texture gpu-texture (texture/->texture lifecycle gl 0)]
        (is (= (+ GL3/GL_TEXTURE0 3) (gl-integer gl GL3/GL_ACTIVE_TEXTURE)))
        (is (= (.getTextureObject gpu-texture gl) (gl-integer gl GL3/GL_TEXTURE_BINDING_2D_ARRAY)))
        (.glActiveTexture gl (+ GL3/GL_TEXTURE0 4))
        (is (zero? (gl-integer gl GL3/GL_TEXTURE_BINDING_2D_ARRAY)))
        (gl.types/unbind! lifecycle gl {})
        (is (= GL3/GL_TEXTURE0 (gl-integer gl GL3/GL_ACTIVE_TEXTURE)))
        (.glActiveTexture gl (+ GL3/GL_TEXTURE0 3))
        (is (zero? (gl-integer gl GL3/GL_TEXTURE_BINDING_2D_ARRAY)))
        (.glActiveTexture gl GL3/GL_TEXTURE0)
        (is (= GL3/GL_NO_ERROR (.glGetError gl)))))))

;; Verifies cached uploads react to content versions, dimensions, and layer
;; count, and that dropping the scene context deletes every allocated texture.
(deftest array-texture-cache-lifecycle-test
  (gl-support/with-offscreen-gl
    (let [texture-ids (atom #{})]
      (doseq [[width height colors version]
              [[2 2 [[255 0 0 255] [0 255 0 255]] 0]
               [2 2 [[0 0 255 255] [255 255 0 255]] 1]
               [3 1 [[255 0 255 255] [0 255 255 255]] 1]
               [3 1 [[255 0 255 255] [0 255 255 255] [255 255 255 255]] 1]]]
        (let [layers (mapv #(solid-layer width height % version false) colors)
              lifecycle (texture/make-gpu-array-texture ::cache layers 0 {})
              ^Texture gpu-texture (texture/->texture lifecycle gl 0)
              equal-lifecycle (texture/make-gpu-array-texture
                                ::cache (mapv #(solid-layer width height % version false) colors) 0 {})]
          (swap! texture-ids conj (.getTextureObject gpu-texture gl))
          (is (identical? gpu-texture (texture/->texture equal-lifecycle gl 0)))
          (is (= [width height (count colors)] (texture-level-shape gl gpu-texture 0)))
          (is (= (coll/into-> colors [] (mapcat #(solid-pixels width height %)))
                 (texture-level-pixels gl gpu-texture 0)))))
      (scene-cache/drop-context! gl)
      (doseq [^long texture-id @texture-ids]
        (is (false? (.glIsTexture gl texture-id))))
      (is (= GL3/GL_NO_ERROR (.glGetError gl))))))

;; Verifies explicit mip uploads keep every layer at every level, and changing
;; only mip count invalidates cached storage and clamps the accessible levels.
(deftest array-texture-explicit-mipmap-test
  (gl-support/with-offscreen-gl
    (let [pixels-by-layer [[(solid-pixels 4 2 [255 0 0 255])
                            (solid-pixels 2 1 [0 255 0 255])
                            [0 0 255 255]]
                           [(solid-pixels 4 2 [255 255 0 255])
                            (solid-pixels 2 1 [0 255 255 255])
                            [255 0 255 255]]]]
      (doseq [level-count [1 3 2]]
        (let [layers (mapv #(mipmapped-layer 4 2 (subvec % 0 level-count) 0) pixels-by-layer)
              lifecycle (texture/make-gpu-array-texture ::explicit-mips layers 0 {})
              ^Texture gpu-texture (texture/->texture lifecycle gl 0)]
          (is (= (dec (long level-count)) (texture-max-level gl gpu-texture)))
          (dotimes [level level-count]
            (is (= [(max 1 (bit-shift-right 4 level)) (max 1 (bit-shift-right 2 level)) 2]
                   (texture-level-shape gl gpu-texture level)))
            (is (= (coll/into-> pixels-by-layer [] (mapcat #(nth % level)))
                   (texture-level-pixels gl gpu-texture level))))
          (is (= GL3/GL_NO_ERROR (.glGetError gl))))))))

;; Verifies generated mipmaps contain each layer's pixels and are regenerated
;; when the source version changes, including non-square base images.
(deftest array-texture-generated-mipmap-test
  (gl-support/with-offscreen-gl
    (doseq [[version colors] [[0 [[255 0 0 255] [0 255 0 255]]]
                              [1 [[0 0 255 255] [255 255 0 255]]]]]
      (let [lifecycle (texture/make-gpu-array-texture
                        ::generated-mips (mapv #(solid-layer 4 2 % version true) colors) 0 {})
            ^Texture gpu-texture (texture/->texture lifecycle gl 0)]
        (doseq [[level width height] [[0 4 2] [1 2 1] [2 1 1]]]
          (is (= [width height 2] (texture-level-shape gl gpu-texture level)))
          (is (= (coll/into-> colors [] (mapcat #(solid-pixels width height %)))
                 (texture-level-pixels gl gpu-texture level))))
        (is (= GL3/GL_NO_ERROR (.glGetError gl)))))))

;; Verifies per-layer row padding is honored and external unpack state cannot
;; shift the upload or leak from array creation and subsequent cache updates.
(deftest array-texture-unpack-state-test
  (gl-support/with-offscreen-gl
    (let [unpack-state {GL3/GL_UNPACK_ALIGNMENT 8
                        GL3/GL_UNPACK_ROW_LENGTH 7
                        GL3/GL_UNPACK_IMAGE_HEIGHT 8
                        GL3/GL_UNPACK_SKIP_PIXELS 1
                        GL3/GL_UNPACK_SKIP_ROWS 2
                        GL3/GL_UNPACK_SKIP_IMAGES 3
                        GL3/GL_UNPACK_SWAP_BYTES 1
                        GL3/GL_UNPACK_LSB_FIRST 1}]
      (doseq [[^long parameter ^long value] unpack-state]
        (.glPixelStorei gl parameter value))
      (doseq [version [0 1]]
        (let [^TextureRequestData padded-layer
              (texture/make-texture-request-data
                (byte-buffer [255 0 0 0 255 0 0 0 255 99 99 99
                              255 255 0 0 255 255 255 0 255 99 99 99])
                version :rgb 3 2 false)

              ^TextureData padded-data (.-texture-data padded-layer)

              ^TextureRequestData packed-layer
              (texture/make-texture-request-data
                (byte-buffer [0 0 0 255 255 255 127 127 127
                              0 0 255 0 255 0 255 0 0])
                version :rgb 3 2 false)

              ^TextureData packed-data (.-texture-data packed-layer)]
          (.setAlignment padded-data 4)
          (.setRowLength padded-data 4)
          (.setAlignment packed-data 1)
          (let [lifecycle (texture/make-gpu-array-texture
                            ::unpack [(wrap-texture-data padded-data version)
                                      (wrap-texture-data packed-data version)] 0 {})
                ^Texture gpu-texture (texture/->texture lifecycle gl 0)]
            (doseq [[^long parameter value] unpack-state]
              (is (= value (gl-integer gl parameter))))
            (is (= [255 0 0 255 0 255 0 255 0 0 255 255
                    255 255 0 255 0 255 255 255 255 0 255 255
                    0 0 0 255 255 255 255 255 127 127 127 255
                    0 0 255 255 0 255 0 255 255 0 0 255]
                   (texture-level-pixels gl gpu-texture 0)))
            (is (= GL3/GL_NO_ERROR (.glGetError gl)))))))))

;; Verifies uploads accept client memory even with an unpack buffer bound, and
;; preserve the caller's texture binding, unpack buffer, and active unit.
(deftest array-texture-upload-binding-state-test
  (gl-support/with-offscreen-gl
    (let [sentinel-lifecycle (texture/make-gpu-array-texture
                               ::sentinel [(solid-layer 1 1 [255 0 0 255] 0 false)] 0 {})
          ^Texture sentinel-texture (texture/->texture sentinel-lifecycle gl 0)
          buffer-ids (int-array 1)]
      (.glActiveTexture gl (+ GL3/GL_TEXTURE0 2))
      (.bind sentinel-texture gl)
      (.glGenBuffers gl 1 buffer-ids 0)
      (.glBindBuffer gl GL3/GL_PIXEL_UNPACK_BUFFER (aget buffer-ids 0))
      (.glBufferData gl GL3/GL_PIXEL_UNPACK_BUFFER 128 nil GL3/GL_STATIC_DRAW)
      (try
        (doseq [version [0 1]]
          (let [lifecycle (texture/make-gpu-array-texture
                            ::binding-state [(solid-layer 2 2 [0 255 0 255] version false)] 0 {})]
            (texture/->texture lifecycle gl 0)
            (is (= (+ GL3/GL_TEXTURE0 2) (gl-integer gl GL3/GL_ACTIVE_TEXTURE)))
            (is (= (.getTextureObject sentinel-texture gl) (gl-integer gl GL3/GL_TEXTURE_BINDING_2D_ARRAY)))
            (is (= (aget buffer-ids 0) (gl-integer gl GL3/GL_PIXEL_UNPACK_BUFFER_BINDING)))
            (is (= GL3/GL_NO_ERROR (.glGetError gl)))))
        (finally
          (.glBindBuffer gl GL3/GL_PIXEL_UNPACK_BUFFER 0)
          (.glDeleteBuffers gl 1 buffer-ids 0)
          (.glBindTexture gl GL3/GL_TEXTURE_2D_ARRAY 0)
          (.glActiveTexture gl GL3/GL_TEXTURE0))))))
