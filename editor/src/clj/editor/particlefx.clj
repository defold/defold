;; Copyright 2020-2025 The Defold Foundation
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

(ns editor.particlefx
  (:require [clojure.java.io :as io]
            [dynamo.graph :as g]
            [editor.app-view :as app-view]
            [editor.attachment :as attachment]
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
            [editor.gl.vertex2 :as vtx]
            [editor.graph-util :as gu]
            [editor.graphics :as graphics]
            [editor.handler :as handler]
            [editor.id :as id]
            [editor.localization :as localization]
            [editor.material :as material]
            [editor.math :as math]
            [editor.outline :as outline]
            [editor.particle-lib :as plib]
            [editor.properties :as properties]
            [editor.protobuf :as protobuf]
            [editor.resource :as resource]
            [editor.resource-node :as resource-node]
            [editor.scene :as scene]
            [editor.scene-cache :as scene-cache]
            [editor.scene-picking :as scene-picking]
            [editor.scene-tools :as scene-tools]
            [editor.types :as types]
            [editor.validation :as validation]
            [editor.workspace :as workspace]
            [util.coll :as coll :refer [pair]])
  (:import [com.dynamo.particle.proto Particle$BlendMode Particle$EmissionSpace Particle$Emitter Particle$Emitter$ParticleProperty Particle$Emitter$Property Particle$EmitterKey Particle$EmitterType Particle$Modifier Particle$Modifier$Property Particle$ParticleFX Particle$ParticleKey Particle$ParticleOrientation Particle$PlayMode Particle$SizeMode Particle$SplinePoint]
           [com.google.protobuf ByteString]
           [com.jogamp.opengl GL GL2]
           [editor.gl.shader ShaderLifecycle]
           [editor.gl.vertex2 VertexBuffer]
           [editor.properties Curve CurveSpread]
           [editor.types AABB]
           [java.nio ByteBuffer ByteOrder]
           [javax.vecmath Matrix3d Matrix4d Point3d Quat4d Vector3d]))

(set! *warn-on-reflection* true)

(def particle-fx-icon "icons/32/Icons_17-ParticleFX.png")
(def emitter-icon "icons/32/Icons_18-ParticleFX-emitter.png")
(def emitter-template "templates/template.emitter")
(def modifier-icon "icons/32/Icons_19-ParicleFX-Modifier.png")

(def particlefx-ext "particlefx")

