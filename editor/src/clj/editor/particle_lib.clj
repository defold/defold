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

(ns editor.particle-lib
  (:require [editor.buffers :as buffers]
            [editor.graphics.types :as graphics.types]
            [editor.math :as math]
            [editor.protobuf :as protobuf]
            [util.murmur :as murmur])
  (:import [com.defold.libs ParticleLibrary ParticleLibrary$AnimPlayback ParticleLibrary$AnimationData ParticleLibrary$FetchResourcesCallback ParticleLibrary$FetchResourcesData ParticleLibrary$FetchResourcesParams ParticleLibrary$FetchResourcesResult ParticleLibrary$InstanceStats ParticleLibrary$Quat ParticleLibrary$RenderInstanceCallback ParticleLibrary$Stats ParticleLibrary$Vector3 ParticleLibrary$Vector4 ParticleLibrary$VertexAttributeInfo ParticleLibrary$VertexAttributeInfos]
           [com.dynamo.graphics.proto Graphics$CoordinateSpace]
           [com.dynamo.particle.proto Particle$ParticleFX]
           [com.jogamp.common.nio Buffers]
           [com.sun.jna Pointer]
           [com.sun.jna.ptr IntByReference]
           [java.nio ByteBuffer]
           [javax.vecmath Matrix4d Point3d Quat4d Vector3d]))

(set! *warn-on-reflection* true)

(defn- create-context [max-emitter-count max-particle-count]
  (ParticleLibrary/Particle_CreateContext max-emitter-count max-particle-count))

(defn- update-tile-sources [^Pointer prototype prototype-msg]
  (let [emitter-count (count (:emitters prototype-msg))]
    (doseq [emitter-index (range emitter-count)]
      (ParticleLibrary/Particle_SetTileSource prototype emitter-index (Pointer. (inc emitter-index))))
    prototype))

(defn- new-prototype [prototype-msg]
  (let [^bytes data (protobuf/map->bytes Particle$ParticleFX prototype-msg)]
    (-> (ParticleLibrary/Particle_NewPrototype (ByteBuffer/wrap data) (count data))
      (update-tile-sources prototype-msg))))

(defn- reload-prototype [^Pointer prototype prototype-msg]
  (let [^bytes data (protobuf/map->bytes Particle$ParticleFX prototype-msg)]
    (ParticleLibrary/Particle_ReloadPrototype prototype (ByteBuffer/wrap data) (count data))
    (update-tile-sources prototype prototype-msg)))

(defn- point3d->plib [^Point3d p]
  (ParticleLibrary$Vector3. (.x p) (.y p) (.z p)))

(defn- quat4d->plib [^Quat4d q]
  (ParticleLibrary$Quat. (.x q) (.y q) (.z q) (.w q)))

(defn- set-instance-transform [^Pointer context ^Pointer instance ^Matrix4d transform]
  (let [position (Point3d.)
        rotation (Quat4d.)
        scale (Vector3d.)
        _ (math/split-mat4 transform position rotation scale)
        ; Corresponds to how uniform scale is computed in the engine
        min-scale (min (.x scale) (.y scale) (.z scale))]
    (ParticleLibrary/Particle_SetPosition context instance (point3d->plib position))
    (ParticleLibrary/Particle_SetRotation context instance (quat4d->plib rotation))
    (ParticleLibrary/Particle_SetScale context instance min-scale)
    instance))

(def ^:private playback-map
  {:playback-none ParticleLibrary$AnimPlayback/ANIM_PLAYBACK_NONE
   :playback-once-forward ParticleLibrary$AnimPlayback/ANIM_PLAYBACK_ONCE_FORWARD
   :playback-once-backward ParticleLibrary$AnimPlayback/ANIM_PLAYBACK_ONCE_BACKWARD
   :playback-once-pingpong ParticleLibrary$AnimPlayback/ANIM_PLAYBACK_ONCE_PINGPONG
   :playback-loop-forward ParticleLibrary$AnimPlayback/ANIM_PLAYBACK_LOOP_FORWARD
   :playback-loop-backward ParticleLibrary$AnimPlayback/ANIM_PLAYBACK_LOOP_BACKWARD
   :playback-loop-pingpong ParticleLibrary$AnimPlayback/ANIM_PLAYBACK_LOOP_PINGPONG})

