(ns editor.particle-lib
  (:require [dynamo.graph :as g]
            [editor.math :as math]
            [editor.protobuf :as protobuf]
            [editor.gl.vertex :as vertex]
            [editor.gl.shader :as shader])
  (:import [com.dynamo.particle.proto Particle$ParticleFX]
           [com.defold.libs ParticleLibrary ParticleLibrary$Vector3 ParticleLibrary$Vector4 ParticleLibrary$Quat
            ParticleLibrary$AnimationData ParticleLibrary$FetchAnimationCallback
            ParticleLibrary$FetchAnimationResult ParticleLibrary$AnimPlayback
            ParticleLibrary$Stats ParticleLibrary$InstanceStats
            ParticleLibrary$RenderInstanceCallback]
           [com.sun.jna Pointer]
           [com.sun.jna.ptr IntByReference]
           [com.jogamp.common.nio Buffers]
           [java.nio ByteBuffer]
           [javax.vecmath Point3d Quat4d Vector3d Matrix4d]
           [com.google.protobuf Message]))

(set! *warn-on-reflection* true)

(vertex/defvertex vertex-format
  (vec3 position)
  (vec4.ubyte color true)
  (vec2.ushort texcoord0 true))

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

(defrecord FetchAnimCallback [fetch-anim-fn]
 ParticleLibrary$FetchAnimationCallback
 (invoke [self tile-source hash out-data]
   (let [index (int (Pointer/nativeValue tile-source))
         data (fetch-anim-fn (dec index))]
     (if (or (nil? data) (nil? (:texture-set-anim data)))
       ParticleLibrary$FetchAnimationResult/FETCH_ANIMATION_NOT_FOUND
       (let [anim (:texture-set-anim data)]
         (do
           (assert (= hash (ParticleLibrary/Particle_Hash (:id anim))) "Animation id does not match")
           (set! (. out-data texture) (Pointer. index))
           (set! (. out-data texCoords) (.asFloatBuffer ^ByteBuffer (:tex-coords data)))
           (set! (. out-data playback) (get playback-map (:playback anim)))
           (set! (. out-data tileWidth) (int (:width anim)))
           (set! (. out-data tileHeight) (int (:height anim)))
           (set! (. out-data startTile) (:start anim))
           (set! (. out-data endTile) (:end anim))
           (set! (. out-data fps) (:fps anim))
           (set! (. out-data hFlip) (:flip-horizontal anim))
           (set! (. out-data vFlip) (:flip-vertical anim))
           (set! (. out-data structSize) (.size out-data))
           ParticleLibrary$FetchAnimationResult/FETCH_ANIMATION_OK))))))

(defn- create-instance [^Pointer context ^Pointer prototype ^Pointer emitter-state-callback-data ^Matrix4d transform]
  (let [^Pointer instance (ParticleLibrary/Particle_CreateInstance context prototype emitter-state-callback-data)]
    (set-instance-transform context instance transform)))

(defn make-sim [max-emitter-count max-particle-count prototype-msg instance-transforms]
  (let [context (create-context max-emitter-count max-particle-count)
        prototype (new-prototype prototype-msg)
        instances (mapv (fn [^Matrix4d t] (create-instance context prototype nil t)) instance-transforms)
        raw-vbuf (Buffers/newDirectByteBuffer (ParticleLibrary/Particle_GetVertexBufferSize max-particle-count 0))
        vbuf (vertex/vertex-overlay vertex-format raw-vbuf)]
    {:context context
     :prototype prototype
     :emitter-count (count (:emitters prototype-msg))
     :instances instances
     :raw-vbuf raw-vbuf
     :vbuf vbuf
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
  (let [anim-callback (FetchAnimCallback. fetch-anim-fn)
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

(defn gen-vertex-data [sim color]
  (let [context (:context sim)
        dt (:last-dt sim)
        ^ByteBuffer raw-vbuf (:raw-vbuf sim)
        out-size (IntByReference. 0)
        [r g b a] color]
    (doseq [instance (:instances sim)
            emitter-index (range (:emitter-count sim))]
      (ParticleLibrary/Particle_GenerateVertexData context dt instance emitter-index (ParticleLibrary$Vector4. r g b a) raw-vbuf (.capacity raw-vbuf) out-size 0))
    (.limit raw-vbuf (.getValue out-size))
    (let [vbuf (vertex/vertex-overlay vertex-format raw-vbuf)]
      (assoc sim :vbuf vbuf))))

(defn render [sim render-fn]
  (let [context (:context sim)
        render-data (atom [])
        callback (reify ParticleLibrary$RenderInstanceCallback
                   (invoke [this user-context material texture world-transform blend-mode v-index v-count constants constant-count]
                     (swap! render-data conj {:texture (dec (int (Pointer/nativeValue texture)))
                                              :blend-mode blend-mode
                                              :v-index v-index
                                              :v-count v-count})))]
    (doseq [instance (:instances sim)
            emitter-index (range (:emitter-count sim))]
      (when-not (ParticleLibrary/Particle_IsSleeping context instance)
        (ParticleLibrary/Particle_RenderEmitter context instance emitter-index (Pointer. 0) callback)))
    (let [gpu-textures (:gpu-textures sim)]
      (doseq [data @render-data
              :let [texture-index (:texture data)
                    blend-mode (:blend-mode data)
                    v-index (:v-index data)
                    v-count (:v-count data)]]
        (render-fn texture-index blend-mode v-index v-count)))))

(defn stats [sim]
  (let [context (:context sim)
        stats (ParticleLibrary$Stats.)
        instance-stats (ParticleLibrary$InstanceStats.)]
    (ParticleLibrary/Particle_GetStats context stats)
    (ParticleLibrary/Particle_GetInstanceStats context (first (:instances sim)) instance-stats)
    {:particles (.particles stats)
     :max-particles (.maxParticles stats)
     :time (.time instance-stats)}))

(defn reload [sim prototype-msg]
  (reload-prototype (:prototype sim) prototype-msg)
  (let [context (:context sim)]
    (doseq [instance (:instances sim)]
      (ParticleLibrary/Particle_ReloadInstance context instance true)))
  (assoc sim :emitter-count (count (:emitters prototype-msg))))
