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

(ns editor.rig-lib
  (:require [editor.buffers :as buffers]
            [editor.graphics.types :as graphics.types]
            [editor.protobuf :as protobuf]
            [util.murmur :as murmur])
  (:import [com.defold.libs RigLibrary RigLibrary$Matrix4 RigLibrary$VertexAttributeInfo RigLibrary$VertexAttributeInfos]
           [com.dynamo.graphics.proto Graphics$CoordinateSpace]
           [com.dynamo.rig.proto Rig$AnimationSet Rig$MeshSet Rig$Skeleton]
           [com.jogamp.common.nio Buffers]
           [com.jogamp.opengl GL GL2]
           [com.sun.jna Native Pointer Structure]
           [com.sun.jna.ptr IntByReference]
           [java.nio ByteBuffer FloatBuffer]
           [javax.vecmath Matrix4d Vector4d]))

(set! *warn-on-reflection* true)

(def ^:private null-buffer (ByteBuffer/allocateDirect 0))
(def ^:private invalid-pose-matrix-cache-entry 0xFFFF)
(def ^:private default-pose-cache-width 256)
(def ^:private default-pose-cache-height 256)

(defn- direct-bytes ^ByteBuffer [pb-class pb-msg]
  (if (seq pb-msg)
    (ByteBuffer/wrap ^bytes (protobuf/map->bytes pb-class pb-msg))
    null-buffer))

(defn make-sim [skeleton mesh-set animation-set animation-id]
  (let [skeleton-bytes (direct-bytes Rig$Skeleton skeleton)
        mesh-set-bytes (direct-bytes Rig$MeshSet mesh-set)
        animation-set-bytes (direct-bytes Rig$AnimationSet animation-set)
        context (RigLibrary/Rig_CreatePreview skeleton-bytes (.capacity skeleton-bytes)
                                             mesh-set-bytes (.capacity mesh-set-bytes)
                                             animation-set-bytes (.capacity animation-set-bytes))]
    (when (nil? context)
      (throw (ex-info "Unable to create rig preview context." {})))
    {:context context
     :animation-id (some-> animation-id RigLibrary/Rig_Hash)
     :started? false
     :raw-vbufs {}
     :pose-cache-buffer nil
     :pose-cache-texture-id 0
     :pose-cache-width default-pose-cache-width
     :pose-cache-height default-pose-cache-height}))

(defn destroy-sim [sim]
  (when-let [^Pointer context (:context sim)]
    (RigLibrary/Rig_DestroyPreview context)))

(defn simulate [sim dt]
  (let [^Pointer context (:context sim)
        dt (float dt)
        sim (if (and (pos? dt) (not (:started? sim)) (:animation-id sim))
              (do
                (RigLibrary/Rig_PlayAnimation context (:animation-id sim))
                (assoc sim :started? true))
              sim)]
    (RigLibrary/Rig_ResetPoseMatrixCache context)
    (when (pos? (RigLibrary/Rig_GetBoneCount context))
      (RigLibrary/Rig_AcquirePoseMatrixCacheEntry context))
    (RigLibrary/Rig_Update context dt)
    (assoc sim :last-dt dt)))

(defn animated-pose? [sim]
  (RigLibrary/Rig_HasPoseMatrixCacheAnimatedPose (:context sim)))

(defn pose-cache-animation-data [sim]
  (let [^Pointer context (:context sim)
        bone-count (RigLibrary/Rig_GetBoneCount context)
        cache-offset (RigLibrary/Rig_GetPoseMatrixCacheDataOffset context)]
    (if (and (pos? bone-count)
             (not= invalid-pose-matrix-cache-entry cache-offset)
             (RigLibrary/Rig_HasPoseMatrixCacheAnimatedPose context))
      (Vector4d. (* 3.0 cache-offset)
                 bone-count
                 (:pose-cache-width sim)
                 (:pose-cache-height sim))
      (Vector4d. 0.0 0.0 0.0 0.0))))

(defn- ensure-pose-cache-buffer [sim]
  (let [float-count (* (:pose-cache-width sim) (:pose-cache-height sim) 4)]
    (if (= float-count (some-> ^FloatBuffer (:pose-cache-buffer sim) .capacity))
      sim
      (assoc sim :pose-cache-buffer (Buffers/newDirectFloatBuffer ^int float-count)))))