(defn- fetch-resources [^ParticleLibrary$FetchResourcesParams params ^ParticleLibrary$FetchResourcesData out-data fetch-anim-fn]
  (let [emitter-index (.emitterIndex params)
        data (fetch-anim-fn emitter-index)]
    (if (or (nil? data) (nil? (:texture-set-anim data)))
      ParticleLibrary$FetchResourcesResult/FETCH_RESOURCES_NOT_FOUND
      (let [anim (:texture-set-anim data)
            ^ParticleLibrary$AnimationData anim-out (.animationData out-data)]
        (assert (= (.animation params) (ParticleLibrary/Particle_Hash (:id anim))) "Animation id does not match")
        (set! (. anim-out texture) (Pointer. (inc emitter-index)))
        (set! (. anim-out texCoords) (.asFloatBuffer ^ByteBuffer (:tex-coords data)))
        (when-let [tex-dims ^ByteBuffer (:tex-dims data)]
          (set! (. anim-out texDims) (.asFloatBuffer tex-dims)))
        (set! (. anim-out pageIndices) (.asIntBuffer ^ByteBuffer (:page-indices data)))
        (set! (. anim-out frameIndices) (.asIntBuffer ^ByteBuffer (:frame-indices data)))
        (set! (. anim-out playback) (get playback-map (:playback anim)))
        (set! (. anim-out tileWidth) (int (:width anim)))
        (set! (. anim-out tileHeight) (int (:height anim)))
        (set! (. anim-out startTile) (:start anim))
        (set! (. anim-out endTile) (:end anim))
        (set! (. anim-out fps) (:fps anim))
        (set! (. anim-out hFlip) (:flip-horizontal anim))
        (set! (. anim-out vFlip) (:flip-vertical anim))
        (set! (. anim-out structSize) (.size anim-out))
        (set! (. out-data material) Pointer/NULL)
        ParticleLibrary$FetchResourcesResult/FETCH_RESOURCES_OK))))

(defn- create-instance [^Pointer context ^Pointer prototype ^Pointer emitter-state-callback-data ^Matrix4d transform]
  (let [^Pointer instance (ParticleLibrary/Particle_CreateInstance context prototype emitter-state-callback-data)]
    (set-instance-transform context instance transform)))

(defn- make-byte-buffer [size]
  (Buffers/newDirectByteBuffer ^int size))
(defn- make-raw-vbuf [max-particle-count vertex-size]
  (make-byte-buffer (ParticleLibrary/Particle_GetVertexBufferSize max-particle-count vertex-size)))

(defn make-sim [max-emitter-count max-particle-count prototype-msg instance-transforms]
  (let [context (create-context max-emitter-count max-particle-count)
        prototype (new-prototype prototype-msg)
        instances (mapv (fn [^Matrix4d t] (create-instance context prototype nil t)) instance-transforms)]
    {:context context
     :prototype prototype
     :instances instances
     :raw-vbufs (vec (repeat max-emitter-count nil))
     :elapsed-time 0}))

(defn destroy-sim [sim]
  (let [context (:context sim)]
    (doseq [instance (:instances sim)]
      (ParticleLibrary/Particle_DestroyInstance context instance))
    (ParticleLibrary/Particle_DestroyContext context)
    (ParticleLibrary/Particle_DeletePrototype (:prototype sim))))

(defn- reset [sim]
  (doseq [instance (:instances sim)
          :let [context (:context sim)]]
    (ParticleLibrary/Particle_ResetInstance context instance))
  (assoc sim :elapsed-time 0))

(defn sleeping? [sim]
  (let [context (:context sim)]
    (reduce (fn [v instance] (and v (ParticleLibrary/Particle_IsSleeping context instance))) true (:instances sim))))

(defn start [sim]
  (doseq [instance (:instances sim)
          :let [context (:context sim)]]
    (ParticleLibrary/Particle_StartInstance context instance))
  sim)


