(ns editor.particlefx
  (:require [clojure.java.io :as io]
            [dynamo.graph :as g]
            [editor.graph-util :as gu]
            [editor.colors :as colors]
            [editor.math :as math]
            [editor.protobuf :as protobuf]
            [editor.resource :as resource]
            [editor.workspace :as workspace]
            [editor.project :as project]
            [editor.gl :as gl]
            [editor.gl.shader :as shader]
            [editor.gl.vertex :as vtx]
            [editor.scene :as scene]
            [editor.scene-layout :as layout]
            [editor.scene-cache :as scene-cache]
            [editor.outline :as outline]
            [editor.geom :as geom]
            [internal.render.pass :as pass]
            [editor.particle-lib :as plib]
            [editor.properties :as props]
            [editor.validation :as validation]
            [editor.camera :as camera]
            [editor.handler :as handler]
            [editor.core :as core])
  (:import [javax.vecmath Matrix4d Point3d Quat4d Vector4f Vector3d Vector4d]
           [com.dynamo.particle.proto Particle$ParticleFX Particle$Emitter Particle$PlayMode Particle$EmitterType
            Particle$EmissionSpace Particle$BlendMode Particle$ParticleOrientation Particle$ModifierType]
           [javax.media.opengl GL GL2 GL2GL3 GLContext GLProfile GLAutoDrawable GLOffscreenAutoDrawable GLDrawableFactory GLCapabilities]
           [editor.types Region Animation Camera Image TexturePacking Rect EngineFormatTexture AABB TextureSetAnimationFrame TextureSetAnimation TextureSet]
           [com.defold.libs ParticleLibrary]
           [com.sun.jna Pointer]
           [java.nio ByteBuffer]
           [com.google.protobuf ByteString]
           [editor.properties CurveSpread Curve]))

(def ^:private particle-fx-icon "icons/32/Icons_17-ParticleFX.png")
(def ^:private emitter-icon "icons/32/Icons_18-ParticleFX-emitter.png")
(def ^:private emitter-template "templates/template.emitter")
(def ^:private modifier-icon "icons/32/Icons_19-ParicleFX-Modifier.png")

(defn- ->choicebox [cls]
  (let [options (protobuf/enum-values cls)]
    {:type :choicebox
     :options (zipmap (map first options)
                      (map (comp :display-name second) options))}))

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
                                                    (math/inv-transform ep er mp)
                                                    (math/inv-transform er mr)
                                                    (assoc modifier
                                                           :position (math/vecmath->clj mp)
                                                           :rotation (math/vecmath->clj mr))))
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

(def color (scene/select-color pass/outline false [1.0 1.0 1.0]))
(def selected-color (scene/select-color pass/outline true [1.0 1.0 1.0]))

(g/defnk produce-modifier-pb
  [position rotation type magnitude max-distance]
  (let [properties (mapv (fn [[key p]]
                           {:key key
                            :points (:points p)
                            :spread (:spread p)})
                         {:modifier-key-magnitude magnitude
                          :modifier-key-max-distance max-distance})]
    {:position (math/vecmath->clj position)
     :rotation (math/vecmath->clj rotation)
     :type type
     :properties properties}))

