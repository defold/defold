(ns editor.particlefx
  (:require [clojure.java.io :as io]
            [dynamo.graph :as g]
            [editor.app-view :as app-view]
            [editor.build-target :as bt]
            [editor.camera :as camera]
            [editor.colors :as colors]
            [editor.core :as core]
            [editor.defold-project :as project]
            [editor.geom :as geom]
            [editor.gl :as gl]
            [editor.gl.pass :as pass]
            [editor.gl.shader :as shader]
            [editor.gl.texture :as texture]
            [editor.gl.vertex :as vtx]
            [editor.graph-util :as gu]
            [editor.handler :as handler]
            [editor.material :as material]
            [editor.math :as math]
            [editor.outline :as outline]
            [editor.particle-lib :as plib]
            [editor.properties :as props]
            [editor.protobuf :as protobuf]
            [editor.resource :as resource]
            [editor.resource-node :as resource-node]
            [editor.scene :as scene]
            [editor.scene-cache :as scene-cache]
            [editor.scene-picking :as scene-picking]
            [editor.scene-tools :as scene-tools]
            [editor.types :as types]
            [editor.validation :as validation]
            [editor.workspace :as workspace])
  (:import [com.dynamo.particle.proto Particle$ParticleFX Particle$Emitter Particle$PlayMode Particle$EmitterType
            Particle$EmitterKey Particle$ParticleKey Particle$ModifierKey
            Particle$EmissionSpace Particle$BlendMode Particle$ParticleOrientation Particle$ModifierType Particle$SizeMode]
           [com.google.protobuf ByteString]
           [com.jogamp.opengl GL GL2 GL2GL3 GLContext GLProfile GLAutoDrawable GLOffscreenAutoDrawable GLDrawableFactory GLCapabilities]
           [editor.gl.shader ShaderLifecycle]
           [editor.properties CurveSpread Curve]
           [editor.types Region Animation Camera Image TexturePacking Rect EngineFormatTexture AABB TextureSetAnimationFrame TextureSetAnimation TextureSet]
           [java.nio ByteBuffer]
           [javax.vecmath Matrix3d Matrix4d Point3d Quat4d Vector4f Vector3d Vector4d]))

(set! *warn-on-reflection* true)

(def particle-fx-icon "icons/32/Icons_17-ParticleFX.png")
(def emitter-icon "icons/32/Icons_18-ParticleFX-emitter.png")
(def emitter-template "templates/template.emitter")
(def modifier-icon "icons/32/Icons_19-ParicleFX-Modifier.png")

(def particlefx-ext "particlefx")

(defn particle-fx-transform [pb]
  (let [xform (fn [v]
                (let [p (doto (Point3d.) (math/clj->vecmath (:position v)))
                      r (doto (Quat4d.) (math/clj->vecmath (:rotation v)))]
                  [p r]))
        global-modifiers (:modifiers pb)
        new-emitters (mapv (fn [emitter]
                             (let [[ep er] (xform emitter)]
                               (update-in emitter [:modifiers] concat
                                          (mapv (fn [modifier]
                                                  (let [[mp mr] (xform modifier)]
                                                    (assoc modifier
                                                           :position (math/vecmath->clj (math/inv-transform ep er mp))
                                                           :rotation (math/vecmath->clj (math/inv-transform er mr)))))
                                                global-modifiers))))
                           (:emitters pb))]
    (-> pb
      (assoc :emitters new-emitters)
      (dissoc :modifiers))))

; Geometry generation

(defn spiraling [steps a s ps]
  (geom/chain steps (comp (partial geom/rotate [0 0 a]) (partial geom/scale [s s 1])) ps))

(def arrow (let [head-ps (->> geom/origin-geom
                           (geom/transl [0 0.1 0])
                           (geom/circling 3)
                           (geom/scale [0.7 1 1])
                           (geom/transl [0 0.05 0]))]
             (concat
               (geom/transl [0 0.85 0] (interleave head-ps (drop 1 (cycle head-ps))))
               [[0 0 0] [0 0.85 0]])))

(def dash-circle (->> geom/origin-geom
                   (geom/chain 1 (partial geom/transl [0.05 0 0]))
                   (geom/transl [0 1 0])
                   (geom/circling 32)))

; Line shader

(vtx/defvertex color-vtx
  (vec3 position)
  (vec4 color))

(shader/defshader line-vertex-shader
  (attribute vec4 position)
  (attribute vec4 color)
  (varying vec4 var_color)
  (defn void main []
    (setq gl_Position (* gl_ModelViewProjectionMatrix position))
    (setq var_color color)))

(shader/defshader line-fragment-shader
  (varying vec4 var_color)
  (defn void main []
    (setq gl_FragColor var_color)))

(def line-shader (shader/make-shader ::line-shader line-vertex-shader line-fragment-shader))

(shader/defshader line-id-vertex-shader
  (attribute vec4 position)
  (defn void main []
    (setq gl_Position (* gl_ModelViewProjectionMatrix position))))

(shader/defshader line-id-fragment-shader
  (uniform vec4 id)
  (defn void main []
    (setq gl_FragColor id)))

(def line-id-shader (shader/make-shader ::line-id-shader line-id-vertex-shader line-id-fragment-shader {"id" :id}))

(def color colors/outline-color)
(def selected-color colors/selected-outline-color)

(def mod-type->properties {:modifier-type-acceleration [:modifier-key-magnitude]
                           :modifier-type-drag [:modifier-key-magnitude]
                           :modifier-type-radial [:modifier-key-magnitude :modifier-key-max-distance]
                           :modifier-type-vortex [:modifier-key-magnitude :modifier-key-max-distance]})

(defn- get-curve-points [property-value]
  (->> property-value
       props/curve-vals
       (sort-by first)
       (mapv (fn [[x y t-x t-y]] {:x x :y y :t-x t-x :t-y t-y}))
       not-empty))

(g/defnk produce-modifier-pb
  [position rotation type magnitude max-distance use-direction]
  (let [values {:modifier-key-magnitude magnitude
                :modifier-key-max-distance max-distance}
        properties (into []
                     (keep (fn [kw]
                             (let [v (get values kw)]
                               (when-let [points (get-curve-points v)]
                                 {:key kw
                                  :points points
                                  :spread (or (:spread v) 0.0)}))))
                     (mod-type->properties type))]
    {:position position
     :rotation rotation
     :type type
     :properties properties
     :use-direction (if use-direction 1 0)}))