(defn particle-fx-transform [pb]
  (let [xform (fn [v]
                (let [p (doto (Point3d.) (math/clj->vecmath (:position v scene/default-position)))
                      r (doto (Quat4d.) (math/clj->vecmath (:rotation v scene/default-rotation)))]
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

(defn- select-attribute-values [pb-data key]
  (protobuf/sanitize-repeated
    pb-data :emitters
    (fn [emitter]
      (-> emitter
          (dissoc :attributes-save-values)
          (dissoc :attributes-build-target)
          (protobuf/assign-repeated :attributes (get emitter key))))))

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

(defn- curve->pb-spline-points [curve]
  (->> curve
       (properties/curve-vals)
       (sort-by first)
       (mapv (fn [[x y t-x t-y]]
               (protobuf/make-map-without-defaults Particle$SplinePoint
                 :x (float x)
                 :y (float y)
                 :t-x (float t-x)
                 :t-y (float t-y))))))

(defn- pb-spline-point->control-point [pb-spline-point]
  {:pre [(map? pb-spline-point)]} ; Particle$SplinePoint in map format.
  (let [x (protobuf/intern-float (:x pb-spline-point protobuf/float-zero))
        y (protobuf/intern-float (:y pb-spline-point protobuf/float-zero))
        t-x (protobuf/intern-float (:t-x pb-spline-point protobuf/float-one))
        t-y (protobuf/intern-float (:t-y pb-spline-point protobuf/float-zero))]
    (if (and (identical? protobuf/float-zero x)
             (identical? protobuf/float-zero y)
             (identical? protobuf/float-one t-x)
             (identical? protobuf/float-zero t-y))
      properties/default-control-point
      [x y t-x t-y])))

(defn- pb-property->curve [pb-property]
  {:pre [(map? pb-property)]} ; Particle$Emitter$Property, Particle$Emitter$ParticleProperty, or Particle$Modifier$Property in map format.
  (properties/->curve (some->> (:points pb-property)
                               (map pb-spline-point->control-point))))

(defn- pb-property->curve-spread [pb-property]
  {:pre [(map? pb-property)]} ; Particle$Emitter$Property or Particle$Modifier$Property in map format.
  (properties/->curve-spread (some->> (:points pb-property)
                                      (map pb-spline-point->control-point))
                             (:spread pb-property)))

(defn- curve->pb-property [^Class property-pb-class property-key curve]
  (let [points (curve->pb-spline-points curve)
        spread (or (:spread curve) properties/default-spread)]
    (if (= properties/default-spread spread)
      (protobuf/make-map-without-defaults property-pb-class
        :key property-key
        :points points)
      (protobuf/make-map-without-defaults property-pb-class
        :key property-key
        :points points
        :spread spread))))

(def ^:private default-pb-spline-point (protobuf/default-message Particle$SplinePoint #{:required}))

(defn- significant-pb-property? [pb-property]
  {:pre [(map? pb-property)]} ; Particle$Emitter$Property, Particle$Emitter$ParticleProperty, or Particle$Modifier$Property in map format.
  (let [points (:points pb-property)
        spread (:spread pb-property properties/default-spread)]
    (or (not= properties/default-spread spread)
        (< 1 (count points))
        (if-let [point (first points)]
          (not= default-pb-spline-point point)
          false))))

(defn- modifier-type-has-max-distance? [modifier-type]
  (case modifier-type
    (:modifier-type-radial :modifier-type-vortex) true
    false))

(g/defnk produce-modifier-pb
  [position rotation type magnitude max-distance use-direction]
  ;; TODO(save-value-cleanup): Maybe we should strip these like the rest of the
  ;; properties elsewhere? See the comment in sanitize-modifier below.
  (let [properties
        (into []
              (map (fn [[property-key curve]]
                     (curve->pb-property Particle$Modifier$Property property-key curve)))
              (cond-> {:modifier-key-magnitude magnitude}
                      (modifier-type-has-max-distance? type)
                      (assoc :modifier-key-max-distance max-distance)))]

    (protobuf/make-map-without-defaults Particle$Modifier
      :position position
      :rotation rotation
      :type type
      :use-direction (protobuf/boolean->int use-direction)
      :properties properties)))

(def ^:private type->vcount
  {:emitter-type-2dcone 6
   :emitter-type-circle 6 #_(* 2 circle-steps)})

(defn- ->vbuf
  ^VertexBuffer [vs vcount color]
  (let [^VertexBuffer vbuf (->color-vtx vcount)
        ^ByteBuffer buf (.buf vbuf)]
    (doseq [v vs]
      (vtx/buf-push-floats! buf v)
      (vtx/buf-push-floats! buf color))
    (vtx/flip! vbuf)))

(defn- orthonormalize [^Matrix4d m]
  (let [m' (Matrix4d. m)
        r (Matrix3d.)]
    (.get m' r)
    (.setRotationScale m' r)
    m'))

(defn render-lines [^GL2 gl render-args renderables _rcount]
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
            color (colors/renderable-outline-color renderable)
            vs (into (vec (geom/transf-p world-transform-no-scale (geom/scale scale-f vs-screen)))
                     (geom/transf-p world-transform vs-world))
            render-args (if (= pass/selection (:pass render-args))
                          (assoc render-args :id (scene-picking/renderable-picking-id-uniform renderable))
                          render-args)
            vertex-binding (vtx/use-with ::lines (->vbuf vs vcount color) shader)]
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

(def ^:private mod-types
  {:modifier-type-acceleration {:label "Acceleration"
                                :message (localization/message "command.edit.add-secondary-embedded-component.variant.particlefx.option.acceleration")
                                :template {:type :modifier-type-acceleration
                                           :properties [{:key :modifier-key-magnitude
                                                         :points [{:y (float -100.0)}]}]}
                                :geom-data-screen (fn [magnitude _]
                                                    (if (< magnitude 0)
                                                      acceleration-neg-geom-data
                                                      acceleration-geom-data))
                                :geom-data-world (constantly [])}
   :modifier-type-drag {:label "Drag"
                        :message (localization/message "command.edit.add-secondary-embedded-component.variant.particlefx.option.drag")
                        :template {:type :modifier-type-drag
                                   :properties [{:key :modifier-key-magnitude
                                                 :points [{:y (float 1.0)}]}]}
                        :geom-data-screen (constantly drag-geom-data)
                        :geom-data-world (constantly [])}
   :modifier-type-radial {:label "Radial"
                          :message (localization/message "command.edit.add-secondary-embedded-component.variant.particlefx.option.radial")
                          :template {:type :modifier-type-radial
                                     :properties [{:key :modifier-key-magnitude
                                                   :points [{:y (float 100.0)}]}
                                                  {:key :modifier-key-max-distance
                                                   :points [{:y (float 1000.0)}]}]}
                          :geom-data-screen (fn [magnitude _]
                                              (if (< magnitude 0)
                                                radial-neg-geom-data
                                                radial-geom-data))
                          :geom-data-world (fn [_ max-distance]
                                             (geom/scale [max-distance max-distance 1] dash-circle))}
   :modifier-type-vortex {:label "Vortex"
                          :message (localization/message "command.edit.add-secondary-embedded-component.variant.particlefx.option.vortex")
                          :template {:type :modifier-type-vortex
                                     :properties [{:key :modifier-key-magnitude
                                                   :points [{:y (float 100.0)}]}
                                                  {:key :modifier-key-max-distance
                                                   :points [{:y (float 1000.0)}]}]}
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
        magnitude (properties/sample magnitude)
        max-distance (properties/sample max-distance)]
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

  (property type g/Keyword (dynamic visible (g/constantly false))) ; Required protobuf field.
  (property use-direction g/Bool (default (protobuf/int->boolean (protobuf/default Particle$Modifier :use-direction)))
            (dynamic visible (g/constantly false)))
  (property magnitude CurveSpread (default properties/default-curve-spread)
            (dynamic label (properties/label-dynamic :particlefx :magnitude))
            (dynamic tooltip (properties/tooltip-dynamic :particlefx :magnitude)))
  (property max-distance Curve (default properties/default-curve)
            (dynamic label (properties/label-dynamic :particlefx :max-distance))
            (dynamic tooltip (properties/tooltip-dynamic :particlefx :max-distance))
            (dynamic visible (g/fnk [type] (contains? #{:modifier-type-radial :modifier-type-vortex} type))))
  (property node-outline-key g/Str (dynamic visible (g/constantly false))) ; Either assigned in load-fn or generated when attached

  (output transform-properties g/Any scene/produce-unscalable-transform-properties)
  (output pb-msg g/Any produce-modifier-pb)
  (output node-outline outline/OutlineData :cached
    (g/fnk [_node-id type node-outline-key]
      (let [mod-type (mod-types type)]
        {:node-id _node-id
         :node-outline-key node-outline-key
         :label (:message mod-type)
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

(def emitter-types {:emitter-type-circle {:label (localization/message "command.edit.add-embedded-component.variant.particlefx.option.circle")
                                          :geom-data-world (fn [size-x _ _]
                                                             (geom/scale [size-x size-x 1] circle-geom-data))}
                    :emitter-type-sphere {:label (localization/message "command.edit.add-embedded-component.variant.particlefx.option.sphere")
                                          :geom-data-world (fn [size-x _ _]
                                                             (geom/scale [size-x size-x 1] circle-geom-data))}
                    :emitter-type-cone {:label (localization/message "command.edit.add-embedded-component.variant.particlefx.option.cone")
                                        :geom-data-world (fn [size-x size-y _]
                                                           (geom/scale [size-x size-y 1] cone-geom-data))}
                    :emitter-type-2dcone {:label (localization/message "command.edit.add-embedded-component.variant.particlefx.option.2d-cone")
                                          :geom-data-world (fn [size-x size-y _]
                                                             (geom/scale [size-x size-y 1] cone-geom-data))}
                    :emitter-type-box {:label (localization/message "command.edit.add-embedded-component.variant.particlefx.option.box")
                                       :geom-data-world (fn [size-x size-y _]
                                                          (geom/scale [size-x size-y 1] box-geom-data))}})

(defn- convert-blend-mode [^long blend-mode-index]
  (protobuf/pb-enum->val (.getValueDescriptor (Particle$BlendMode/forNumber blend-mode-index))))

(defn- render-emitters-sim [^GL2 gl render-args renderables _rcount]
  (doseq [renderable renderables]
    (let [{:keys [color emitter-index emitter-sim-data material-attribute-infos max-particle-count vertex-attribute-bytes]} (:user-data renderable)]
      (when-let [shader (:shader emitter-sim-data)]
        (let [shader-attribute-infos-by-name (shader/attribute-infos shader gl)
              manufactured-attribute-keys [:position :texcoord0 :page-index :color]
              shader-bound-attributes (graphics/shader-bound-attributes shader-attribute-infos-by-name material-attribute-infos manufactured-attribute-keys :coordinate-space-world)
              vertex-description (graphics/make-vertex-description shader-bound-attributes)
              pfx-sim-request-id (some-> renderable :updatable :node-id)]
          (when-let [pfx-sim-atom (when (and emitter-sim-data pfx-sim-request-id)
                                    (:pfx-sim (scene-cache/lookup-object ::pfx-sim pfx-sim-request-id nil)))]
            (let [pfx-sim @pfx-sim-atom
                  raw-vbuf (plib/gen-emitter-vertex-data pfx-sim emitter-index color max-particle-count vertex-description shader-bound-attributes vertex-attribute-bytes)
                  context (:context pfx-sim)
                  vbuf (vtx/wrap-vertex-buffer vertex-description :static raw-vbuf)
                  all-raw-vbufs (assoc (:raw-vbufs pfx-sim) emitter-index raw-vbuf)]
              ;; TODO: We have to update the "raw-vbufs" here because we will
              ;;       at some point create the GPU vertex buffer from this call.
              ;;       This is due to the fact that we don't know how big the vertex buffer
              ;;       will be until we have a valid GL context: We don't have the correct information
              ;;       until the shader has been compiled. In the future we could perhaps use the meta-data
              ;;       we get from the SPIR-V toolchain to figure this out beforehand.
              (swap! pfx-sim-atom assoc :raw-vbufs all-raw-vbufs)
              (when-let [render-data (plib/render-emitter pfx-sim emitter-index)]
                (let [gpu-texture (:gpu-texture emitter-sim-data)
                      vtx-binding (vtx/use-with context vbuf shader)
                      blend-mode (convert-blend-mode (:blend-mode render-data))]
                  (gl/with-gl-bindings gl render-args [shader vtx-binding gpu-texture]
                    (shader/set-samplers-by-index shader gl 0 (:texture-units gpu-texture))
                    (gl/set-blend-mode gl blend-mode)
                    (gl/gl-draw-arrays gl GL/GL_TRIANGLES (:v-index render-data) (:v-count render-data))
                    (.glBlendFunc gl GL/GL_SRC_ALPHA GL/GL_ONE_MINUS_SRC_ALPHA)))))))))))

(defn- render-emitters [^GL2 gl render-args renderables _rcount]
  (let [pass (:pass render-args)]
    (condp = pass
      pass/selection (render-lines gl render-args renderables count)
      pass/transparent (render-emitters-sim gl render-args renderables count))))

(g/defnk produce-emitter-aabb [type emitter-key-size-x emitter-key-size-y emitter-key-size-z]
  (case type
    (:emitter-type-2dcone :emitter-type-cone)
    (let [+bx (* 0.5 (properties/sample emitter-key-size-x))
          +by (properties/sample emitter-key-size-y)
          -bx (- +bx)]
      (geom/coords->aabb [-bx 0.0 (case type :emitter-type-2dcone 0.0 -bx)]
                         [+bx +by (case type :emitter-type-2dcone 0.0 +bx)]))

    :emitter-type-box
    (let [+bx (* 0.5 (properties/sample emitter-key-size-x))
          +by (* 0.5 (properties/sample emitter-key-size-y))
          +bz (* 0.5 (properties/sample emitter-key-size-z))
          -bx (- +bx)
          -by (- +by)
          -bz (- +bz)]
      (geom/coords->aabb [-bx -by -bz]
                         [+bx +by +bz]))

    (:emitter-type-circle :emitter-type-sphere)
    (let [+bx (properties/sample emitter-key-size-x)
          -bx (- +bx)]
      (geom/coords->aabb [-bx -bx (case type :emitter-type-circle 0.0 -bx)]
                         [+bx +bx (case type :emitter-type-circle 0.0 +bx)]))))

(defn- property-range
  ^double [prop ^double default]
  (if (nil? prop)
    default
    (let [[^double -r ^double +r] (properties/sample-range prop)]
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
            radians (if (zero? emitter-height)
                      (* 0.5 Math/PI)
                      (Math/atan (/ emitter-radius emitter-height)))
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
  [_node-id id transform aabb visibility-aabb type emitter-sim-data emitter-index emitter-key-size-x emitter-key-size-y emitter-key-size-z child-scenes material-attribute-infos max-particle-count vertex-attribute-bytes]
  (let [emitter-type (emitter-types type)
        user-data {:type type
                   :emitter-sim-data emitter-sim-data
                   :emitter-index emitter-index
                   :material-attribute-infos material-attribute-infos
                   :max-particle-count max-particle-count
                   :vertex-attribute-bytes vertex-attribute-bytes
                   :color [1.0 1.0 1.0 1.0]
                   :geom-data-world (apply (:geom-data-world emitter-type)
                                           (mapv properties/sample [emitter-key-size-x emitter-key-size-y emitter-key-size-z]))}]
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

(defn- pb-property-msgs [^Class property-key-enum-pb-class ^Class property-pb-class property-key->curve]
  (into []
        (comp (map first)
              (keep (fn [property-key]
                      (when-some [curve (property-key->curve property-key)]
                        (let [pb-property (curve->pb-property property-pb-class property-key curve)]
                          (when (significant-pb-property? pb-property)
                            pb-property))))))
        (butlast (protobuf/enum-values property-key-enum-pb-class))))

(g/defnode EmitterProperties
  (property emitter-key-spawn-rate CurveSpread (default properties/default-curve-spread)
            (dynamic label (properties/label-dynamic :particlefx :emitter-key-spawn-rate))
            (dynamic tooltip (properties/tooltip-dynamic :particlefx :emitter-key-spawn-rate)))
  (property emitter-key-size-x CurveSpread (default properties/default-curve-spread)
            (dynamic label (properties/label-dynamic :particlefx :emitter-key-size-x))
            (dynamic tooltip (properties/tooltip-dynamic :particlefx :emitter-key-size-x)))
  (property emitter-key-size-y CurveSpread (default properties/default-curve-spread)
            (dynamic label (properties/label-dynamic :particlefx :emitter-key-size-y))
            (dynamic tooltip (properties/tooltip-dynamic :particlefx :emitter-key-size-y)))
  (property emitter-key-size-z CurveSpread (default properties/default-curve-spread)
            (dynamic label (properties/label-dynamic :particlefx :emitter-key-size-z))
            (dynamic tooltip (properties/tooltip-dynamic :particlefx :emitter-key-size-z)))
  (property emitter-key-particle-life-time CurveSpread (default properties/default-curve-spread)
            (dynamic label (properties/label-dynamic :particlefx :emitter-key-particle-life-time))
            (dynamic tooltip (properties/tooltip-dynamic :particlefx :emitter-key-particle-life-time)))
  (property emitter-key-particle-speed CurveSpread (default properties/default-curve-spread)
            (dynamic label (properties/label-dynamic :particlefx :emitter-key-particle-speed))
            (dynamic tooltip (properties/tooltip-dynamic :particlefx :emitter-key-particle-speed)))
  (property emitter-key-particle-size CurveSpread (default properties/default-curve-spread)
            (dynamic label (properties/label-dynamic :particlefx :emitter-key-particle-size))
            (dynamic tooltip (properties/tooltip-dynamic :particlefx :emitter-key-particle-size)))
  (property emitter-key-particle-red CurveSpread (default properties/default-curve-spread)
            (dynamic label (properties/label-dynamic :particlefx :emitter-key-particle-red))
            (dynamic tooltip (properties/tooltip-dynamic :particlefx :emitter-key-particle-red)))
  (property emitter-key-particle-green CurveSpread (default properties/default-curve-spread)
            (dynamic label (properties/label-dynamic :particlefx :emitter-key-particle-green))
            (dynamic tooltip (properties/tooltip-dynamic :particlefx :emitter-key-particle-green)))
  (property emitter-key-particle-blue CurveSpread (default properties/default-curve-spread)
            (dynamic label (properties/label-dynamic :particlefx :emitter-key-particle-blue))
            (dynamic tooltip (properties/tooltip-dynamic :particlefx :emitter-key-particle-blue)))
  (property emitter-key-particle-alpha CurveSpread (default properties/default-curve-spread)
            (dynamic label (properties/label-dynamic :particlefx :emitter-key-particle-alpha))
            (dynamic tooltip (properties/tooltip-dynamic :particlefx :emitter-key-particle-alpha)))
  (property emitter-key-particle-rotation CurveSpread (default properties/default-curve-spread)
            (dynamic label (properties/label-dynamic :particlefx :emitter-key-particle-rotation))
            (dynamic tooltip (properties/tooltip-dynamic :particlefx :emitter-key-particle-rotation)))
  (property emitter-key-particle-stretch-factor-x CurveSpread (default properties/default-curve-spread)
            (dynamic label (properties/label-dynamic :particlefx :emitter-key-particle-stretch-factor-x))
            (dynamic tooltip (properties/tooltip-dynamic :particlefx :emitter-key-particle-stretch-factor-x)))
  (property emitter-key-particle-stretch-factor-y CurveSpread (default properties/default-curve-spread)
            (dynamic label (properties/label-dynamic :particlefx :emitter-key-particle-stretch-factor-y))
            (dynamic tooltip (properties/tooltip-dynamic :particlefx :emitter-key-particle-stretch-factor-y)))
  (property emitter-key-particle-angular-velocity CurveSpread (default properties/default-curve-spread)
            (dynamic label (properties/label-dynamic :particlefx :emitter-key-particle-angular-velocity))
            (dynamic tooltip (properties/tooltip-dynamic :particlefx :emitter-key-particle-angular-velocity)))

  (output emitter-property-msgs g/Any (g/fnk [emitter-key-spawn-rate
                                              emitter-key-size-x
                                              emitter-key-size-y
                                              emitter-key-size-z
                                              emitter-key-particle-life-time
                                              emitter-key-particle-speed
                                              emitter-key-particle-size
                                              emitter-key-particle-red
                                              emitter-key-particle-green
                                              emitter-key-particle-blue
                                              emitter-key-particle-alpha
                                              emitter-key-particle-rotation
                                              emitter-key-particle-stretch-factor-x
                                              emitter-key-particle-stretch-factor-y
                                              emitter-key-particle-angular-velocity
                                              :as args]
                                        ;; TODO: It would be useful to have something like _declared-properties that produces a key-value map like args without the extra stuff and dynamics.
                                        (pb-property-msgs
                                          Particle$EmitterKey
                                          Particle$Emitter$Property
                                          args))))

(g/defnode ParticleProperties
  (property particle-key-scale Curve (default properties/default-curve)
            (dynamic label (properties/label-dynamic :particlefx :particle-key-scale))
            (dynamic tooltip (properties/tooltip-dynamic :particlefx :particle-key-scale)))
  (property particle-key-red Curve (default properties/default-curve)
            (dynamic label (properties/label-dynamic :particlefx :particle-key-red))
            (dynamic tooltip (properties/tooltip-dynamic :particlefx :particle-key-red)))
  (property particle-key-green Curve (default properties/default-curve)
            (dynamic label (properties/label-dynamic :particlefx :particle-key-green))
            (dynamic tooltip (properties/tooltip-dynamic :particlefx :particle-key-green)))
  (property particle-key-blue Curve (default properties/default-curve)
            (dynamic label (properties/label-dynamic :particlefx :particle-key-blue))
            (dynamic tooltip (properties/tooltip-dynamic :particlefx :particle-key-blue)))
  (property particle-key-alpha Curve (default properties/default-curve)
            (dynamic label (properties/label-dynamic :particlefx :particle-key-alpha))
            (dynamic tooltip (properties/tooltip-dynamic :particlefx :particle-key-alpha)))
  (property particle-key-rotation Curve (default properties/default-curve)
            (dynamic label (properties/label-dynamic :particlefx :particle-key-rotation))
            (dynamic tooltip (properties/tooltip-dynamic :particlefx :particle-key-rotation)))
  (property particle-key-stretch-factor-x Curve (default properties/default-curve)
            (dynamic label (properties/label-dynamic :particlefx :particle-key-stretch-factor-x))
            (dynamic tooltip (properties/tooltip-dynamic :particlefx :particle-key-stretch-factor-x)))
  (property particle-key-stretch-factor-y Curve (default properties/default-curve)
            (dynamic label (properties/label-dynamic :particlefx :particle-key-stretch-factor-y))
            (dynamic tooltip (properties/tooltip-dynamic :particlefx :particle-key-stretch-factor-y)))
  (property particle-key-angular-velocity Curve (default properties/default-curve)
            (dynamic label (properties/label-dynamic :particlefx :particle-key-angular-velocity))
            (dynamic tooltip (properties/tooltip-dynamic :particlefx :particle-key-angular-velocity)))

  (output particle-property-msgs g/Any (g/fnk [particle-key-scale
                                               particle-key-red
                                               particle-key-green
                                               particle-key-blue
                                               particle-key-alpha
                                               particle-key-rotation
                                               particle-key-stretch-factor-x
                                               particle-key-stretch-factor-y
                                               particle-key-angular-velocity
                                               :as args]
                                         (pb-property-msgs
                                           Particle$ParticleKey
                                           Particle$Emitter$ParticleProperty
                                           args))))

(g/defnk produce-emitter-pb
  [;; Properties
   animation
   blend-mode
   duration
   id
   inherit-velocity
   material
   max-particle-count
   mode
   particle-orientation
   pivot
   position
   rotation
   size-mode
   space
   start-delay
   start-offset
   stretch-with-velocity
   tile-source
   type

   ;; Non-properties
   emitter-property-msgs
   material-attribute-infos
   modifier-msgs
   particle-property-msgs
   vertex-attribute-bytes
   vertex-attribute-overrides]

  (let [[duration duration-spread] duration
        [start-delay start-delay-spread] start-delay
        attributes-save-values (graphics/vertex-attribute-overrides->save-values vertex-attribute-overrides material-attribute-infos)
        attributes-build-target (graphics/vertex-attribute-overrides->build-target vertex-attribute-overrides vertex-attribute-bytes material-attribute-infos)

        emitter
        (protobuf/make-map-without-defaults Particle$Emitter
          :id id
          :mode mode
          :duration duration
          :duration-spread duration-spread
          :space space
          :position position
          :rotation rotation
          :tile-source (resource/resource->proj-path tile-source)
          :animation animation
          :material (resource/resource->proj-path material)
          :blend-mode blend-mode
          :particle-orientation particle-orientation
          :inherit-velocity inherit-velocity
          :max-particle-count max-particle-count
          :type type
          :start-delay start-delay
          :start-delay-spread start-delay-spread
          :properties emitter-property-msgs
          :particle-properties particle-property-msgs
          :modifiers modifier-msgs
          :size-mode size-mode
          :stretch-with-velocity stretch-with-velocity
          :start-offset start-offset
          :pivot pivot)]

    (assoc emitter
      :attributes-save-values attributes-save-values
      :attributes-build-target attributes-build-target)))

(defn- resolve-modifier-node-outline-key [evaluation-context modifier-id parent-id]
  (let [type-label (:label (mod-types (g/node-value modifier-id :type evaluation-context)))
        taken-keys (outline/taken-node-outline-keys parent-id evaluation-context)]
    (g/update-property modifier-id :node-outline-key (fnil outline/next-node-outline-key type-label) taken-keys)))

(defn- attach-modifier
  ([parent-id modifier-id]
   (attach-modifier parent-id modifier-id true))
  ([parent-id modifier-id resolve-node-outline-key?]
   (concat
     (when resolve-node-outline-key?
       ;; resolve the node outline key before connecting the modifiers so taken
       ;; node outline keys don't include the modifier key
       (g/expand-ec resolve-modifier-node-outline-key modifier-id parent-id))
     (let [conns [[:_node-id :nodes]
                  [:node-outline :child-outlines]
                  [:scene :child-scenes]
                  [:pb-msg :modifier-msgs]]]
       (for [[from to] conns]
         (g/connect modifier-id from parent-id to))))))

(defn- prop-resource-error [nil-severity _node-id prop-kw prop-value prop-name]
  (or (validation/prop-error nil-severity _node-id prop-kw validation/prop-nil? prop-value prop-name)
      (validation/prop-error :fatal _node-id prop-kw validation/prop-resource-not-exists? prop-value prop-name)))

(defn- validate-material [_node-id material material-max-page-count material-shader texture-page-count]
  (let [is-paged-material (boolean (some-> material-shader shader/is-using-array-samplers?))]
    (or (prop-resource-error :fatal _node-id :material material "Material")
        (validation/prop-error :fatal _node-id :material shader/page-count-mismatch-error-message is-paged-material texture-page-count material-max-page-count "Image"))))

(g/defnk produce-properties [_node-id _declared-properties material-attribute-infos vertex-attribute-overrides]
  (let [attribute-properties
        (when-not (g/error-value? material-attribute-infos)
          (graphics/attribute-property-entries _node-id material-attribute-infos 0 vertex-attribute-overrides))]
    (-> _declared-properties
        (update :properties into attribute-properties)
        (update :display-order into (map first) attribute-properties))))

(declare ParticleFXNode)

(def ^:private pb-default-duration (protobuf/default Particle$Emitter :duration))
(def ^:private pb-default-duration-spread (protobuf/default Particle$Emitter :duration-spread))
(def ^:private default-duration (pair pb-default-duration pb-default-duration-spread))

(def ^:private pb-default-start-delay (protobuf/default Particle$Emitter :start-delay))
(def ^:private pb-default-start-delay-spread (protobuf/default Particle$Emitter :start-delay-spread))
(def ^:private default-start-delay (pair pb-default-start-delay pb-default-start-delay-spread))

(g/defnode EmitterNode
  (inherits core/Scope)
  (inherits scene/SceneNode)
  (inherits outline/OutlineNode)
  (inherits EmitterProperties)
  (inherits ParticleProperties)

  (property id g/Str (default (protobuf/default Particle$Emitter :id)))
  (property pivot types/Vec3 (default scene/default-position)
            (dynamic label (properties/label-dynamic :particlefx :pivot))
            (dynamic tooltip (properties/tooltip-dynamic :particlefx :pivot)))
  (property mode g/Keyword ; Required protobuf field.
            (dynamic edit-type (g/constantly (properties/->pb-choicebox Particle$PlayMode)))
            (dynamic label (properties/label-dynamic :particlefx :mode))
            (dynamic tooltip (properties/tooltip-dynamic :particlefx :mode)))
  (property duration types/Vec2 (default default-duration)
            (dynamic edit-type (g/constantly {:type types/Vec2 :labels ["" "+/-"]}))
            (dynamic label (properties/label-dynamic :particlefx :duration))
            (dynamic tooltip (properties/tooltip-dynamic :particlefx :duration)))
  (property space g/Keyword ; Required protobuf field.
            (dynamic edit-type (g/constantly (properties/->pb-choicebox Particle$EmissionSpace)))
            (dynamic label (properties/label-dynamic :particlefx :space))
            (dynamic tooltip (properties/tooltip-dynamic :particlefx :space)))

  (property tile-source resource/Resource ; Required protobuf field.
            (dynamic label (properties/label-dynamic :particlefx :tile-source))
            (dynamic tooltip (properties/tooltip-dynamic :particlefx :tile-source))
            (value (gu/passthrough tile-source-resource))
            (set (fn [evaluation-context self old-value new-value]
                   (project/resource-setter evaluation-context self old-value new-value
                                            [:resource :tile-source-resource]
                                            [:build-targets :dep-build-targets]
                                            [:texture-set :texture-set]
                                            [:texture-page-count :texture-page-count]
                                            [:gpu-texture :gpu-texture]
                                            [:anim-ids :anim-ids])))
            (dynamic edit-type (g/constantly
                                 {:type resource/Resource
                                  :ext ["atlas" "tilesource"]}))
            (dynamic error (g/fnk [_node-id tile-source]
                                  (prop-resource-error :fatal _node-id :tile-source tile-source "Image"))))

  (property animation g/Str ; Required protobuf field.
            (dynamic label (properties/label-dynamic :particlefx :animation))
            (dynamic tooltip (properties/tooltip-dynamic :particlefx :animation))
            (dynamic error (g/fnk [_node-id animation tile-source anim-ids]
                             (when tile-source
                               (or (validation/prop-error :fatal _node-id :animation validation/prop-empty? animation "Animation")
                                   (validation/prop-error :fatal _node-id :animation validation/prop-anim-missing? animation anim-ids)))))
            (dynamic edit-type (g/fnk [anim-ids]
                                      (let [vals (seq anim-ids)]
                                        (properties/->choicebox vals)))))

  (property material resource/Resource ; Required protobuf field.
            (dynamic label (properties/label-dynamic :particlefx :material))
            (dynamic tooltip (properties/tooltip-dynamic :particlefx :material))
            (value (gu/passthrough material-resource))
            (set (fn [evaluation-context self old-value new-value]
                   (project/resource-setter evaluation-context self old-value new-value
                                            [:resource :material-resource]
                                            [:max-page-count :material-max-page-count]
                                            [:attribute-infos :material-attribute-infos]
                                            [:shader :material-shader]
                                            [:samplers :material-samplers]
                                            [:build-targets :dep-build-targets])))
            (dynamic edit-type (g/constantly
                                 {:type resource/Resource
                                  :ext ["material"]}))
            (dynamic error (g/fnk [_node-id material material-max-page-count material-shader texture-page-count]
                             (prop-resource-error :fatal _node-id :material material "Material")
                             (validate-material _node-id material material-max-page-count material-shader texture-page-count))))

  (property blend-mode g/Keyword (default (protobuf/default Particle$Emitter :blend-mode))
            (dynamic tip (validation/blend-mode-tip blend-mode Particle$BlendMode))
            (dynamic edit-type (g/constantly (properties/->pb-choicebox Particle$BlendMode))))

  (property particle-orientation g/Keyword (default (protobuf/default Particle$Emitter :particle-orientation))
            (dynamic edit-type (g/constantly (properties/->pb-choicebox Particle$ParticleOrientation)))
            (dynamic label (properties/label-dynamic :particlefx :particle-orientation))
            (dynamic tooltip (properties/tooltip-dynamic :particlefx :particle-orientation)))
  (property inherit-velocity g/Num (default (protobuf/default Particle$Emitter :inherit-velocity))
            (dynamic label (properties/label-dynamic :particlefx :inherit-velocity))
            (dynamic tooltip (properties/tooltip-dynamic :particlefx :inherit-velocity)))
  (property max-particle-count g/Int ; Required protobuf field.
            (dynamic label (properties/label-dynamic :particlefx :max-particle-count))
            (dynamic tooltip (properties/tooltip-dynamic :particlefx :max-particle-count)))
  (property type g/Keyword ; Required protobuf field.
            (dynamic edit-type (g/constantly (properties/->pb-choicebox Particle$EmitterType)))
            (dynamic label (properties/label-dynamic :particlefx :type))
            (dynamic tooltip (properties/tooltip-dynamic :particlefx :type)))
  (property start-delay types/Vec2 (default default-start-delay)
            (dynamic edit-type (g/constantly {:type types/Vec2 :labels ["" "+/-"]}))
            (dynamic label (properties/label-dynamic :particlefx :start-delay))
            (dynamic tooltip (properties/tooltip-dynamic :particlefx :start-delay)))
  (property size-mode g/Keyword (default (protobuf/default Particle$Emitter :size-mode))
            (dynamic edit-type (g/constantly (properties/->pb-choicebox Particle$SizeMode)))
            (dynamic label (properties/label-dynamic :particlefx :size-mode))
            (dynamic tooltip (properties/tooltip-dynamic :particlefx :size-mode)))
  (property stretch-with-velocity g/Bool (default (protobuf/default Particle$Emitter :stretch-with-velocity))
            (dynamic label (properties/label-dynamic :particlefx :stretch-with-velocity))
            (dynamic tooltip (properties/tooltip-dynamic :particlefx :stretch-with-velocity)))
  (property start-offset g/Num (default (protobuf/default Particle$Emitter :start-offset))
            (dynamic label (properties/label-dynamic :particlefx :start-offset))
            (dynamic tooltip (properties/tooltip-dynamic :particlefx :start-offset)))
  (property vertex-attribute-overrides g/Any (default {})
            (dynamic visible (g/constantly false)))

  (display-order [:id scene/SceneNode :pivot :mode :size-mode :space :duration :start-delay :start-offset :tile-source :animation :material :blend-mode
                  :max-particle-count :type :particle-orientation :inherit-velocity [(localization/message "property.particlefx.category.particle-life") ParticleProperties]])

  (input tile-source-resource resource/Resource)
  (input material-resource resource/Resource)
  (input material-shader ShaderLifecycle)
  (input material-samplers g/Any)
  (input material-max-page-count g/Int)
  (input material-attribute-infos g/Any)
  (input default-tex-params g/Any)
  (input texture-set g/Any)
  (input gpu-texture g/Any)
  (input texture-page-count g/Int :substitute nil)
  (input dep-build-targets g/Any :array)
  (input emitter-indices g/Any)
  (output emitter-index g/Any (g/fnk [_node-id emitter-indices] (emitter-indices _node-id)))
  (output build-targets g/Any (g/fnk [_node-id tile-source material material-max-page-count material-shader texture-page-count animation anim-ids dep-build-targets]
                                (or (when-let [errors (->> [(validation/prop-error :fatal _node-id :tile-source validation/prop-nil? tile-source "Image")
                                                            (validate-material _node-id material material-max-page-count material-shader texture-page-count)
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
                                                      :children (localization/annotate-as-sorted localization/natural-sort-by-label child-outlines)
                                                      :child-reqs [{:node-type ModifierNode
                                                                    :tx-attach-fn attach-modifier}]}))
  (output aabb AABB produce-emitter-aabb)
  (output visibility-aabb AABB produce-emitter-visibility-aabb)
  (output emitter-sim-data g/Any :cached
          (g/fnk [animation texture-set gpu-texture material-shader]
            (when (and animation texture-set gpu-texture)
              (let [texture-set-anim (first (filter #(= animation (:id %)) (:animations texture-set)))
                    ^ByteString tex-coords (:tex-coords texture-set)
                    tex-coords-buffer (ByteBuffer/allocateDirect (.size tex-coords))
                    page-indices (:page-indices texture-set)
                    page-indices-buffer (ByteBuffer/allocateDirect (* Integer/BYTES (count page-indices)))
                    frame-indices (:frame-indices texture-set)
                    frame-indices-buffer (ByteBuffer/allocateDirect (* Integer/BYTES (count frame-indices)))]
                (.put tex-coords-buffer (.asReadOnlyByteBuffer tex-coords))
                (.flip tex-coords-buffer)
                (.order page-indices-buffer ByteOrder/LITTLE_ENDIAN)
                (.put (.asIntBuffer page-indices-buffer) (int-array page-indices))
                (.flip page-indices-buffer)
                (.order frame-indices-buffer ByteOrder/LITTLE_ENDIAN)
                (.put (.asIntBuffer frame-indices-buffer) (int-array frame-indices))
                (.flip frame-indices-buffer)
                {:gpu-texture gpu-texture
                 :shader material-shader
                 :texture-set-anim texture-set-anim
                 :tex-coords tex-coords-buffer
                 :page-indices page-indices-buffer
                 :frame-indices frame-indices-buffer}))))
  (output _properties g/Properties :cached produce-properties)
  (output vertex-attribute-bytes g/Any :cached (g/fnk [_node-id material-attribute-infos vertex-attribute-overrides]
                                                 (graphics/attribute-bytes-by-attribute-key _node-id material-attribute-infos 0 vertex-attribute-overrides))))

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

(defn- resolve-emitter-id [evaluation-context self-id emitter-id]
  (g/update-property emitter-id :id id/resolve (g/node-value self-id :ids evaluation-context)))

(defn- attach-emitter
  ([self-id emitter-id]
   (attach-emitter self-id emitter-id false))
  ([self-id emitter-id resolve-id?]
   (concat
     (when resolve-id?
       ;; resolve id before connecting the emitter so :ids don't include the emitter id
       (g/expand-ec resolve-emitter-id self-id emitter-id))
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
       (g/connect self-id from emitter-id to)))))

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
  (output save-value g/Any :cached (g/fnk [pb-data] (select-attribute-values pb-data :attributes-save-values)))
  (output pb-data g/Any (g/fnk [emitter-msgs modifier-msgs]
                          (protobuf/make-map-without-defaults Particle$ParticleFX
                            :emitters emitter-msgs
                            :modifiers modifier-msgs)))
  (output rt-pb-data g/Any :cached (g/fnk [pb-data]
                                     (particle-fx-transform (select-attribute-values pb-data :attributes-build-target))))
  (output emitter-sim-data g/Any :cached (gu/passthrough emitter-sim-data))
  (output emitter-indices g/Any :cached
          (g/fnk [^:unsafe _evaluation-context nodes]
            ;; We use unsafe evaluation context to get child node types: these
            ;; should never change
            (let [basis (:basis _evaluation-context)]
              (into {}
                    (comp
                      (filter #(g/node-instance? basis EmitterNode %))
                      (map-indexed (fn [index emitter-node]
                                     [emitter-node index])))
                    nodes))))

  (output build-targets g/Any :cached produce-build-targets)
  (output scene g/Any :cached produce-scene)
  (output scene-updatable g/Any :cached produce-scene-updatable)
  (output node-outline outline/OutlineData :cached
          (g/fnk [^:unsafe _evaluation-context _node-id child-outlines]
            ;; We use unsafe evaluation context to get child node types: these
            ;; should never change
            (let [basis (:basis _evaluation-context)
                  outlines (group-by #(g/node-instance? basis ModifierNode (:node-id %)) child-outlines)
                  mod-outlines (get outlines true)
                  emitter-outlines (get outlines false)]
              {:node-id _node-id
               :node-outline-key "ParticleFX"
               :label (localization/message "outline.particlefx")
               :icon particle-fx-icon
               :children (localization/annotate-as-sorted
                           (fn [localization-state _]
                             (into (localization/natural-sort-by-label localization-state emitter-outlines)
                                   (localization/natural-sort-by-label localization-state mod-outlines)))
                           (into emitter-outlines mod-outlines))
               :child-reqs [{:node-type EmitterNode
                             :tx-attach-fn (fn [self-id child-id]
                                             (attach-emitter self-id child-id true))}
                            {:node-type ModifierNode
                             :tx-attach-fn attach-modifier}]})))
  (output fetch-anim-fn Runnable :cached (g/fnk [emitter-sim-data] (fn [index] (get emitter-sim-data index)))))

(defn- make-modifier
  [parent-id modifier node-outline-key]
  (let [graph-id (g/node-id->graph-id parent-id)]
    (g/make-nodes graph-id [mod-node [ModifierNode :node-outline-key node-outline-key]]
      (gu/set-properties-from-pb-map mod-node Particle$Modifier modifier
        position :position
        rotation :rotation
        type :type
        use-direction (protobuf/int->boolean :use-direction))
      (into []
            (mapcat (fn [property]
                      (case (:key property)
                        :modifier-key-magnitude (g/set-property mod-node :magnitude (pb-property->curve-spread property))
                        :modifier-key-max-distance (g/set-property mod-node :max-distance (pb-property->curve property))
                        nil)))
            (:properties modifier))
      (attach-modifier parent-id mod-node false))))

(defn- add-modifier-handler [parent-id type select-fn]
  (when-some [modifier (get-in mod-types [type :template])]
    (let [node-outline-key (outline/next-node-outline-key (modifier-type-label type)
                                                          (outline/taken-node-outline-keys parent-id))
          op-seq (gensym)
          mod-node (first (g/tx-nodes-added
                            (g/transact
                              (concat
                                (g/operation-label (localization/message "operation.particlefx.add-modifier"))
                                (g/operation-sequence op-seq)
                                (make-modifier parent-id modifier node-outline-key)))))]
      (when (some? select-fn)
        (g/transact
          (concat
            (g/operation-sequence op-seq)
            (select-fn [mod-node])))))))

(defn- selection->emitter [selection]
  (handler/adapt-single selection EmitterNode))

(defn- selection->particlefx [selection]
  (handler/adapt-single selection ParticleFXNode))

(handler/defhandler :edit.add-secondary-embedded-component :workbench
  (active? [selection] (or (selection->emitter selection)
                           (selection->particlefx selection)))
  (label [user-data] (if-not user-data
                       (localization/message "command.edit.add-secondary-embedded-component.variant.particlefx")
                       (->> user-data :modifier-type mod-types :message)))
  (run [selection user-data app-view]
    (let [parent-id (or (selection->particlefx selection)
                        (selection->emitter selection))]
      (add-modifier-handler parent-id (:modifier-type user-data) (fn [node-ids] (app-view/select app-view node-ids)))))
  (options [selection user-data]
    (when (not user-data)
      (mapv (fn [[type data]]
              {:label (:message data)
               :icon modifier-icon
               :command :edit.add-secondary-embedded-component
               :user-data {:modifier-type type}})
            mod-types))))

(defn- make-emitter
  ([self emitter]
   (make-emitter self emitter nil false))
  ([self emitter select-fn resolve-id?]
   (let [project (project/get-project self)
         workspace (project/workspace project)
         graph-id (g/node-id->graph-id self)
         resolve-resource #(workspace/resolve-workspace-resource workspace %)]
     (g/make-nodes graph-id [emitter-node EmitterNode]
       (gu/set-properties-from-pb-map emitter-node Particle$Emitter emitter
         position :position
         rotation :rotation
         id :id
         mode :mode
         space :space
         tile-source (resolve-resource :tile-source)
         animation :animation
         material (resolve-resource :material)
         vertex-attribute-overrides (graphics/override-attributes->vertex-attribute-overrides :attributes)
         blend-mode :blend-mode
         particle-orientation :particle-orientation
         inherit-velocity :inherit-velocity
         max-particle-count :max-particle-count
         type :type
         size-mode :size-mode
         stretch-with-velocity :stretch-with-velocity
         start-offset :start-offset
         pivot :pivot)
       (let [{:keys [duration duration-spread]} emitter]
         (when (or duration duration-spread)
           (g/set-property emitter-node :duration
             (pair (or duration pb-default-duration)
                   (or duration-spread pb-default-duration-spread)))))
       (let [{:keys [start-delay start-delay-spread]} emitter]
         (when (or start-delay start-delay-spread)
           (g/set-property emitter-node :start-delay
             (pair (or start-delay pb-default-start-delay)
                   (or start-delay-spread pb-default-start-delay-spread)))))
       (coll/mapcat #(g/set-property emitter-node (:key %) (pb-property->curve-spread %))
                    (:properties emitter))
       (coll/mapcat #(g/set-property emitter-node (:key %) (pb-property->curve %))
                    (:particle-properties emitter))
       (attach-emitter self emitter-node resolve-id?)
       (map (partial make-modifier emitter-node)
            (:modifiers emitter)
            (outline/gen-node-outline-keys
              (map (comp modifier-type-label :type)
                   (:modifiers emitter))))
       (when select-fn
         (select-fn [emitter-node]))))))

(defn- add-emitter-handler [self type select-fn]
  (when-let [resource (io/resource emitter-template)]
    (let [emitter (protobuf/read-map-without-defaults Particle$Emitter resource)]
      (g/transact
        (concat
          (g/operation-label (localization/message "operation.particlefx.add-emitter"))
          (make-emitter self (assoc emitter :type type) select-fn true))))))

(handler/defhandler :edit.add-embedded-component :workbench
  (active? [selection] (selection->particlefx selection))
  (label [user-data]
    (if-not user-data
      (localization/message "command.edit.add-embedded-component.variant.particlefx")
      (->> user-data :emitter-type (get emitter-types) :label)))
  (run [selection user-data app-view] (let [pfx (selection->particlefx selection)]
                                        (add-emitter-handler pfx (:emitter-type user-data) (fn [node-ids] (app-view/select app-view node-ids)))))
  (options [selection user-data]
           (when (not user-data)
             (mapv (fn [[type data]]
                     {:label (:label data)
                      :icon emitter-icon
                      :command :edit.add-embedded-component
                      :user-data {:emitter-type type}})
                   emitter-types))))


;;--------------------------------------------------------------------
;; Manipulators

(defn- update-curve-spread-start-value
  [curve-spread f]
  (let [[first-point & rest] (properties/curve-vals curve-spread)
        first-point' (update first-point 1 f)]
    (properties/->curve-spread (into [first-point'] rest) (:spread curve-spread))))

(defmethod scene-tools/manip-scalable? ::ModifierNode [_node-id] true)

(defmethod scene-tools/manip-scale-manips ::ModifierNode [node-id]
  [:scale-x])

(defmethod scene-tools/manip-scale ::ModifierNode
  [evaluation-context node-id ^Vector3d delta]
  (let [old-magnitude (g/node-value node-id :magnitude evaluation-context)
        new-magnitude (update-curve-spread-start-value old-magnitude #(properties/scale-and-round % (.getX delta)))]
    (g/set-property node-id :magnitude new-magnitude)))

(defmethod scene-tools/manip-scalable? ::EmitterNode [_node-id] true)

(defmethod scene-tools/manip-scale ::EmitterNode
  [evaluation-context node-id ^Vector3d delta]
  (let [old-x (g/node-value node-id :emitter-key-size-x evaluation-context)
        old-y (g/node-value node-id :emitter-key-size-y evaluation-context)
        old-z (g/node-value node-id :emitter-key-size-z evaluation-context)
        new-x (update-curve-spread-start-value old-x #(properties/scale-by-absolute-value-and-round % (.getX delta)))
        new-y (update-curve-spread-start-value old-y #(properties/scale-by-absolute-value-and-round % (.getY delta)))
        new-z (update-curve-spread-start-value old-z #(properties/scale-by-absolute-value-and-round % (.getZ delta)))]
    (g/set-properties node-id
      :emitter-key-size-x new-x
      :emitter-key-size-y new-y
      :emitter-key-size-z new-z)))

(defn load-particle-fx [project self _resource pb]
  (concat
    (g/connect project :settings self :project-settings)
    (g/connect project :default-tex-params self :default-tex-params)
    (map (partial make-emitter self)
         (:emitters pb))
    (map (partial make-modifier self)
         (:modifiers pb)
         (outline/gen-node-outline-keys (map (comp modifier-type-label :type)
                                             (:modifiers pb))))))

(defn- sanitize-emitter [emitter]
  ;; Particle$Emitter in map format.
  (-> emitter
      (protobuf/assign-repeated :properties (filterv significant-pb-property? (:properties emitter)))
      (protobuf/assign-repeated :particle-properties (filterv significant-pb-property? (:particle-properties emitter)))
      (protobuf/sanitize-repeated :attributes graphics/sanitize-attribute-override)))

(defn- sanitize-modifier [modifier]
  ;; Particle$Modifier in map format.
  ;; Add default properties. We do this instead of stripping them out because
  ;; they have individual defaults. This means we always have to include all the
  ;; modifier properties in the file. Luckily, there are far fewer of these than
  ;; the emitter and particle properties.
  ;;
  ;; TODO(save-value-cleanup): Maybe we should just sanitize these like the rest
  ;; of the properties? If I add a modifier, it will have the default properties
  ;; and this only covers the case where there were no properties at all. Seems
  ;; like a hack to make a short-lived file format without properties load in
  ;; the editor?
  (update modifier :properties #(or (not-empty %) (get-in mod-types [(:type modifier) :template :properties]))))

(defn- sanitize-particle-fx [particle-fx]
  ;; Particle$ParticleFX in map format.
  (-> particle-fx
      (protobuf/sanitize-repeated :emitters sanitize-emitter)
      (protobuf/sanitize-repeated :modifiers sanitize-modifier)))

(defn register-resource-types [workspace]
  (concat
    (attachment/register
      workspace EmitterNode :modifiers
      :add {ModifierNode attach-modifier}
      :get attachment/nodes-getter)
    (attachment/register
      workspace ParticleFXNode :emitters
      :add {EmitterNode attach-emitter}
      :get (attachment/nodes-by-type-getter EmitterNode))
    (attachment/register
      workspace ParticleFXNode :modifiers
      :add {ModifierNode attach-modifier}
      :get (attachment/nodes-by-type-getter ModifierNode))
    (resource-node/register-ddf-resource-type workspace
      :ext particlefx-ext
      :label "Particle FX"
      :node-type ParticleFXNode
      :ddf-type Particle$ParticleFX
      :load-fn load-particle-fx
      :sanitize-fn sanitize-particle-fx
      :icon particle-fx-icon
      :icon-class :design
      :tags #{:component :non-embeddable}
      :tag-opts {:component {:transform-properties #{:position :rotation}}}
      :view-types [:scene :text]
      :view-opts {:scene {:grid true}})))

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