(def ^:private type->vcount
  {:emitter-type-2dcone 6
   :emitter-type-circle 6 #_(* 2 circle-steps)})

(defn- ->vb [vs vcount color]
  (let [vb (->color-vtx vcount)]
    (doseq [v vs]
      (conj! vb (into v color)))
    (persistent! vb)))

(defn- scale-factor [camera viewport]
  (let [inv-view (doto (Matrix4d. (camera/camera-view-matrix camera)) (.invert))
        x-axis   (Vector4d.)
        _        (.getColumn inv-view 0 x-axis)
        cp1      (camera/camera-project camera viewport (Point3d.))
        cp2      (camera/camera-project camera viewport (Point3d. (.x x-axis) (.y x-axis) (.z x-axis)))]
    (/ 1.0 (Math/abs (- (.x cp1) (.x cp2))))))

(defn render-lines [^GL2 gl render-args renderables rcount]
  (let [camera (:camera render-args)
        viewport (:viewport render-args)
        scale-f (scale-factor camera viewport)]
    (doseq [renderable renderables
            :let [vs-screen (get-in renderable [:user-data :geom-data-screen] [])
                  vs-world (get-in renderable [:user-data :geom-data-world] [])
                  vcount (+ (count vs-screen) (count vs-world))]
            :when (> vcount 0)]
      (let [world-transform (:world-transform renderable)
            color (-> (if (:selected renderable) selected-color color)
                    (conj 1))
            vs (geom/transf-p world-transform (concat
                                                (geom/scale [scale-f scale-f 1] vs-screen)
                                                vs-world))
            vertex-binding (vtx/use-with ::lines (->vb vs vcount color) line-shader)]
        (gl/with-gl-bindings gl render-args [line-shader vertex-binding]
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

(g/defnk produce-modifier-scene
  [_node-id transform aabb type magnitude max-distance]
  (let [mod-type (mod-types type)
        magnitude (props/sample magnitude)
        max-distance (props/sample max-distance)]
    {:node-id _node-id
     :transform transform
     :aabb aabb
     :renderable {:render-fn render-lines
                  :batch-key nil
                  :select-batch-key _node-id
                  :user-data {:geom-data-screen ((:geom-data-screen mod-type) magnitude max-distance)
                              :geom-data-world ((:geom-data-world mod-type) magnitude max-distance)}
                  :passes [pass/outline pass/selection]}}))

(g/defnode ModifierNode
  (inherits scene/SceneNode)
  (inherits outline/OutlineNode)

  (property type g/Keyword (dynamic visible (g/always false)))
  (property magnitude CurveSpread)
  (property max-distance Curve (dynamic visible (g/fnk [type] (contains? #{:modifier-type-radial :modifier-type-vortex} type))))

  (output pb-msg g/Any :cached produce-modifier-pb)
  (output node-outline outline/OutlineData :cached
    (g/fnk [_node-id type]
      (let [mod-type (mod-types type)]
        {:node-id _node-id :label (:label mod-type) :icon modifier-icon})))
  (output aabb AABB (g/fnk [] (geom/aabb-incorporate (geom/null-aabb) 0 0 0)))
  (output scene g/Any :cached produce-modifier-scene))

(def ^:private circle-steps 32)

(defn- gen-vertex [^Matrix4d wt ^Point3d pt x y [cr cg cb]]
  (.set pt x y 0)
  (.transform wt pt)
  [(.x pt) (.y pt) (.z pt) cr cg cb 1])

(defn- conj-2d-cone! [vbuf ^Matrix4d wt ^Point3d pt type size color]
  (let [[sx sy _] size
        hsx (* sx 0.5)
        xl (gen-vertex wt pt (- hsx) sy color)
        xr (gen-vertex wt pt hsx sy color)
        c (gen-vertex wt pt 0.0 0.0 color)]
    (-> vbuf (conj! c) (conj! xr) (conj! xr) (conj! xl) (conj! xl) (conj! c))))

(defn- gen-vertex-buffer
  [vcount renderables count]
  (let [tmp-point (Point3d.)]
    (loop [renderables renderables
           vbuf (->color-vtx vcount)]
      (if-let [renderable (first renderables)]
        (let [color (if (:selected renderable) selected-color color)
              world-transform (:world-transform renderable)
              user-data (:user-data renderable)
              type (:type user-data)
              size (:size user-data)]
          (recur (rest renderables) (conj-2d-cone! vbuf world-transform tmp-point type size color)))
        (persistent! vbuf)))))

(def circle-geom-data (let [ps (->> geom/origin-geom
                                 (geom/transl [0 1 0])
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

(g/defnk produce-emitter-scene
  [_node-id transform aabb type emitter-key-size-x emitter-key-size-y emitter-key-size-z child-scenes]
  (let [emitter-type (emitter-types type)]
    {:node-id _node-id
     :transform transform
     :aabb aabb
     :renderable {:render-fn render-lines
                  :batch-key nil
                  :select-batch-key _node-id
                  :user-data {:type type
                              :geom-data-world (apply (:geom-data-world emitter-type)
                                                 (mapv props/sample [emitter-key-size-x emitter-key-size-y emitter-key-size-z]))}
                  :passes [pass/outline pass/selection]}
     :children child-scenes}))

(g/defnode EmitterProperties
  (property emitter-key-spawn-rate CurveSpread (dynamic label (g/always "Spawn Rate")))
  (property emitter-key-size-x CurveSpread (dynamic label (g/always "Emitter Size X")))
  (property emitter-key-size-y CurveSpread (dynamic label (g/always "Emitter Size Y")))
  (property emitter-key-size-z CurveSpread (dynamic label (g/always "Emitter Size Z")))
  (property emitter-key-particle-life-time CurveSpread (dynamic label (g/always "Particle Life Time")))
  (property emitter-key-particle-speed CurveSpread (dynamic label (g/always "Initial Speed")))
  (property emitter-key-particle-size CurveSpread (dynamic label (g/always "Initial Size")))
  (property emitter-key-particle-red CurveSpread (dynamic label (g/always "Initial Red")))
  (property emitter-key-particle-green CurveSpread (dynamic label (g/always "Initial Green")))
  (property emitter-key-particle-blue CurveSpread (dynamic label (g/always "Initial Blue")))
  (property emitter-key-particle-alpha CurveSpread (dynamic label (g/always "Initial Alpha")))
  (property emitter-key-particle-rotation CurveSpread (dynamic label (g/always "Initial Rotation"))))

(g/defnode ParticleProperties
  (property particle-key-scale Curve (dynamic label (g/always "Life Scale")))
  (property particle-key-red Curve (dynamic label (g/always "Life Red")))
  (property particle-key-green Curve (dynamic label (g/always "Life Green")))
  (property particle-key-blue Curve (dynamic label (g/always "Life Blue")))
  (property particle-key-alpha Curve (dynamic label (g/always "Life Alpha")))
  (property particle-key-rotation Curve (dynamic label (g/always "Life Rotation"))))

(defn- get-property [properties kw]
  (let [v (get-in properties [kw :value])]
    (if (satisfies? resource/Resource v)
      (resource/proj-path v)
      v)))

(g/defnk produce-emitter-pb
  [position rotation _declared-properties modifier-msgs]
  (let [properties (:properties _declared-properties)]
    (into {:position (math/vecmath->clj position)
           :rotation (math/vecmath->clj rotation)
           :modifiers modifier-msgs}
          (concat
            (map (fn [kw] [kw (get-property properties kw)])
                 [:id :mode :duration :space :tile-source :animation :material :blend-mode :particle-orientation
                  :inherit-velocity :max-particle-count :type :start-delay])
            [[:properties (mapv (fn [kw] (let [v (get-in properties [kw :value])]
                                           (assoc v :key kw)))
                                (filter (fn [kw] (.startsWith (name kw) "emitter-key"))
                                        (keys (g/declared-properties EmitterProperties))))]
             [:particle-properties (mapv (fn [kw] (let [v (get-in properties [kw :value])]
                                                    (assoc v :key kw)))
                                         (filter (fn [kw] (.startsWith (name kw) "particle-key"))
                                                 (keys (g/declared-properties ParticleProperties))))]]))))

(defn- attach-modifier [self-id parent-id modifier-id]
  (concat
    (let [conns [[:_node-id :nodes]]]
      (for [[from to] conns]
        (g/connect modifier-id from self-id to)))
    (let [conns [[:node-outline :child-outlines]
                 [:scene :child-scenes]
                 [:pb-msg :modifier-msgs]]]
      (for [[from to] conns]
        (g/connect modifier-id from parent-id to)))))

(g/defnode EmitterNode
  (inherits scene/SceneNode)
  (inherits outline/OutlineNode)
  (inherits EmitterProperties)
  (inherits ParticleProperties)

  (property id g/Str)
  (property mode g/Keyword
            (dynamic edit-type (g/always (->choicebox Particle$PlayMode)))
            (dynamic label (g/always "Play Mode")))
  (property duration g/Num)
  (property space g/Keyword
            (dynamic edit-type (g/always (->choicebox Particle$EmissionSpace)))
            (dynamic label (g/always "Emission Space")))

  (property tile-source (g/protocol resource/Resource)
            (dynamic label (g/always "Image"))
            (value (gu/passthrough tile-source-resource))
            (set (project/gen-resource-setter [[:resource :tile-source-resource]
                                               [:texture-set-data :texture-set-data]
                                               [:gpu-texture :gpu-texture]
                                               [:anim-data :anim-data]]))
            (validate (validation/validate-resource tile-source "Missing image"
                                                    [texture-set-data gpu-texture anim-data])))

  (property animation g/Str
            (validate (validation/validate-animation animation anim-data))
            (dynamic edit-type
                     (g/fnk [anim-data] {:type :choicebox
                                         :options (or (and anim-data (not (g/error? anim-data)) (zipmap (keys anim-data) (keys anim-data))) {})})))
  
  (property material (g/protocol resource/Resource)
            (value (gu/passthrough material-resource))
            (set (project/gen-resource-setter [[:resource :material-resource]]))
            (validate (validation/validate-resource material)))

  (property blend-mode g/Keyword
            (dynamic tip (validation/blend-mode-tip blend-mode Particle$BlendMode))
            (dynamic edit-type (g/always (->choicebox Particle$BlendMode))))

  (property particle-orientation g/Keyword (dynamic edit-type (g/always (->choicebox Particle$ParticleOrientation))))
  (property inherit-velocity g/Num)
  (property max-particle-count g/Int)
  (property type g/Keyword
            (dynamic edit-type (g/always (->choicebox Particle$EmitterType)))
            (dynamic label (g/always "Emitter Type")))
  (property start-delay g/Num)

  (display-order [:id scene/SceneNode :mode :space :duration :start-delay :tile-source :animation :material :blend-mode
                  :max-particle-count :type :particle-orientation :inherit-velocity ["Particle" ParticleProperties]])

  (input tile-source-resource (g/protocol resource/Resource))
  (input material-resource (g/protocol resource/Resource))
  (input texture-set-data g/Any)
  (input gpu-texture g/Any)
  (input anim-data g/Any)
  (input child-scenes g/Any :array)
  (input modifier-msgs g/Any :array)

  (output scene g/Any :cached produce-emitter-scene)
  (output pb-msg g/Any :cached produce-emitter-pb)
  (output node-outline outline/OutlineData :cached
    (g/fnk [_node-id id child-outlines]
      (let [pfx-id (core/scope _node-id)]
        {:node-id _node-id
         :label id
         :icon emitter-icon
         :children child-outlines
         :child-reqs [{:node-type ModifierNode
                       :tx-attach-fn (fn [self-id child-id]
                                       (attach-modifier pfx-id self-id child-id))}]})))
  (output aabb AABB (g/fnk [type emitter-key-size-x emitter-key-size-y emitter-key-size-z]
                           (let [[x y z] (mapv props/sample [emitter-key-size-x emitter-key-size-y emitter-key-size-z])
                                 [w h d] (case type
                                           :emitter-type-circle [x x x]
                                           :emitter-type-box [x y z]
                                           :emitter-type-sphere [x x x]
                                           :emitter-type-cone [x y x]
                                           :emitter-type-2dcone [x y x])]
                             (-> (geom/null-aabb)
                               (geom/aabb-incorporate (- w) (- h) (- d))
                               (geom/aabb-incorporate w h d)))))
  (output emitter-sim-data g/Any :cached
          (g/fnk [animation texture-set-data gpu-texture]
                 (let [tex-set (:texture-set texture-set-data)
                       texture-set-anim (first (filter #(= animation (:id %)) (:animations tex-set)))
                       ^ByteString tex-coords (:tex-coords tex-set)
                       tex-coords-buffer (ByteBuffer/allocateDirect (.size tex-coords))]
                   (.put tex-coords-buffer (.asReadOnlyByteBuffer tex-coords))
                   (.flip tex-coords-buffer)
                   {:gpu-texture gpu-texture
                    :texture-set-anim texture-set-anim
                    :tex-coords tex-coords-buffer}))))

(g/defnk produce-save-data [resource pb-data]
  {:resource resource
   :content (protobuf/map->str Particle$ParticleFX pb-data)})

(defn- build-pb [self basis resource dep-resources user-data]
  (let [pb  (:pb user-data)
        pb  (reduce (fn [pb [label resource]]
                      (assoc-in pb label resource))
                    pb (map (fn [[label res]]
                              [label (resource/proj-path (get dep-resources res))])
                            (:dep-resources user-data)))]
    {:resource resource :content (protobuf/map->bytes Particle$ParticleFX pb)}))

(def ^:private resource-fields [[:emitters :tile-source] [:emitters :material]])

(g/defnk produce-build-targets [_node-id project-id resource rt-pb-data dep-build-targets]
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
    [{:node-id _node-id
      :resource (workspace/make-build-resource resource)
      :build-fn build-pb
      :user-data {:pb rt-pb-data
                  :dep-resources dep-resources}
      :deps dep-build-targets}]))

; TODO - un-hardcode
(def ^:private max-emitter-count 32)
(def ^:private max-particle-count 512)

(defn- convert-blend-mode [blend-mode-index]
  (protobuf/pb-enum->val (.getValueDescriptor (Particle$BlendMode/valueOf ^int blend-mode-index))))

(defn- render-emitter [emitter-sim-data ^GL gl render-args vtx-binding view-proj emitter-index blend-mode v-index v-count]
  (let [gpu-texture (:gpu-texture (get emitter-sim-data emitter-index))
        blend-mode (convert-blend-mode blend-mode)]
    (gl/with-gl-bindings gl render-args [gpu-texture plib/shader vtx-binding]
      (case blend-mode
        :blend-mode-alpha (.glBlendFunc gl GL/GL_ONE GL/GL_ONE_MINUS_SRC_ALPHA)
        (:blend-mode-add :blend-mode-add-alpha) (.glBlendFunc gl GL/GL_ONE GL/GL_ONE)
        :blend-mode-mult (.glBlendFunc gl GL/GL_ZERO GL/GL_SRC_COLOR))
      (shader/set-uniform plib/shader gl "texture" 0)
      (shader/set-uniform plib/shader gl "view_proj" view-proj)
      (shader/set-uniform plib/shader gl "tint" (Vector4f. 1 1 1 1))
      (gl/gl-draw-arrays gl GL/GL_TRIANGLES v-index v-count)
      (.glBlendFunc gl GL/GL_SRC_ALPHA GL/GL_ONE_MINUS_SRC_ALPHA))))

(defn- render-pfx [^GL2 gl render-args renderables count]
  (doseq [renderable renderables
          :let [node-id (last (:node-path renderable))]]
    (when-let [pfx-sim-ref (:pfx-sim (scene-cache/lookup-object ::pfx-sim node-id nil))]
      (let [pfx-sim @pfx-sim-ref
            user-data (:user-data renderable)
            render-emitter-fn (:render-emitter-fn user-data)
            vtx-binding (:vtx-binding pfx-sim)
            camera (:camera render-args)
            view-proj (doto (Matrix4d.)
                        (.mul (camera/camera-projection-matrix camera) (camera/camera-view-matrix camera)))]
        (plib/render pfx-sim (partial render-emitter-fn gl render-args vtx-binding view-proj))))))

(g/defnk produce-scene [_node-id child-scenes rt-pb-data fetch-anim-fn render-emitter-fn]
  {:node-id _node-id
   :node-path [_node-id]
   :updatable {:update-fn (g/fnk [dt world-transform]
                                 (let [data [max-emitter-count max-particle-count rt-pb-data world-transform]
                                       pfx-sim-ref (:pfx-sim (scene-cache/request-object! ::pfx-sim _node-id nil data))]
                                   (swap! pfx-sim-ref plib/simulate dt fetch-anim-fn [world-transform])))
               :name "ParticleFX"}
   :renderable {:render-fn render-pfx
                :batch-key nil
                :user-data {:render-emitter-fn render-emitter-fn}
                :passes [pass/transparent pass/selection]}
   :aabb (reduce geom/aabb-union (geom/null-aabb) (filter #(not (nil? %)) (map :aabb child-scenes)))
   :children child-scenes})

(defn- attach-emitter [self-id emitter-id]
  (let [conns [[:_node-id :nodes]
               [:node-outline :child-outlines]
               [:scene :child-scenes]
               [:pb-msg :emitter-msgs]
               [:emitter-sim-data :emitter-sim-data]]]
    (for [[from to] conns]
      (g/connect emitter-id from self-id to))))

(g/defnode ParticleFXNode
  (inherits project/ResourceNode)

  (input dep-build-targets g/Any :array)
  (input emitter-msgs g/Any :array)
  (input modifier-msgs g/Any :array)
  (input child-scenes g/Any :array)
  (input emitter-sim-data g/Any :array)

  (output save-data g/Any :cached produce-save-data)
  (output pb-data g/Any :cached (g/fnk [emitter-msgs modifier-msgs]
                                       {:emitters emitter-msgs :modifiers modifier-msgs}))
  (output rt-pb-data g/Any :cached (g/fnk [pb-data] (particle-fx-transform pb-data)))
  (output emitter-sim-data g/Any :cached (g/fnk [emitter-sim-data] emitter-sim-data))
  (output build-targets g/Any :cached produce-build-targets)
  (output scene g/Any :cached produce-scene)
  (output node-outline outline/OutlineData :cached (g/fnk [_node-id child-outlines]
                                                     {:node-id _node-id
                                                      :label "ParticleFX"
                                                      :icon particle-fx-icon
                                                      :children child-outlines
                                                      :child-reqs [{:node-type EmitterNode
                                                                    :tx-attach-fn attach-emitter}
                                                                   {:node-type ModifierNode
                                                                    :tx-attach-fn (fn [self-id child-id]
                                                                                    (attach-modifier self-id self-id child-id))}]}))
  (output fetch-anim-fn Runnable :cached (g/fnk [emitter-sim-data] (fn [index] (get emitter-sim-data index))))
  (output render-emitter-fn Runnable :cached (g/fnk [emitter-sim-data] (partial render-emitter emitter-sim-data))))

(defn- emitter? [node-id]
  (when (g/node-instance? EmitterNode node-id)
    node-id))

(defn- pfx? [node-id node-type]
  (if (g/node-instance? ParticleFXNode node-id)
    node-id
    (when (contains? (g/declared-inputs node-type) :source-id)
      (let [source-id (g/node-value node-id :source-id)]
        (when (g/node-instance? ParticleFXNode source-id)
          source-id)))))

(defn- v4->euler [v]
  (math/quat->euler (doto (Quat4d.) (math/clj->vecmath v))))

(defn- make-modifier
  ([self parent-id modifier]
    (make-modifier self parent-id modifier false))
  ([self parent-id modifier select?]
    (let [graph-id (g/node-id->graph-id self)]
      (g/make-nodes graph-id
                    [mod-node [ModifierNode :position (:position modifier) :rotation (v4->euler (:rotation modifier))
                               :type (:type modifier)]]
                    (let [mod-properties (into {} (map #(do [(:key %) (dissoc % :key)])
                                                       (:properties modifier)))]
                      (concat
                        (g/set-property mod-node :magnitude (if-let [prop (:modifier-key-magnitude mod-properties)]
                                                              (props/map->CurveSpread (:modifier-key-magnitude mod-properties))
                                                              props/default-curve-spread))
                        (g/set-property mod-node :max-distance (if-let [prop (:modifier-key-max-distance mod-properties)]
                                                                 (props/map->Curve (:modifier-key-max-distance mod-properties))
                                                                 props/default-curve))))
                    (attach-modifier self parent-id mod-node)
                    (if select?
                      (let [project (project/get-project self)]
                        (project/select project [mod-node]))
                      [])))))

(defn- add-modifier-handler [self parent-id type]
  (when-let [modifier (get-in mod-types [type :template])]
    (g/transact
      (concat
        (g/operation-label "Add Modifier")
        (make-modifier self parent-id modifier true)))))

(handler/defhandler :add-secondary :global
  (label [user-data] (if-not user-data
                       "Add Modifier"
                       (get-in user-data [:modifier-data :label])))
  (active? [selection] (and (= 1 (count selection))
                            (let [node-id (first selection)
                                  type (g/node-type (g/node-by-id node-id))]
                              (or (emitter? node-id)
                                  (pfx? node-id type)))))
  (run [user-data]
       (let [parent-id (:_node-id user-data)
             self (if (emitter? parent-id)
                    (core/scope parent-id)
                    parent-id)]
         (add-modifier-handler self parent-id (:modifier-type user-data))))
  (options [selection user-data]
           (when (not user-data)
             (let [self (let [node-id (first selection)
                              type (g/node-type (g/node-by-id node-id))]
                          (or (emitter? node-id)
                              (pfx? node-id type)))]
               (mapv (fn [[type data]] {:label (:label data)
                                        :icon modifier-icon
                                        :command :add-secondary
                                        :user-data {:_node-id self :modifier-type type :modifier-data data}}) mod-types)))))

(defn- make-emitter
  ([self emitter]
    (make-emitter self emitter false))
  ([self emitter select?]
    (let [project   (project/get-project self)
          workspace (project/workspace project)
          graph-id (g/node-id->graph-id self)
          tile-source (workspace/resolve-workspace-resource workspace (:tile-source emitter))
          material (workspace/resolve-workspace-resource workspace (:material emitter))]
      (g/make-nodes graph-id
                    [emitter-node [EmitterNode :position (:position emitter) :rotation (v4->euler (:rotation emitter))
                                   :id (:id emitter) :mode (:mode emitter) :duration (:duration emitter) :space (:space emitter)
                                   :tile-source tile-source :animation (:animation emitter) :material material
                                   :blend-mode (:blend-mode emitter) :particle-orientation (:particle-orientation emitter)
                                   :inherit-velocity (:inherit-velocity emitter) :max-particle-count (:max-particle-count emitter)
                                   :type (:type emitter) :start-delay (:start-delay emitter)]]
                    (let [emitter-properties (into {} (map #(do [(:key %) (select-keys % [:points :spread])]) (:properties emitter)))]
                      (for [key (keys (g/declared-properties EmitterProperties))
                            :when (contains? emitter-properties key)]
                        (g/set-property emitter-node key (props/map->CurveSpread (get emitter-properties key)))))
                    (let [particle-properties (into {} (map #(do [(:key %) (select-keys % [:points])]) (:particle-properties emitter)))]
                      (for [key (keys (g/declared-properties ParticleProperties))
                            :when (contains? particle-properties key)]
                        (g/set-property emitter-node key (props/map->Curve (get particle-properties key)))))
                    (attach-emitter self emitter-node)
                    (for [modifier (:modifiers emitter)]
                      (make-modifier self emitter-node modifier))
                    (if select?
                      (project/select project [emitter-node])
                      [])))))

(defn- add-emitter-handler [self type]
  (when-let [resource (io/resource emitter-template)]
      (let [emitter (protobuf/read-text Particle$Emitter resource)]
        (g/transact
          (concat
            (g/operation-label "Add Emitter")
            (make-emitter self (assoc emitter :type type) true))))))

(handler/defhandler :add :global
  (label [user-data] (if-not user-data
                       "Add Emitter"
                       (get-in user-data [:emitter-data :label])))
  (active? [selection] (and (= 1 (count selection))
                            (let [node-id (first selection)
                                  type (g/node-type (g/node-by-id node-id))]
                              (pfx? node-id type))))
  (run [user-data] (add-emitter-handler (:_node-id user-data) (:emitter-type user-data)))
  (options [selection user-data]
           (when (not user-data)
             (let [self (let [node-id (first selection)
                              type (g/node-type (g/node-by-id node-id))]
                          (pfx? node-id type))]
               (mapv (fn [[type data]] {:label (:label data)
                                        :icon emitter-icon
                                        :command :add
                                        :user-data {:_node-id self :emitter-type type :emitter-data data}}) emitter-types)))))

(defn- connect-build-targets [self project path]
  (let [resource (workspace/resolve-resource (g/node-value self :resource) path)]
    (project/connect-resource-node project resource self [[:build-targets :dep-build-targets]])))

(defn load-particle-fx [project self resource]
  (let [pb (protobuf/read-text Particle$ParticleFX resource)
        graph-id (g/node-id->graph-id self)]
    (concat
      (for [emitter (:emitters pb)]
        (make-emitter self emitter))
      (for [modifier (:modifiers pb)]
        (make-modifier self self modifier))
      (for [res [[:emitters :tile-source] [:emitters :material]]]
        (for [v (get pb (first res))]
          (let [path (if (second res) (get v (second res)) v)]
            (connect-build-targets self project path)))))))

(defn register-resource-types [workspace]
  (workspace/register-resource-type workspace
                                     :ext "particlefx"
                                     :label "Particle FX"
                                     :node-type ParticleFXNode
                                     :load-fn load-particle-fx
                                     :icon particle-fx-icon
                                     :tags #{:component}
                                     :view-types [:scene]
                                     :view-opts {:scene {:grid true}}))

(defn- make-pfx-sim [_ data]
  (let [[max-emitter-count max-particle-count rt-pb-data world-transform] data]
    {:pfx-sim (atom (plib/make-sim max-emitter-count max-particle-count rt-pb-data [world-transform]))
     :protobuf-msg rt-pb-data}))

(defn- update-pfx-sim [_ pfx-sim data]
  (let [[max-emitter-count max-particle-count rt-pb-data world-transform] data
        pfx-sim-ref (:pfx-sim pfx-sim)]
    (when (not= rt-pb-data (:protobuf-msg pfx-sim))
      (swap! pfx-sim-ref plib/reload rt-pb-data))
    (assoc pfx-sim :protobuf-msg rt-pb-data)))

(defn- destroy-pfx-sims [_ pfx-sims _]
  (doseq [pfx-sim pfx-sims]
    (plib/destroy-sim @(:pfx-sim pfx-sim))))

(scene-cache/register-object-cache! ::pfx-sim make-pfx-sim update-pfx-sim destroy-pfx-sims)