(defn update-pose-cache-texture! [^GL2 gl sim]
  (let [sim (ensure-pose-cache-buffer sim)
        ^FloatBuffer buffer (:pose-cache-buffer sim)
        width (int (:pose-cache-width sim))
        height (int (:pose-cache-height sim))
        texture-id (int (:pose-cache-texture-id sim))
        texture-id (if (zero? texture-id)
                     (let [ids (int-array 1)]
                       (.glGenTextures gl 1 ids 0)
                       (aget ids 0))
                     texture-id)]
    (.clear buffer)
    (RigLibrary/Rig_WritePoseMatrixCache (:context sim) buffer (* width height))
    (.position buffer 0)
    (.limit buffer (.capacity buffer))
    (.glActiveTexture gl (+ GL/GL_TEXTURE0 15))
    (.glBindTexture gl GL/GL_TEXTURE_2D texture-id)
    (.glTexParameteri gl GL/GL_TEXTURE_2D GL/GL_TEXTURE_MIN_FILTER GL/GL_NEAREST)
    (.glTexParameteri gl GL/GL_TEXTURE_2D GL/GL_TEXTURE_MAG_FILTER GL/GL_NEAREST)
    (.glTexParameteri gl GL/GL_TEXTURE_2D GL/GL_TEXTURE_WRAP_S GL/GL_CLAMP_TO_EDGE)
    (.glTexParameteri gl GL/GL_TEXTURE_2D GL/GL_TEXTURE_WRAP_T GL/GL_CLAMP_TO_EDGE)
    (.glTexImage2D gl GL/GL_TEXTURE_2D 0 GL2/GL_RGBA32F width height 0 GL/GL_RGBA GL/GL_FLOAT buffer)
    (assoc sim :pose-cache-buffer buffer :pose-cache-texture-id texture-id)))

(defn model-transform [sim model-index use-bone-transform]
  (let [buffer (Buffers/newDirectFloatBuffer 16)]
    (when (RigLibrary/Rig_GetModelMatrix (:context sim) model-index (boolean use-bone-transform) buffer)
      (.position buffer 0)
      (let [m (Matrix4d.)]
        (set! (.m00 m) (.get buffer 0))
        (set! (.m10 m) (.get buffer 1))
        (set! (.m20 m) (.get buffer 2))
        (set! (.m30 m) (.get buffer 3))
        (set! (.m01 m) (.get buffer 4))
        (set! (.m11 m) (.get buffer 5))
        (set! (.m21 m) (.get buffer 6))
        (set! (.m31 m) (.get buffer 7))
        (set! (.m02 m) (.get buffer 8))
        (set! (.m12 m) (.get buffer 9))
        (set! (.m22 m) (.get buffer 10))
        (set! (.m32 m) (.get buffer 11))
        (set! (.m03 m) (.get buffer 12))
        (set! (.m13 m) (.get buffer 13))
        (set! (.m23 m) (.get buffer 14))
        (set! (.m33 m) (.get buffer 15))
        m))))

(defn- attribute-name-key->byte-buffer ^ByteBuffer [name-key vertex-attribute-bytes]
  (when-let [attribute-bytes (get vertex-attribute-bytes name-key)]
    (doto (Buffers/newDirectByteBuffer ^int (count attribute-bytes))
      (.put ^bytes attribute-bytes)
      (.flip))))

(defn- coordinate-space->int
  ^long [coordinate-space]
  (case coordinate-space
    (:coordinate-space-world :coordinate-space-local) (graphics.types/coordinate-space-pb-int coordinate-space)
    Graphics$CoordinateSpace/COORDINATE_SPACE_LOCAL_VALUE))

