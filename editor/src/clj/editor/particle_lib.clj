(ns editor.particle-lib
  (:require [dynamo.graph :as g]
            [editor.math :as math]
            [editor.protobuf :as protobuf]
            [editor.gl.vertex :as vertex]
            [editor.gl.shader :as shader])
  (:import [com.dynamo.particle.proto Particle$ParticleFX]
           [com.defold.libs ParticleLibrary ParticleLibrary$Vector3 ParticleLibrary$Quat
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

(shader/defshader vertex-shader
  (uniform mat4 view_proj)
  (uniform mat4 world)
  (attribute vec4 position)
  (attribute vec2 texcoord0)
  (attribute vec4 color)
  (varying vec2 var_texcoord0)
  (varying vec4 var_color)
  (defn void main []
    ; NOTE: world isn't used here. Particle positions are already transformed
    ; prior to rendering but the world-transform is set for sorting.
    (setq gl_Position (* view_proj (vec4 position.xyz 1.0)))
    (setq var_texcoord0 texcoord0)
    (setq var_color color)))

(shader/defshader fragment-shader
  (varying vec4 position)
  (varying vec2 var_texcoord0)
  (varying vec4 var_color)
  (uniform sampler2D texture)
  (uniform vec4 tint)
  (defn void main []
    ; Pre-multiply alpha since all runtime textures already are
    (setq vec4 tint_pm (vec4 (* tint.xyz tint.w) tint.w))
    ; var_color is vertex color from the particle system, already pre-multiplied
    (setq gl_FragColor (* (texture2D texture var_texcoord0.xy) var_color tint_pm))))

(def shader (shader/make-shader ::shader vertex-shader fragment-shader))

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

(defn- create-instance [^Pointer context ^Pointer prototype ^Matrix4d transform]
  (let [^Pointer instance (ParticleLibrary/Particle_CreateInstance context prototype)]
    (set-instance-transform context instance transform)))

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

(defn make-sim [max-emitter-count max-particle-count prototype-msg instance-transforms]
  (let [context (create-context max-emitter-count max-particle-count)
        prototype (new-prototype prototype-msg)
        instances (mapv (fn [^Matrix4d t] (create-instance context prototype t)) instance-transforms)
        raw-vbuf (Buffers/newDirectByteBuffer (ParticleLibrary/Particle_GetVertexBufferSize max-particle-count))
        vbuf (vertex/vertex-overlay vertex-format raw-vbuf)]
    {:context context
     :prototype prototype
     :emitter-count (count (:emitters prototype-msg))
     :instances instances
     :raw-vbuf raw-vbuf
     :vbuf vbuf
     :vtx-binding (vertex/use-with context vbuf shader)
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
  (let [out-size (IntByReference. 0)
        anim-callback (FetchAnimCallback. fetch-anim-fn)
        sim (if (sleeping? sim)
              (-> sim
                (reset)
                (start))
              sim)
        context (:context sim)
        ^ByteBuffer raw-vbuf (:raw-vbuf sim)]
    (doseq [[instance transform] (map vector (:instances sim) instance-transforms)]
      (set-instance-transform context instance transform))
    (ParticleLibrary/Particle_Update context dt raw-vbuf (.capacity raw-vbuf) out-size anim-callback)
    (.limit raw-vbuf (.getValue out-size))
    (let [vbuf (vertex/vertex-overlay vertex-format raw-vbuf)]
      (-> sim
        (update :elapsed-time #(+ % dt))
        (assoc :vbuf vbuf :vtx-binding (vertex/use-with context vbuf shader))))))

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
      (ParticleLibrary/Particle_RenderEmitter context instance emitter-index (Pointer. 0) callback))
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
  sim)