(defn simulate [sim dt fetch-anim-fn instance-transforms]
  (let [anim-callback (reify ParticleLibrary$FetchResourcesCallback
                        (invoke [_ params out-data]
                          (fetch-resources params out-data fetch-anim-fn)))
        sim (if (sleeping? sim)
              (-> sim
                (reset)
                (start))
              sim)
        context (:context sim)]
    (doseq [[instance transform] (map vector (:instances sim) instance-transforms)]
      (set-instance-transform context instance transform))
    (ParticleLibrary/Particle_Update context dt anim-callback)
    (-> sim
      (assoc :last-dt dt)
      (update :elapsed-time #(+ % dt)))))

(defn- attribute-name-key->byte-buffer ^ByteBuffer [name-key vertex-attribute-bytes]
  (when-let [attribute-bytes (get vertex-attribute-bytes name-key)]
    (buffers/wrap-byte-array attribute-bytes :byte-order/native)))

(defn- coordinate-space->int
  ^long [coordinate-space]
  (case coordinate-space
    (:coordinate-space-world :coordinate-space-local) (graphics.types/coordinate-space-pb-int coordinate-space)
    Graphics$CoordinateSpace/COORDINATE_SPACE_LOCAL_VALUE))

(defn- attribute-info->particle-attribute-info [^Pointer context attribute-info vertex-attribute-bytes]
  (let [attribute-name-hash (murmur/hash64 (:name attribute-info))
        attribute-semantic-type (graphics.types/semantic-type-pb-int (:semantic-type attribute-info))
        attribute-coordinate-space (coordinate-space->int (:coordinate-space attribute-info))
        attribute-data-type (graphics.types/data-type-pb-int (:data-type attribute-info))
        attribute-vector-type (graphics.types/vector-type-pb-int (:vector-type attribute-info))
        attribute-step-function (graphics.types/vertex-step-function-pb-int (:step-function attribute-info))
        attribute-bytes (attribute-name-key->byte-buffer (:name-key attribute-info) vertex-attribute-bytes)
        attribute-bytes-count (if (nil? attribute-bytes)
                                0
                                (.capacity attribute-bytes))
        particle-attribute-info (ParticleLibrary$VertexAttributeInfo.)
        context-attribute-scratch-ptr (ParticleLibrary/Particle_WriteAttributeToScratchBuffer context attribute-bytes attribute-bytes-count)]
    (set! (. particle-attribute-info nameHash) attribute-name-hash)
    (set! (. particle-attribute-info semanticType) attribute-semantic-type)
    (set! (. particle-attribute-info dataType) attribute-data-type)
    (set! (. particle-attribute-info vectorType) attribute-vector-type)
    (set! (. particle-attribute-info stepFunction) attribute-step-function)
    (set! (. particle-attribute-info coordinateSpace) attribute-coordinate-space)
    (set! (. particle-attribute-info valuePtr) context-attribute-scratch-ptr)
    (set! (. particle-attribute-info valueVectorType) attribute-vector-type)
    (set! (. particle-attribute-info normalize) (boolean (:normalize attribute-info)))
    particle-attribute-info))

(defn- make-particle-attribute-infos [^Pointer context vertex-description vertex-attribute-bytes]
  (let [vertex-stride (:size vertex-description)
        attribute-infos (:attributes vertex-description)
        infos (ParticleLibrary$VertexAttributeInfos.)
        num-attribute-infos (count attribute-infos)]
    (ParticleLibrary/Particle_ResetAttributeScratchBuffer context)
    (doseq [i (range num-attribute-infos)]
      (aset (.infos infos) i (attribute-info->particle-attribute-info context (get attribute-infos i) vertex-attribute-bytes)))
    (set! (. infos vertexStride) (int vertex-stride))
    (set! (. infos numInfos) (int num-attribute-infos))
    infos))

(defn- emitter-vertex-data [sim emitter-index max-particle-count vertex-description]
  (or (get (:raw-vbufs sim) emitter-index)
      (make-raw-vbuf max-particle-count (:size vertex-description))))

(defn gen-emitter-vertex-data [sim emitter-index color max-particle-count vertex-description vertex-attribute-bytes]
  (when-let [raw-vbuf ^ByteBuffer (emitter-vertex-data sim emitter-index max-particle-count vertex-description)]
    (let [context (:context sim)
          dt (:last-dt sim)
          out-size (IntByReference. 0)
          [r g b a] color
          instances (:instances sim)
          particle-attribute-infos (make-particle-attribute-infos context vertex-description vertex-attribute-bytes)]
      (assert (= 1 (count instances)))
      (.position raw-vbuf 0)
      (.limit raw-vbuf (.capacity raw-vbuf))
      (ParticleLibrary/Particle_GenerateVertexData context dt (first instances) emitter-index particle-attribute-infos (ParticleLibrary$Vector4. r g b a) raw-vbuf (.capacity raw-vbuf) out-size)
      (.position raw-vbuf (.getValue out-size))
      (.flip raw-vbuf)
      raw-vbuf)))

(defn render-emitter [sim emitter-index]
  (let [context (:context sim)
        render-data (atom nil)
        callback (reify ParticleLibrary$RenderInstanceCallback
                   (invoke [this user-context material texture world-transform blend-mode v-index v-count constants constant-count]
                     (assert (not @render-data))
                     (reset! render-data {:texture (dec (int (Pointer/nativeValue texture)))
                                          :blend-mode blend-mode
                                          :v-index v-index
                                          :v-count v-count})))
        instances (:instances sim)]
    (assert (= 1 (count instances)))
    (when-not (ParticleLibrary/Particle_IsSleeping context (first instances))
      (ParticleLibrary/Particle_RenderEmitter context (first instances) emitter-index (Pointer. 0) callback)
      @render-data)))

(defn stats [sim]
  (let [context (:context sim)
        stats (ParticleLibrary$Stats.)
        instance-stats (ParticleLibrary$InstanceStats.)]
    (ParticleLibrary/Particle_GetStats context stats)
    (ParticleLibrary/Particle_GetInstanceStats context (first (:instances sim)) instance-stats)
    {:particles (.particles stats)
     :max-particles (.maxParticles stats)
     :time (.time instance-stats)}))

(defn reload [sim prototype-msg max-particle-count]
  (reload-prototype (:prototype sim) prototype-msg)
  (let [context (:context sim)]
    (doseq [instance (:instances sim)]
      (ParticleLibrary/Particle_ReloadInstance context instance true))
    (ParticleLibrary/Particle_SetContextMaxParticleCount context ^int max-particle-count))
  (assoc sim :raw-vbufs (vec (repeat (count (:emitters prototype-msg)) nil))))