(defn- attribute-info->rig-attribute-info ^RigLibrary$VertexAttributeInfo [^RigLibrary$VertexAttributeInfo rig-attribute-info attribute-info vertex-attribute-bytes scratch-buffers]
  (let [attribute-name-hash (murmur/hash64 (:name attribute-info))
        attribute-semantic-type (graphics.types/semantic-type-pb-int (:semantic-type attribute-info))
        attribute-coordinate-space (coordinate-space->int (:coordinate-space attribute-info))
        attribute-data-type (graphics.types/data-type-pb-int (:data-type attribute-info))
        attribute-vector-type (graphics.types/vector-type-pb-int (:vector-type attribute-info))
        attribute-step-function (graphics.types/vertex-step-function-pb-int (:step-function attribute-info))
        attribute-bytes (attribute-name-key->byte-buffer (:name-key attribute-info) vertex-attribute-bytes)
        element-count (graphics.types/vector-type-component-count (:vector-type attribute-info))]
    (when attribute-bytes
      (swap! scratch-buffers conj attribute-bytes))
    (set! (. rig-attribute-info nameHash) attribute-name-hash)
    (set! (. rig-attribute-info semanticType) attribute-semantic-type)
    (set! (. rig-attribute-info dataType) attribute-data-type)
    (set! (. rig-attribute-info vectorType) attribute-vector-type)
    (set! (. rig-attribute-info stepFunction) attribute-step-function)
    (set! (. rig-attribute-info coordinateSpace) attribute-coordinate-space)
    (set! (. rig-attribute-info valuePtr) (if attribute-bytes
                                            (Native/getDirectBufferPointer attribute-bytes)
                                            Pointer/NULL))
    (set! (. rig-attribute-info valueVectorType) attribute-vector-type)
    (set! (. rig-attribute-info elementCount) (int element-count))
    (set! (. rig-attribute-info normalize) (boolean (:normalize attribute-info)))
    rig-attribute-info))

(defn- make-rig-attribute-infos [vertex-description vertex-attribute-bytes]
  (let [vertex-stride (:size vertex-description)
        attribute-infos (:attributes vertex-description)
        num-attribute-infos (count attribute-infos)
        first-rig-attribute-info (RigLibrary$VertexAttributeInfo.)
        rig-attribute-info-array (.toArray first-rig-attribute-info num-attribute-infos)
        infos (RigLibrary$VertexAttributeInfos.)
        scratch-buffers (atom [])]
    (doseq [i (range num-attribute-infos)]
      (attribute-info->rig-attribute-info (aget ^objects rig-attribute-info-array i)
                                          (get attribute-infos i)
                                          vertex-attribute-bytes
                                          scratch-buffers))
    (doseq [i (range num-attribute-infos)]
      (.write ^Structure (aget ^objects rig-attribute-info-array i)))
    (set! (. infos infos) (.getPointer ^RigLibrary$VertexAttributeInfo (aget ^objects rig-attribute-info-array 0)))
    (set! (. infos numInfos) (int num-attribute-infos))
    (set! (. infos vertexStride) (int vertex-stride))
    [infos @scratch-buffers]))

(defn- ensure-raw-vbuf [sim request-id byte-capacity]
  (let [^ByteBuffer raw-vbuf (get (:raw-vbufs sim) request-id)]
    (if (and raw-vbuf (= byte-capacity (.capacity raw-vbuf)))
      raw-vbuf
      (Buffers/newDirectByteBuffer ^int byte-capacity))))

(defn gen-world-vertex-data [sim request-id model-index mesh-index world-transform normal-transform vertex-description vertex-attribute-bytes]
  (let [^Pointer context (:context sim)
        vertex-count (RigLibrary/Rig_GetDeindexedVertexCount context model-index mesh-index)
        byte-capacity (* vertex-count (:size vertex-description))]
    (when (pos? byte-capacity)
      (let [^ByteBuffer raw-vbuf (ensure-raw-vbuf sim request-id byte-capacity)
            out-size (IntByReference. 0)
            [rig-attribute-infos _scratch-buffers] (make-rig-attribute-infos vertex-description vertex-attribute-bytes)]
        (.position raw-vbuf 0)
        (.limit raw-vbuf (.capacity raw-vbuf))
        (when (zero? (RigLibrary/Rig_GenerateVertexData context model-index mesh-index
                                                         (RigLibrary$Matrix4. ^Matrix4d world-transform)
                                                         (RigLibrary$Matrix4. ^Matrix4d normal-transform)
                                                         rig-attribute-infos
                                                         raw-vbuf (.capacity raw-vbuf) out-size))
          (.position raw-vbuf (.getValue out-size))
          (.flip raw-vbuf)
          [raw-vbuf (assoc-in sim [:raw-vbufs request-id] raw-vbuf)])))))