(def ^:private type->vcount
  {:emitter-type-2dcone 6
   :emitter-type-circle 6 #_(* 2 circle-steps)})

(defn- ->vb [vs vcount color]
  (let [vb (->color-vtx vcount)]
    (doseq [v vs]
      (conj! vb (into v color)))
    (persistent! vb)))

(defn- orthonormalize [^Matrix4d m]
  (let [m' (Matrix4d. m)
        r (Matrix3d.)]
    (.get m' r)
    (.setRotationScale m' r)
    m'))

(defn render-lines [^GL2 gl render-args renderables rcount]
  (let [camera (:camera render-args)
        viewport (:viewport render-args)
        scale-f (camera/scale-factor camera viewport)
        shader (if (= pass/selection (:pass render-args))
                 line-id-shader
                 line-shader)]
    (doseq [renderable renderables
            :let [vs-screen (get-in renderable [:user-data :geom-data-screen] [])
                  vs-world (get-in renderable [:user-data :geom-data-world] [])
                  vcount (+ (count vs-screen) (count vs-world))]
            :when (> vcount 0)]
      (let [^Matrix4d world-transform (:world-transform renderable)
            world-transform-no-scale (orthonormalize world-transform)
            color (if (:selected renderable) selected-color color)
            vs (into (vec (geom/transf-p world-transform-no-scale (geom/scale scale-f vs-screen)))
                     (geom/transf-p world-transform vs-world))
            render-args (if (= pass/selection (:pass render-args))
                          (assoc render-args :id (scene-picking/renderable-picking-id-uniform renderable))
                          render-args)
            vertex-binding (vtx/use-with ::lines (->vb vs vcount color) shader)]
        (gl/with-gl-bindings gl render-args [shader vertex-binding]
          (gl/gl-draw-arrays gl GL/GL_LINES 0 vcount))))))

; Modifier geometry

(def drag-geom-data (let [top (->> geom/origin-geom
                                (geom/transl [0 10 0])
                                (geom/chain 4 (partial geom/transl [15 10 0])))
                         bottom (geom/scale [1 -1 1] top)]
                      (interleave top bottom)))

(def vortex-geom-data (let [ps (->> geom/origin-geom
                                 (geom/transl [0 1 0])
                                 (spiraling 30 20 1.15)
                                 (geom/scale [0.7 0.7 1]))]
                        (geom/circling 4 (drop-last 2 (interleave ps (drop 1 (cycle ps)))))))

(def vortex-neg-geom-data (geom/scale [-1 1 1] vortex-geom-data))

(def acceleration-geom-data (let [right (->> arrow
                                         (geom/chain 1 (partial geom/transl [0.5 -0.25 0]))
                                         (geom/transl [0.5 -0.25 0]))
                                  left (->> right
                                         (geom/scale [-1 1 1]))]
                              (geom/scale [50 50 1] (concat left arrow right))))

(def acceleration-neg-geom-data (geom/scale [1 -1 1] acceleration-geom-data))

(def radial-geom-data (->> arrow
                            (geom/transl [0 0.3 0])
                            (geom/circling 8)
                            (geom/scale [40 40 1])))

(def radial-neg-geom-data (->> arrow
                            (geom/scale [1 -1 1])
                            (geom/transl [0 1.4 0])
                            (geom/circling 8)
                            (geom/scale [40 40 1])))

(def ^:private mod-types {:modifier-type-acceleration {:label "Acceleration"
                                                       :template {:type :modifier-type-acceleration
                                                                  :use-direction 0
                                                                  :position [0 0 0]
                                                                  :rotation [0 0 0 1]
                                                                  :properties [{:key :modifier-key-magnitude
                                                                                :points [{:x 0.0 :y -100.0 :t-x 1.0 :t-y 0.0}]
                                                                                :spread 0.0}]}
                                                       :geom-data-screen (fn [magnitude _]
                                                                           (if (< magnitude 0)
                                                                             acceleration-neg-geom-data
                                                                             acceleration-geom-data))
                                                       :geom-data-world (constantly [])}
                          :modifier-type-drag {:label "Drag"
                                               :template {:type :modifier-type-drag
                                                          :use-direction 0
                                                          :position [0 0 0]
                                                          :rotation [0 0 0 1]
                                                          :properties [{:key :modifier-key-magnitude
                                                                        :points [{:x 0.0 :y 1.0 :t-x 1.0 :t-y 0.0}]
                                                                        :spread 0.0}]}
                                               :geom-data-screen (constantly drag-geom-data)
                                               :geom-data-world (constantly [])}
                          :modifier-type-radial {:label "Radial"
                                                 :template {:type :modifier-type-radial
                                                            :use-direction 0
                                                            :position [0 0 0]
                                                            :rotation [0 0 0 1]
                                                            :properties [{:key :modifier-key-magnitude
                                                                          :points [{:x 0.0 :y 100.0 :t-x 1.0 :t-y 0.0}]
                                                                          :spread 0.0}
                                                                         {:key :modifier-key-max-distance
                                                                          :points [{:x 0.0 :y 1000.0 :t-x 1.0 :t-y 0.0}]}]}
                                                 :geom-data-screen (fn [magnitude _]
                                                                     (if (< magnitude 0)
                                                                       radial-neg-geom-data
                                                                       radial-geom-data))
                                                 :geom-data-world (fn [_ max-distance]
                                                                    (geom/scale [max-distance max-distance 1] dash-circle))}
                          :modifier-type-vortex {:label "Vortex"
                                                 :template {:type :modifier-type-vortex
                                                            :use-direction 0
                                                            :position [0 0 0]
                                                            :rotation [0 0 0 1]
                                                            :properties [{:key :modifier-key-magnitude
                                                                          :points [{:x 0.0 :y 100.0 :t-x 1.0 :t-y 0.0}]
                                                                          :spread 0.0}
                                                                         {:key :modifier-key-max-distance
                                                                          :points [{:x 0.0 :y 1000.0 :t-x 1.0 :t-y 0.0}]}]}
                                                 :geom-data-screen (fn [magnitude _]
                                                              (if (< magnitude 0)
                                                                vortex-neg-geom-data
                                                                vortex-geom-data))
                                                 :geom-data-world (fn [_ max-distance]
                                                                    (geom/scale [max-distance max-distance 1] dash-circle))}})

(defn- modifier-type-label [modifier-type]
  (:label (mod-types modifier-type)))

(def ^:private modifier-visibility-aabb
  ;; TODO: There's not really a good way to deal with modifiers unless we
  ;; calculate the bounds from the vertex buffer of the affected emitter.
  ;; For now, just use a fairly large :visibility-aabb for all modifiers.
  (geom/coords->aabb [-2500.0 -2500.0 -2500.0]
                     [2500.0 2500.0 2500.0]))

(g/defnk produce-modifier-scene
  [_node-id transform type magnitude max-distance node-outline-key]
  (let [mod-type (mod-types type)
        magnitude (props/sample magnitude)
        max-distance (props/sample max-distance)]
    {:node-id _node-id
     :node-outline-key node-outline-key
     :transform transform
     :aabb geom/empty-bounding-box
     :visibility-aabb modifier-visibility-aabb
     :renderable {:render-fn render-lines
                  :tags #{:particlefx :outline}
                  :batch-key nil
                  :select-batch-key _node-id
                  :user-data {:geom-data-screen ((:geom-data-screen mod-type) magnitude max-distance)
                              :geom-data-world ((:geom-data-world mod-type) magnitude max-distance)}
                  :passes [pass/outline pass/selection]}}))

(g/defnode ModifierNode
  (inherits scene/SceneNode)
  (inherits outline/OutlineNode)

  (property type g/Keyword (dynamic visible (g/constantly false)))
  (property use-direction g/Bool (default false)
            (dynamic visible (g/constantly false)))
  (property magnitude CurveSpread)
  (property max-distance Curve (dynamic visible (g/fnk [type] (contains? #{:modifier-type-radial :modifier-type-vortex} type))))
  (property node-outline-key g/Str (dynamic visible (g/constantly false)))

  (output transform-properties g/Any scene/produce-unscalable-transform-properties)
  (output pb-msg g/Any produce-modifier-pb)
  (output node-outline outline/OutlineData :cached
    (g/fnk [_node-id type node-outline-key]
      (let [mod-type (mod-types type)]
        {:node-id _node-id
         :node-outline-key node-outline-key
         :label (:label mod-type)
         :icon modifier-icon})))
  (output scene g/Any :cached produce-modifier-scene))

(def ^:private circle-steps 32)

(def circle-geom-data (let [ps (->> geom/origin-geom
                                 (geom/transl [0 0.5 0])
                                 (geom/circling 64))]
                        (interleave ps (drop 1 (cycle ps)))))

(def cone-geom-data (let [ps [[-0.5 1.0 0.0] [0.0 0.0 0.0] [0.5 1.0 0.0]]]
                      (interleave ps (drop 1 (cycle ps)))))

(def box-geom-data (let [ps (->> geom/origin-geom
                              (geom/transl [0.5 0.5 0.0])
                              (geom/circling 4))]
                     (interleave ps (drop 1 (cycle ps)))))

(def emitter-types {:emitter-type-circle {:label "Circle"
                                          :geom-data-world (fn [size-x _ _]
                                                             (geom/scale [size-x size-x 1] circle-geom-data))}
                    :emitter-type-sphere {:label "Sphere"
                                          :geom-data-world (fn [size-x _ _]
                                                             (geom/scale [size-x size-x 1] circle-geom-data))}
                    :emitter-type-cone {:label "Cone"
                                        :geom-data-world (fn [size-x size-y _]
                                                           (geom/scale [size-x size-y 1] cone-geom-data))}
                    :emitter-type-2dcone {:label "2D Cone"
                                          :geom-data-world (fn [size-x size-y _]
                                                             (geom/scale [size-x size-y 1] cone-geom-data))}
                    :emitter-type-box {:label "Box"
                                       :geom-data-world (fn [size-x size-y _]
                                                          (geom/scale [size-x size-y 1] box-geom-data))}})

(defn- convert-blend-mode [blend-mode-index]
  (protobuf/pb-enum->val (.getValueDescriptor (Particle$BlendMode/valueOf ^int blend-mode-index))))

(defn- render-emitters-sim [^GL2 gl render-args renderables rcount]
  (doseq [renderable renderables]
    (let [{:keys [emitter-sim-data emitter-index color]} (:user-data renderable)
          pfx-sim-request-id (some-> renderable :updatable :node-id)]
      (when-let [pfx-sim (when (and emitter-sim-data pfx-sim-request-id)
                           (some-> (:pfx-sim (scene-cache/lookup-object ::pfx-sim pfx-sim-request-id nil)) deref))]
        (plib/gen-emitter-vertex-data pfx-sim emitter-index color)
        (let [context (:context pfx-sim)
              vbuf (plib/get-emitter-vertex-data pfx-sim emitter-index)]
          (when-let [render-data (plib/render-emitter pfx-sim emitter-index)]
            (let [gpu-texture (:gpu-texture emitter-sim-data)
                  shader (:shader emitter-sim-data)
                  vtx-binding (vtx/use-with context vbuf shader)
                  blend-mode (convert-blend-mode (:blend-mode render-data))]
              (gl/with-gl-bindings gl render-args [gpu-texture shader vtx-binding]
                (gl/set-blend-mode gl blend-mode)
                (gl/gl-draw-arrays gl GL/GL_TRIANGLES (:v-index render-data) (:v-count render-data))
                (.glBlendFunc gl GL/GL_SRC_ALPHA GL/GL_ONE_MINUS_SRC_ALPHA)))))))))

(defn- render-emitters [^GL2 gl render-args renderables rcount]
  (let [pass (:pass render-args)]
    (condp = pass
      pass/selection (render-lines gl render-args renderables count)
      pass/transparent (render-emitters-sim gl render-args renderables count))))

(g/defnk produce-emitter-aabb [type emitter-key-size-x emitter-key-size-y emitter-key-size-z]
  (case type
    (:emitter-type-2dcone :emitter-type-cone)
    (let [+bx (* 0.5 (props/sample emitter-key-size-x))
          +by (props/sample emitter-key-size-y)
          -bx (- +bx)]
      (geom/coords->aabb [-bx 0.0 (case type :emitter-type-2dcone 0.0 -bx)]
                         [+bx +by (case type :emitter-type-2dcone 0.0 +bx)]))

    :emitter-type-box
    (let [+bx (* 0.5 (props/sample emitter-key-size-x))
          +by (* 0.5 (props/sample emitter-key-size-y))
          +bz (* 0.5 (props/sample emitter-key-size-z))
          -bx (- +bx)
          -by (- +by)
          -bz (- +bz)]
      (geom/coords->aabb [-bx -by -bz]
                         [+bx +by +bz]))

    (:emitter-type-circle :emitter-type-sphere)
    (let [+bx (props/sample emitter-key-size-x)
          -bx (- +bx)]
      (geom/coords->aabb [-bx -bx (case type :emitter-type-circle 0.0 -bx)]
                         [+bx +bx (case type :emitter-type-circle 0.0 +bx)]))))

(defn- property-range
  ^double [prop ^double default]
  (if (nil? prop)
    default
    (let [[^double -r ^double +r] (props/sample-range prop)]
      (max (Math/abs -r) +r))))

(g/defnk produce-emitter-visibility-aabb
  [type
   emitter-key-size-x
   emitter-key-size-y
   emitter-key-size-z
   emitter-key-particle-life-time
   emitter-key-particle-size
   emitter-key-particle-speed
   emitter-key-particle-stretch-factor-x
   emitter-key-particle-stretch-factor-y
   particle-key-scale
   particle-key-stretch-factor-x
   particle-key-stretch-factor-y]
  ;; Crude approximation of the farthest bounds reached by the particles.
  (let [particle-speed (property-range emitter-key-particle-speed 0.0)
        particle-life-time (property-range emitter-key-particle-life-time 0.0)
        particle-travel-distance (* particle-speed particle-life-time)
        particle-radius (* (property-range particle-key-scale 1.0)
                           (property-range emitter-key-particle-size 0.0)
                           0.5
                           (+ 1.0
                              (max (property-range particle-key-stretch-factor-x 0.0)
                                   (property-range particle-key-stretch-factor-y 0.0)))
                           (+ 1.0
                              (max (property-range emitter-key-particle-stretch-factor-x 0.0)
                                   (property-range emitter-key-particle-stretch-factor-y 0.0))))]
    (case type
      (:emitter-type-2dcone :emitter-type-cone)
      (let [emitter-radius (* 0.5 (property-range emitter-key-size-x 0.0))
            emitter-height (property-range emitter-key-size-y 0.0)
            radians (Math/atan (/ emitter-radius emitter-height))
            padding (+ particle-radius particle-travel-distance)
            +bx (+ emitter-radius (* padding (Math/sin radians)))
            +by (+ emitter-height padding)
            -bx (- +bx)
            -by (- particle-radius)]
        (geom/coords->aabb [-bx -by (case type :emitter-type-2dcone 0.0 -bx)]
                           [+bx +by (case type :emitter-type-2dcone 0.0 +bx)]))

      :emitter-type-box
      (let [emitter-radius-x (* 0.5 (property-range emitter-key-size-x 0.0))
            emitter-radius-y (* 0.5 (property-range emitter-key-size-y 0.0))
            emitter-radius-z (* 0.5 (property-range emitter-key-size-z 0.0))
            +bx (+ emitter-radius-x particle-radius)
            +by (+ emitter-radius-y particle-radius particle-travel-distance)
            +bz (+ emitter-radius-z particle-radius)
            -bx (- +bx)
            -by (- 0.0 emitter-radius-y particle-radius)
            -bz (- +bz)]
        (geom/coords->aabb [-bx -by -bz]
                           [+bx +by +bz]))

      (:emitter-type-circle :emitter-type-sphere)
      (let [emitter-radius (* 0.5 (property-range emitter-key-size-x 0.0))
            +bx (+ emitter-radius particle-radius particle-travel-distance)
            -bx (- +bx)]
        (geom/coords->aabb [-bx -bx (case type :emitter-type-circle 0.0 -bx)]
                           [+bx +bx (case type :emitter-type-circle 0.0 +bx)])))))

(g/defnk produce-emitter-scene
  [_node-id id transform aabb visibility-aabb type emitter-sim-data emitter-index emitter-key-size-x emitter-key-size-y emitter-key-size-z child-scenes]
  (let [emitter-type (emitter-types type)
        user-data {:type type
                   :emitter-sim-data emitter-sim-data
                   :emitter-index emitter-index
                   :color [1.0 1.0 1.0 1.0]
                   :geom-data-world (apply (:geom-data-world emitter-type)
                                           (mapv props/sample [emitter-key-size-x emitter-key-size-y emitter-key-size-z]))}]
    {:node-id _node-id
     :node-outline-key id
     :transform transform
     :aabb aabb
     :visibility-aabb visibility-aabb
     :renderable {:render-fn render-emitters
                  :batch-key nil
                  :tags #{:particlefx}
                  :select-batch-key _node-id
                  :user-data user-data
                  :passes [pass/selection pass/transparent]}
     :children (into [{:node-id _node-id
                       :node-outline-key id
                       :aabb aabb
                       :renderable {:render-fn render-lines
                                    :batch-key nil
                                    :tags #{:particlefx :outline}
                                    :select-batch-key _node-id
                                    :user-data user-data
                                    :passes [pass/outline]}}]
                     child-scenes)}))

(g/defnode EmitterProperties
  (property emitter-key-spawn-rate CurveSpread (dynamic label (g/constantly "Spawn Rate")))
  (property emitter-key-size-x CurveSpread (dynamic label (g/constantly "Emitter Size X")))
  (property emitter-key-size-y CurveSpread (dynamic label (g/constantly "Emitter Size Y")))
  (property emitter-key-size-z CurveSpread (dynamic label (g/constantly "Emitter Size Z")))
  (property emitter-key-particle-life-time CurveSpread (dynamic label (g/constantly "Particle Life Time")))
  (property emitter-key-particle-speed CurveSpread (dynamic label (g/constantly "Initial Speed")))
  (property emitter-key-particle-size CurveSpread (dynamic label (g/constantly "Initial Size")))
  (property emitter-key-particle-red CurveSpread (dynamic label (g/constantly "Initial Red")))
  (property emitter-key-particle-green CurveSpread (dynamic label (g/constantly "Initial Green")))
  (property emitter-key-particle-blue CurveSpread (dynamic label (g/constantly "Initial Blue")))
  (property emitter-key-particle-alpha CurveSpread (dynamic label (g/constantly "Initial Alpha")))
  (property emitter-key-particle-rotation CurveSpread (dynamic label (g/constantly "Initial Rotation")))
  (property emitter-key-particle-stretch-factor-x CurveSpread (dynamic label (g/constantly "Initial Stretch X")))
  (property emitter-key-particle-stretch-factor-y CurveSpread (dynamic label (g/constantly "Initial Stretch Y")))
  (property emitter-key-particle-angular-velocity CurveSpread (dynamic label (g/constantly "Initial Angular Velocity"))))

(g/defnode ParticleProperties
  (property particle-key-scale Curve (dynamic label (g/constantly "Life Scale")))
  (property particle-key-red Curve (dynamic label (g/constantly "Life Red")))
  (property particle-key-green Curve (dynamic label (g/constantly "Life Green")))
  (property particle-key-blue Curve (dynamic label (g/constantly "Life Blue")))
  (property particle-key-alpha Curve (dynamic label (g/constantly "Life Alpha")))
  (property particle-key-rotation Curve (dynamic label (g/constantly "Life Rotation")))
  (property particle-key-stretch-factor-x Curve (dynamic label (g/constantly "Life Stretch X")))
  (property particle-key-stretch-factor-y Curve (dynamic label (g/constantly "Life Stretch Y")))
  (property particle-key-angular-velocity Curve (dynamic label (g/constantly "Life Angular Velocity"))))

(def ^:private value-spread-keys #{:duration :start-delay})

(defn- kw->spread-kw [kw]
  (keyword (format "%s-spread" (name kw))))

(defn- get-property [properties kw]
  (let [{:keys [type value]} (properties kw)]
    (cond
      (value-spread-keys kw) [[kw (first value)] [(kw->spread-kw kw) (second value)]]
      (= :editor.resource/Resource (:k type)) [[kw (resource/resource->proj-path value)]]
      true [[kw value]])))

(g/defnk produce-emitter-pb
  [position rotation _declared-properties modifier-msgs]
  (let [properties (:properties _declared-properties)]
    (into {:position position
           :rotation rotation
           :modifiers modifier-msgs}
          (concat
            (mapcat (fn [kw] (get-property properties kw))
                 [:id :mode :duration :space :tile-source :animation :material :blend-mode :particle-orientation
                  :inherit-velocity :max-particle-count :type :start-delay :size-mode :stretch-with-velocity :start-offset])
            [[:properties (into []
                                (comp (map first)
                                      (keep (fn [kw]
                                              (let [v (get-in properties [kw :value])]
                                                (when-let [points (get-curve-points v)]
                                                  {:key kw
                                                   :points points
                                                   :spread (:spread v)})))))
                                (butlast (protobuf/enum-values Particle$EmitterKey)))]
             [:particle-properties (into []
                                         (comp (map first)
                                               (keep (fn [kw]
                                                       (when-let [points (get-curve-points (get-in properties [kw :value]))]
                                                         {:key kw
                                                          :points points}))))
                                         (butlast (protobuf/enum-values Particle$ParticleKey)))]]))))

(defn- attach-modifier [self-id parent-id modifier-id resolve-node-outline-key?]
  (concat
    (for [[from to] [[:_node-id :nodes]]]
      (g/connect modifier-id from self-id to))
    (let [conns [[:node-outline :child-outlines]
                 [:scene :child-scenes]
                 [:pb-msg :modifier-msgs]]]
      (for [[from to] conns]
        (g/connect modifier-id from parent-id to)))
    (when resolve-node-outline-key?
      (g/update-property modifier-id :node-outline-key outline/next-node-outline-key (outline/taken-node-outline-keys parent-id)))))

(defn- prop-resource-error [nil-severity _node-id prop-kw prop-value prop-name]
  (or (validation/prop-error nil-severity _node-id prop-kw validation/prop-nil? prop-value prop-name)
      (validation/prop-error :fatal _node-id prop-kw validation/prop-resource-not-exists? prop-value prop-name)))

(declare ParticleFXNode)

(g/defnode EmitterNode
  (inherits scene/SceneNode)
  (inherits outline/OutlineNode)
  (inherits EmitterProperties)
  (inherits ParticleProperties)

  (property id g/Str)
  (property mode g/Keyword
            (dynamic edit-type (g/constantly (props/->pb-choicebox Particle$PlayMode)))
            (dynamic label (g/constantly "Play Mode")))
  (property duration types/Vec2
            (dynamic edit-type (g/constantly {:type types/Vec2 :labels ["" "+/-"]})))
  (property space g/Keyword
            (dynamic edit-type (g/constantly (props/->pb-choicebox Particle$EmissionSpace)))
            (dynamic label (g/constantly "Emission Space")))

  (property tile-source resource/Resource
            (dynamic label (g/constantly "Image"))
            (value (gu/passthrough tile-source-resource))
            (set (fn [evaluation-context self old-value new-value]
                   (project/resource-setter evaluation-context self old-value new-value
                                            [:resource :tile-source-resource]
                                            [:build-targets :dep-build-targets]
                                            [:texture-set :texture-set]
                                            [:gpu-texture :gpu-texture]
                                            [:anim-ids :anim-ids])))
            (dynamic edit-type (g/constantly
                                 {:type resource/Resource
                                  :ext ["atlas" "tilesource"]}))
            (dynamic error (g/fnk [_node-id tile-source]
                                  (prop-resource-error :fatal _node-id :tile-source tile-source "Image"))))

  (property animation g/Str
            (dynamic error (g/fnk [_node-id animation tile-source anim-ids]
                             (when tile-source
                               (or (validation/prop-error :fatal _node-id :animation validation/prop-empty? animation "Animation")
                                   (validation/prop-error :fatal _node-id :animation validation/prop-anim-missing? animation anim-ids)))))
            (dynamic edit-type (g/fnk [anim-ids]
                                      (let [vals (seq anim-ids)]
                                        (props/->choicebox vals)))))

  (property material resource/Resource
            (value (gu/passthrough material-resource))
            (set (fn [evaluation-context self old-value new-value]
                   (project/resource-setter evaluation-context self old-value new-value
                                            [:resource :material-resource]
                                            [:shader :material-shader]
                                            [:samplers :material-samplers]
                                            [:build-targets :dep-build-targets])))
            (dynamic edit-type (g/constantly
                                 {:type resource/Resource
                                  :ext ["material"]}))
            (dynamic error (g/fnk [_node-id material]
                                  (prop-resource-error :fatal _node-id :material material "Material"))))

  (property blend-mode g/Keyword
            (dynamic tip (validation/blend-mode-tip blend-mode Particle$BlendMode))
            (dynamic edit-type (g/constantly (props/->pb-choicebox Particle$BlendMode))))

  (property particle-orientation g/Keyword (dynamic edit-type (g/constantly (props/->pb-choicebox Particle$ParticleOrientation))))
  (property inherit-velocity g/Num)
  (property max-particle-count g/Int)
  (property type g/Keyword
            (dynamic edit-type (g/constantly (props/->pb-choicebox Particle$EmitterType)))
            (dynamic label (g/constantly "Emitter Type")))
  (property start-delay types/Vec2
            (dynamic edit-type (g/constantly {:type types/Vec2 :labels ["" "+/-"]})))
  (property size-mode g/Keyword
            (dynamic edit-type (g/constantly (props/->pb-choicebox Particle$SizeMode)))
            (dynamic label (g/constantly "Size Mode")))
  (property stretch-with-velocity g/Bool)
  (property start-offset g/Num)

  (display-order [:id scene/SceneNode :mode :size-mode :space :duration :start-delay :start-offset :tile-source :animation :material :blend-mode
                  :max-particle-count :type :particle-orientation :inherit-velocity ["Particle" ParticleProperties]])

  (input tile-source-resource resource/Resource)
  (input material-resource resource/Resource)
  (input material-shader ShaderLifecycle)
  (input material-samplers g/Any)
  (input default-tex-params g/Any)
  (input texture-set g/Any)
  (input gpu-texture g/Any)
  (input dep-build-targets g/Any :array)
  (input emitter-indices g/Any)
  (output emitter-index g/Any (g/fnk [_node-id emitter-indices] (emitter-indices _node-id)))
  (output build-targets g/Any (g/fnk [_node-id tile-source material animation anim-ids dep-build-targets]
                                (or (when-let [errors (->> [(validation/prop-error :fatal _node-id :tile-source validation/prop-nil? tile-source "Image")
                                                            (validation/prop-error :fatal _node-id :material validation/prop-nil? material "Material")
                                                            (validation/prop-error :fatal _node-id :animation validation/prop-nil? animation "Animation")
                                                            (validation/prop-error :fatal _node-id :animation validation/prop-anim-missing? animation anim-ids)]
                                                           (remove nil?)
                                                           (seq))]
                                      (g/error-aggregate errors))
                                    dep-build-targets)))
  (input anim-ids g/Any)
  (input child-scenes g/Any :array)
  (input modifier-msgs g/Any :array)

  (output tex-params g/Any (g/fnk [material-samplers default-tex-params]
                             (or (some-> material-samplers first material/sampler->tex-params)
                                 default-tex-params)))
  (output gpu-texture g/Any (g/fnk [gpu-texture tex-params] (texture/set-params gpu-texture tex-params)))
  (output transform-properties g/Any scene/produce-unscalable-transform-properties)
  (output scene g/Any :cached produce-emitter-scene)
  (output pb-msg g/Any produce-emitter-pb)
  (output node-outline outline/OutlineData :cached (g/fnk [_node-id id child-outlines]
                                                     {:node-id _node-id
                                                      :node-outline-key id
                                                      :label id
                                                      :icon emitter-icon
                                                      :children (outline/natural-sort child-outlines)
                                                      :child-reqs [{:node-type ModifierNode
                                                                    :tx-attach-fn (fn [self-id child-id]
                                                                                    (let [pfx-id (core/scope-of-type self-id ParticleFXNode)]
                                                                                      (attach-modifier pfx-id self-id child-id true)))}]}))
  (output aabb AABB produce-emitter-aabb)
  (output visibility-aabb AABB produce-emitter-visibility-aabb)
  (output emitter-sim-data g/Any :cached
          (g/fnk [animation texture-set gpu-texture material-shader]
            (when (and animation texture-set gpu-texture)
              (let [texture-set-anim (first (filter #(= animation (:id %)) (:animations texture-set)))
                    ^ByteString tex-coords (:tex-coords texture-set)
                    tex-coords-buffer (ByteBuffer/allocateDirect (.size tex-coords))]
                (.put tex-coords-buffer (.asReadOnlyByteBuffer tex-coords))
                (.flip tex-coords-buffer)
                {:gpu-texture gpu-texture
                 :shader material-shader
                 :texture-set-anim texture-set-anim
                 :tex-coords tex-coords-buffer})))))

(defn- build-pb [resource dep-resources user-data]
  (let [pb  (:pb user-data)
        pb  (reduce (fn [pb [label resource]]
                      (assoc-in pb label resource))
                    pb (map (fn [[label res]]
                              [label (resource/proj-path (get dep-resources res))])
                            (:dep-resources user-data)))]
    {:resource resource :content (protobuf/map->bytes Particle$ParticleFX pb)}))

(def ^:private resource-fields [[:emitters :tile-source] [:emitters :material]])

(g/defnk produce-build-targets [_node-id resource rt-pb-data dep-build-targets]
  (let [dep-build-targets (flatten dep-build-targets)
        deps-by-source (into {} (map #(let [res (:resource %)] [(resource/proj-path (:resource res)) res]) dep-build-targets))
        resource-fields (mapcat (fn [field]
                                  (if (vector? field)
                                    (mapv
                                     (fn [i]
                                       (into [(first field) i] (rest field)))
                                     (range (count (get rt-pb-data (first field)))))
                                    [field]))
                                resource-fields)
        dep-resources (map (fn [label] [label (get deps-by-source (if (vector? label) (get-in rt-pb-data label) (get rt-pb-data label)))]) resource-fields)]
    [(bt/with-content-hash
       {:node-id _node-id
        :resource (workspace/make-build-resource resource)
        :build-fn build-pb
        :user-data {:pb rt-pb-data
                    :dep-resources dep-resources}
        :deps dep-build-targets})]))

(defn- render-pfx [^GL2 gl render-args renderables count])

(g/defnk produce-scene [_node-id child-scenes scene-updatable]
  (let [scene {:node-id _node-id
               :renderable {:render-fn render-pfx
                            :batch-key nil
                            :passes [pass/transparent pass/selection]}
               :aabb geom/empty-bounding-box
               :children child-scenes}]
    (scene/map-scene #(assoc % :updatable scene-updatable) scene)))

(g/defnk produce-scene-updatable [_node-id rt-pb-data fetch-anim-fn project-settings]
  {:node-id _node-id
   :name "ParticleFX"
   :update-fn (fn [state {:keys [dt world-transform node-id]}]
                (let [max-emitter-count (get project-settings ["particle_fx" "max_count"])
                      max-particle-count (get project-settings ["particle_fx" "max_particle_count"])
                      data [max-emitter-count max-particle-count rt-pb-data world-transform]
                      pfx-sim-ref (:pfx-sim (scene-cache/request-object! ::pfx-sim node-id nil data))]
                  (swap! pfx-sim-ref plib/simulate dt fetch-anim-fn [world-transform])
                  state))})

(defn- attach-emitter [self-id emitter-id resolve-id?]
  (concat
    (for [[from to] [[:_node-id :nodes]
                     [:node-outline :child-outlines]
                     [:scene :child-scenes]
                     [:pb-msg :emitter-msgs]
                     [:emitter-sim-data :emitter-sim-data]
                     [:id :ids]
                     [:build-targets :dep-build-targets]]]
      (g/connect emitter-id from self-id to))
    (for [[from to] [[:default-tex-params :default-tex-params]
                     [:emitter-indices :emitter-indices]]]
      (g/connect self-id from emitter-id to))
    (when resolve-id?
      (g/update-property emitter-id :id outline/resolve-id (g/node-value self-id :ids)))))

(g/defnode ParticleFXNode
  (inherits resource-node/ResourceNode)

  (input project-settings g/Any)
  (input default-tex-params g/Any)
  (input dep-build-targets g/Any :array)
  (input emitter-msgs g/Any :array)
  (input modifier-msgs g/Any :array)
  (input child-scenes g/Any :array)
  (input emitter-sim-data g/Any :array)
  (input ids g/Str :array)

  (output default-tex-params g/Any (gu/passthrough default-tex-params))
  (output save-value g/Any (gu/passthrough pb-data))
  (output pb-data g/Any (g/fnk [emitter-msgs modifier-msgs]
                               {:emitters emitter-msgs :modifiers modifier-msgs}))
  (output rt-pb-data g/Any :cached (g/fnk [pb-data] (particle-fx-transform pb-data)))
  (output emitter-sim-data g/Any :cached (gu/passthrough emitter-sim-data))
  (output emitter-indices g/Any :cached (g/fnk [nodes]
                                               (into {}
                                                     (comp
                                                       (filter #(g/node-instance? EmitterNode %))
                                                       (map-indexed (fn [index emitter-node]
                                                                      [emitter-node index])))
                                                     nodes)))
                                                 
  (output build-targets g/Any :cached produce-build-targets)
  (output scene g/Any :cached produce-scene)
  (output scene-updatable g/Any :cached produce-scene-updatable)
  (output node-outline outline/OutlineData :cached (g/fnk [_node-id child-outlines]
                                                     (let [[mod-outlines emitter-outlines] (let [outlines (group-by #(g/node-instance? ModifierNode (:node-id %)) child-outlines)]
                                                                                             [(get outlines true) (get outlines false)])]
                                                       {:node-id _node-id
                                                        :node-outline-key "ParticleFX"
                                                        :label "ParticleFX"
                                                        :icon particle-fx-icon
                                                        :children (into (outline/natural-sort emitter-outlines) (outline/natural-sort mod-outlines))
                                                        :child-reqs [{:node-type EmitterNode
                                                                      :tx-attach-fn (fn [self-id child-id]
                                                                                      (attach-emitter self-id child-id true))}
                                                                     {:node-type ModifierNode
                                                                      :tx-attach-fn (fn [self-id child-id]
                                                                                      (attach-modifier self-id self-id child-id true))}]})))
  (output fetch-anim-fn Runnable :cached (g/fnk [emitter-sim-data] (fn [index] (get emitter-sim-data index)))))

(defn- make-modifier
  [self parent-id modifier node-outline-key]
  (let [graph-id (g/node-id->graph-id self)]
    (g/make-nodes graph-id
                  [mod-node [ModifierNode :position (:position modifier) :rotation (:rotation modifier)
                             :type (:type modifier) :node-outline-key node-outline-key]]
                  (let [mod-properties (into {} (map #(do [(:key %) (dissoc % :key)])
                                                     (:properties modifier)))]
                    (concat
                      (g/set-property mod-node :use-direction (= 1 (:use-direction mod-properties)))
                      (g/set-property mod-node :magnitude (if-let [prop (:modifier-key-magnitude mod-properties)]
                                                            (props/->curve-spread (map #(let [{:keys [x y t-x t-y]} %] [x y t-x t-y]) (:points prop)) (:spread prop))
                                                            props/default-curve-spread))
                      (g/set-property mod-node :max-distance (if-let [prop (:modifier-key-max-distance mod-properties)]
                                                               (props/->curve (map #(let [{:keys [x y t-x t-y]} %] [x y t-x t-y]) (:points prop)))
                                                               props/default-curve))))
                  (attach-modifier self parent-id mod-node false))))

(defn- add-modifier-handler [self parent-id type select-fn]
  (when-some [modifier (get-in mod-types [type :template])]
    (let [node-outline-key (outline/next-node-outline-key (modifier-type-label type)
                                                          (outline/taken-node-outline-keys parent-id))
          op-seq (gensym)
          mod-node (first (g/tx-nodes-added
                            (g/transact
                              (concat
                                (g/operation-label "Add Modifier")
                                (g/operation-sequence op-seq)
                                (make-modifier self parent-id modifier node-outline-key)))))]
      (when (some? select-fn)
        (g/transact
          (concat
            (g/operation-sequence op-seq)
            (select-fn [mod-node])))))))

(defn- selection->emitter [selection]
  (handler/adapt-single selection EmitterNode))

(defn- selection->particlefx [selection]
  (handler/adapt-single selection ParticleFXNode))

(handler/defhandler :add-secondary :workbench
  (active? [selection] (or (selection->emitter selection)
                           (selection->particlefx selection)))
  (label [user-data] (if-not user-data
                       "Add Modifier"
                       (->> user-data :modifier-type mod-types :label)))
  (run [selection user-data app-view]
    (let [[self parent-id] (if-let [pfx (selection->particlefx selection)]
                             [pfx pfx]
                             (let [emitter (selection->emitter selection)]
                               [(core/scope emitter) emitter]))]
      (add-modifier-handler self parent-id (:modifier-type user-data) (fn [node-ids] (app-view/select app-view node-ids)))))
  (options [selection user-data]
    (when (not user-data)
      (mapv (fn [[type data]] {:label (:label data)
                               :icon modifier-icon
                               :command :add-secondary
                               :user-data {:modifier-type type}}) mod-types))))

(defn- make-emitter
  ([self emitter]
    (make-emitter self emitter nil false))
  ([self emitter select-fn resolve-id?]
    (let [project   (project/get-project self)
          workspace (project/workspace project)
          graph-id (g/node-id->graph-id self)
          tile-source (workspace/resolve-workspace-resource workspace (:tile-source emitter))
          material (workspace/resolve-workspace-resource workspace (:material emitter))]
      (g/make-nodes graph-id
                    [emitter-node [EmitterNode :position (:position emitter) :rotation (:rotation emitter)
                                   :id (:id emitter) :mode (:mode emitter) :duration [(:duration emitter) (:duration-spread emitter)] :space (:space emitter)
                                   :tile-source tile-source :animation (:animation emitter) :material material
                                   :blend-mode (:blend-mode emitter) :particle-orientation (:particle-orientation emitter)
                                   :inherit-velocity (:inherit-velocity emitter) :max-particle-count (:max-particle-count emitter)
                                   :type (:type emitter) :start-delay [(:start-delay emitter) (:start-delay-spread emitter)] :size-mode (:size-mode emitter)
                                   :stretch-with-velocity (:stretch-with-velocity emitter) :start-offset (:start-offset emitter)]]
                    (let [emitter-properties (into {} (map #(do [(:key %) (select-keys % [:points :spread])]) (:properties emitter)))]
                      (for [key (g/declared-property-labels EmitterProperties)
                            :when (contains? emitter-properties key)
                            :let [p (get emitter-properties key)
                                  curve (props/->curve-spread (map #(let [{:keys [x y t-x t-y]} %]
                                                                      [x y t-x t-y]) (:points p)) (:spread p))]]
                        (g/set-property emitter-node key curve)))
                    (let [particle-properties (into {} (map #(do [(:key %) (select-keys % [:points])]) (:particle-properties emitter)))]
                      (for [key (g/declared-property-labels ParticleProperties)
                            :when (contains? particle-properties key)
                            :let [p (get particle-properties key)
                                  curve (props/->curve (map #(let [{:keys [x y t-x t-y]} %]
                                                               [x y t-x t-y]) (:points p)))]]
                        (g/set-property emitter-node key curve)))
                    (attach-emitter self emitter-node resolve-id?)
                    (map (partial make-modifier self emitter-node)
                         (:modifiers emitter)
                         (outline/gen-node-outline-keys (map (comp modifier-type-label :type)
                                                             (:modifiers emitter))))
                    (if select-fn
                      (select-fn [emitter-node])
                      [])))))

(defn- add-emitter-handler [self type select-fn]
  (when-let [resource (io/resource emitter-template)]
      (let [emitter (protobuf/read-text Particle$Emitter resource)]
        (g/transact
          (concat
            (g/operation-label "Add Emitter")
            (make-emitter self (assoc emitter :type type) select-fn true))))))

(handler/defhandler :add :workbench
  (active? [selection] (selection->particlefx selection))
  (label [user-data] (if-not user-data
                       "Add Emitter"
                       (->> user-data :emitter-type (get emitter-types) :label)))
  (run [selection user-data app-view] (let [pfx (selection->particlefx selection)]
                                        (add-emitter-handler pfx (:emitter-type user-data) (fn [node-ids] (app-view/select app-view node-ids)))))
  (options [selection user-data]
           (when (not user-data)
             (let [self (selection->particlefx selection)]
               (mapv (fn [[type data]] {:label (:label data)
                                        :icon emitter-icon
                                        :command :add
                                        :user-data {:emitter-type type}}) emitter-types)))))


;;--------------------------------------------------------------------
;; Manipulators

(defn- update-curve-spread-start-value
  [curve-spread f]
  (let [[first-point & rest] (props/curve-vals curve-spread)
        first-point' (update first-point 1 f)]
    (props/->curve-spread (into [first-point'] rest) (:spread curve-spread))))

(defmethod scene-tools/manip-scalable? ::ModifierNode [_node-id] true)

(defmethod scene-tools/manip-scale-manips ::ModifierNode [node-id]
  [:scale-x])

(defmethod scene-tools/manip-scale ::ModifierNode
  [evaluation-context node-id ^Vector3d delta]
  (let [mag (g/node-value node-id :magnitude evaluation-context)]
    (concat
      (g/set-property node-id :magnitude
                      (update-curve-spread-start-value mag #(props/round-scalar (* % (.getX delta))))))))

(defmethod scene-tools/manip-scalable? ::EmitterNode [_node-id] true)

(defmethod scene-tools/manip-scale ::EmitterNode
  [evaluation-context node-id ^Vector3d delta]
  (let [x (g/node-value node-id :emitter-key-size-x evaluation-context)
        y (g/node-value node-id :emitter-key-size-y evaluation-context)
        z (g/node-value node-id :emitter-key-size-z evaluation-context)]
    (concat
      (g/set-property node-id :emitter-key-size-x
                      (update-curve-spread-start-value x #(props/round-scalar (Math/abs (* % (.getX delta))))))
      (g/set-property node-id :emitter-key-size-y
                      (update-curve-spread-start-value y #(props/round-scalar (Math/abs (* % (.getY delta))))))
      (g/set-property node-id :emitter-key-size-z
                      (update-curve-spread-start-value z #(props/round-scalar (Math/abs (* % (.getZ delta)))))))))


(defn load-particle-fx [project self _resource pb]
  (concat
    (g/connect project :settings self :project-settings)
    (g/connect project :default-tex-params self :default-tex-params)
    (map (partial make-emitter self)
         (:emitters pb))
    (map (partial make-modifier self self)
         (:modifiers pb)
         (outline/gen-node-outline-keys (map (comp modifier-type-label :type)
                                             (:modifiers pb))))))

(defn- add-default-properties-to-modifier [mod-pb]
  (update mod-pb :properties #(or (not-empty %) (get-in mod-types [(:type mod-pb) :template :properties]))))

(defn- add-default-properties-to-modifiers [pb]
  (update pb :modifiers (partial mapv add-default-properties-to-modifier)))

(defn register-resource-types [workspace]
  (resource-node/register-ddf-resource-type workspace
    :ext particlefx-ext
    :label "Particle FX"
    :node-type ParticleFXNode
    :ddf-type Particle$ParticleFX
    :load-fn load-particle-fx
    :sanitize-fn add-default-properties-to-modifiers
    :icon particle-fx-icon
    :tags #{:component :non-embeddable}
    :tag-opts {:component {:transform-properties #{:position :rotation}}}
    :view-types [:scene :text]
    :view-opts {:scene {:grid true}}))

(defn- make-pfx-sim [_ data]
  (let [[max-emitter-count max-particle-count rt-pb-data world-transform] data]
    {:pfx-sim (atom (plib/make-sim max-emitter-count max-particle-count rt-pb-data [world-transform]))
     :protobuf-msg rt-pb-data
     :max-particle-count max-particle-count}))

(defn- update-pfx-sim [_ pfx-sim data]
  (let [[_max-emitter-count max-particle-count rt-pb-data _world-transform] data
        pfx-sim-ref (:pfx-sim pfx-sim)]
    (when (or (not= rt-pb-data (:protobuf-msg pfx-sim))
              (not= max-particle-count (:max-particle-count pfx-sim)))
      (swap! pfx-sim-ref plib/reload rt-pb-data max-particle-count))
    (assoc pfx-sim :protobuf-msg rt-pb-data :max-particle-count max-particle-count)))

(defn- destroy-pfx-sims [_ pfx-sims _]
  (doseq [pfx-sim pfx-sims]
    (plib/destroy-sim @(:pfx-sim pfx-sim))))

(scene-cache/register-object-cache! ::pfx-sim make-pfx-sim update-pfx-sim destroy-pfx-sims)
