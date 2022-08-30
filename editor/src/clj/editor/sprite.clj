;; Copyright 2020-2022 The Defold Foundation
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

(ns editor.sprite
  (:require [clojure.string :as str]
            [editor.protobuf :as protobuf]
            [dynamo.graph :as g]
            [editor.colors :as colors]
            [editor.graph-util :as gu]
            [editor.geom :as geom]
            [editor.gl :as gl]
            [editor.gl.shader :as shader]
            [editor.gl.texture :as texture]
            [editor.gl.vertex :as vtx]
            [editor.defold-project :as project]
            [editor.material :as material]
            [editor.properties :as properties]
            [editor.scene :as scene]
            [editor.scene-picking :as scene-picking]
            [editor.workspace :as workspace]
            [editor.validation :as validation]
            [editor.pipeline :as pipeline]
            [editor.resource :as resource]
            [editor.resource-node :as resource-node]
            [editor.texture-set :as texture-set]
            [editor.gl.pass :as pass]
            [editor.types :as types]
            [editor.slice9 :as slice9])
  (:import [com.dynamo.graphics.proto Graphics$Cubemap Graphics$TextureImage Graphics$TextureImage$Image Graphics$TextureImage$Type]
           [com.dynamo.gamesys.proto Sprite$SpriteDesc Sprite$SpriteDesc$BlendMode Sprite$SpriteDesc$SizeMode]
           [com.jogamp.opengl.util.awt TextRenderer]
           [editor.types Region Animation Camera Image TexturePacking Rect EngineFormatTexture AABB TextureSetAnimationFrame TextureSetAnimation TextureSet]
           [editor.gl.shader ShaderLifecycle]
           [java.awt.image BufferedImage]
           [java.io PushbackReader]
           [com.jogamp.opengl GL GL2 GLContext GLDrawableFactory]
           [com.jogamp.opengl.glu GLU]
           [javax.vecmath Matrix4d Point3d Vector4d]))

(set! *warn-on-reflection* true)

(def sprite-icon "icons/32/Icons_14-Sprite.png")

(defn- v3->v4 [v]
  (conj v 0.0))

(defn- v4->v3 [v4]
  (subvec v4 0 3))

; Render assets
(vtx/defvertex texture-vtx
  (vec3 position)
  (vec2 texcoord0 true))

(shader/defshader vertex-shader
  (attribute vec4 position)
  (attribute vec2 texcoord0)
  (varying vec2 var_texcoord0)
  (defn void main []
    (setq gl_Position (* gl_ModelViewProjectionMatrix (vec4 position.xyz 1.0)))
    (setq var_texcoord0 texcoord0)))

(shader/defshader fragment-shader
  (varying vec2 var_texcoord0)
  (uniform sampler2D texture_sampler)
  (defn void main []
    (setq gl_FragColor (texture2D texture_sampler var_texcoord0.xy))))

; TODO - macro of this
(def shader (shader/make-shader ::shader vertex-shader fragment-shader))

(vtx/defvertex color-vtx
  (vec3 position)
  (vec4 color))

(shader/defshader outline-vertex-shader
  (attribute vec4 position)
  (attribute vec4 color)
  (varying vec4 var_color)
  (defn void main []
    (setq gl_Position (* gl_ModelViewProjectionMatrix position))
    (setq var_color color)))

(shader/defshader outline-fragment-shader
  (varying vec4 var_color)
  (defn void main []
    (setq gl_FragColor var_color)))

; TODO - macro of this
(def outline-shader (shader/make-shader ::outline-shader outline-vertex-shader outline-fragment-shader))

(defn- conj-animation-data!
  [vbuf animation frame-index world-transform size-mode size slice9]
  (let [animation-frame (get-in animation [:frames frame-index])]
    (reduce conj! vbuf (if (= :size-mode-auto size-mode)
                         (texture-set/vertex-data animation-frame world-transform)
                         (let [slice9-data (slice9/vertex-data animation-frame size slice9)
                               positions (:position-data slice9-data)
                               uvs (:uv-data slice9-data)]
                           (into [] (map into positions uvs)))))))

(defn- gen-vertex-buffer
  [renderables count]
  (persistent! (reduce (fn [vbuf {:keys [world-transform updatable user-data]}]
                         (let [{:keys [animation size-mode size slice9]} user-data
                               frame (get-in updatable [:state :frame] 0)]
                           (conj-animation-data! vbuf animation frame world-transform size-mode size slice9)))
                       (->texture-vtx (* count 6))
                       renderables)))

(defn- gen-outline-vertex [^Matrix4d wt ^Point3d pt x y cr cg cb]
  (.set pt x y 0)
  (.transform wt pt)
  [(.x pt) (.y pt) (.z pt) cr cg cb 1])

(defn- conj-outline-quad! [vbuf ^Matrix4d wt ^Point3d pt width height cr cg cb]
  (let [x1 (* 0.5 width)
        y1 (* 0.5 height)
        x0 (- x1)
        y0 (- y1)
        v0 (gen-outline-vertex wt pt x0 y0 cr cg cb)
        v1 (gen-outline-vertex wt pt x1 y0 cr cg cb)
        v2 (gen-outline-vertex wt pt x1 y1 cr cg cb)
        v3 (gen-outline-vertex wt pt x0 y1 cr cg cb)]
    (-> vbuf (conj! v0) (conj! v1) (conj! v1) (conj! v2) (conj! v2) (conj! v3) (conj! v3) (conj! v0))))

(defn- conj-outline-slice9-quad! [vbuf line-data ^Matrix4d world-transform tmp-point cr cg cb]
  (let [line-data-points (map (fn [item] (gen-outline-vertex world-transform tmp-point (get item 0) (get item 1) cr cg cb))  line-data)]
    (reduce conj! vbuf line-data-points)))

(defn- gen-outline-vertex-buffer
  [renderables count]
  (let [tmp-point (Point3d.)]
    (loop [renderables renderables
           vbuf (->color-vtx (* count 8))]
      (if-let [renderable (first renderables)]
        (let [color (if (:selected renderable) colors/selected-outline-color colors/outline-color)
              cr (get color 0)
              cg (get color 1)
              cb (get color 2)
              world-transform (:world-transform renderable)
              user-data (:user-data renderable)
              size (:size user-data)
              size-mode (:size-mode user-data)
              slice9 (:slice9 user-data)
              anim-width (if (= :size-mode-auto size-mode)
                           (-> user-data :animation :width)
                           (get size 0))
              anim-height (if (= :size-mode-auto size-mode)
                            (-> user-data :animation :height)
                            (get size 1))
              animation (:animation user-data)
              animation-frame (get-in animation [:frames 0])] ; frame index??
          (recur (rest renderables) (if (= :size-mode-auto size-mode)
                                      (conj-outline-quad! vbuf world-transform tmp-point anim-width anim-height cr cg cb)
                                      (let [slice9-data (slice9/vertex-data animation-frame size slice9)
                                            line-data (:line-data slice9-data)]
                                        (conj-outline-slice9-quad! vbuf line-data world-transform tmp-point cr cg cb)))))
        (persistent! vbuf)))))

; Rendering

(shader/defshader sprite-id-vertex-shader
  (uniform mat4 view_proj)
  (attribute vec4 position)
  (attribute vec2 texcoord0)
  (varying vec2 var_texcoord0)
  (defn void main []
    (setq gl_Position (* view_proj (vec4 position.xyz 1.0)))
    (setq var_texcoord0 texcoord0)))

(shader/defshader sprite-id-fragment-shader
  (varying vec2 var_texcoord0)
  (uniform sampler2D DIFFUSE_TEXTURE)
  (uniform vec4 id)
  (defn void main []
    (setq vec4 color (texture2D DIFFUSE_TEXTURE var_texcoord0))
    (if (> color.a 0.05)
      (setq gl_FragColor id)
      (discard))))

(def id-shader (shader/make-shader ::sprite-id-shader sprite-id-vertex-shader sprite-id-fragment-shader {"view_proj" :view-proj "id" :id}))

(defn- quad-count [size-mode slice9]
  (let [x0 (get slice9 0)
        x1 (get slice9 2)
        y0 (get slice9 1)
        y1 (get slice9 3)
        columns (cond-> 1 (pos? x0) inc (pos? x1) inc)
        rows (cond-> 1 (pos? y0) inc (pos? y1) inc)]
    (if (= :size-mode-auto size-mode)
      1
      (* columns rows))))

(defn- count-quads [renderables]
  (reduce + (map :quad-count (map :user-data renderables))))

(defn render-sprites [^GL2 gl render-args renderables count]
  (let [user-data (:user-data (first renderables))
        gpu-texture (:gpu-texture user-data)
        pass (:pass render-args)
        num-quads (count-quads renderables)]
    (condp = pass
      pass/transparent
      (let [shader (:shader user-data)
            vertex-binding (vtx/use-with ::sprite-trans (gen-vertex-buffer renderables num-quads) shader)
            blend-mode (:blend-mode user-data)]
        (gl/with-gl-bindings gl render-args [shader vertex-binding gpu-texture]
          (gl/set-blend-mode gl blend-mode)
          (gl/gl-draw-arrays gl GL/GL_TRIANGLES 0 (* num-quads 6))
          (.glBlendFunc gl GL/GL_SRC_ALPHA GL/GL_ONE_MINUS_SRC_ALPHA)))

      pass/selection
      (let [vertex-binding (vtx/use-with ::sprite-selection (gen-vertex-buffer renderables num-quads) id-shader)]
        (gl/with-gl-bindings gl (assoc render-args :id (scene-picking/renderable-picking-id-uniform (first renderables))) [id-shader vertex-binding gpu-texture]
          (gl/gl-draw-arrays gl GL/GL_TRIANGLES 0 (* num-quads 6)))))))

(defn- render-sprite-outlines [^GL2 gl render-args renderables count]
  (assert (= pass/outline (:pass render-args)))
  (let [num-quads (count-quads renderables)
        outline-vertex-binding (vtx/use-with ::sprite-outline (gen-outline-vertex-buffer renderables num-quads) outline-shader)]
    (gl/with-gl-bindings gl render-args [outline-shader outline-vertex-binding]
      (gl/gl-draw-arrays gl GL/GL_LINES 0 (* num-quads 8)))))

; Node defs

(g/defnk produce-save-value [image default-animation material blend-mode size-mode size slice9]
  {:tile-set (resource/resource->proj-path image)
   :default-animation default-animation
   :material (resource/resource->proj-path material)
   :blend-mode blend-mode
   :size-mode size-mode
   :size (v3->v4 size)
   :slice9 slice9})

(g/defnk produce-scene
  [_node-id aabb gpu-texture material-shader animation blend-mode size-mode size slice9]
  (cond-> {:node-id _node-id
           :aabb aabb
           :renderable {:passes [pass/selection]}}
    (seq (:frames animation))
    (assoc :renderable {:render-fn render-sprites
                        :batch-key [gpu-texture blend-mode material-shader]
                        :select-batch-key _node-id
                        :tags #{:sprite}
                        :user-data {:gpu-texture gpu-texture
                                    :shader material-shader
                                    :animation animation
                                    :blend-mode blend-mode
                                    :size-mode size-mode
                                    :size size
                                    :slice9 slice9
                                    :quad-count (quad-count size-mode slice9)}
                        :passes [pass/transparent pass/selection]})

    (and (:width animation) (:height animation))
    (assoc :children [{:node-id _node-id
                       :aabb aabb
                       :renderable {:render-fn render-sprite-outlines
                                    :batch-key [outline-shader]
                                    :tags #{:sprite :outline}
                                    :select-batch-key _node-id
                                    :user-data {:animation animation
                                                :size-mode size-mode
                                                :size size
                                                :slice9 slice9
                                                :quad-count (quad-count size-mode slice9)}
                                    :passes [pass/outline]}}])

    (< 1 (count (:frames animation)))
    (assoc :updatable (texture-set/make-animation-updatable _node-id "Sprite" animation))))

(g/defnk produce-build-targets [_node-id resource image anim-ids default-animation material blend-mode size-mode size slice9 dep-build-targets]
  (or (when-let [errors (->> [(validation/prop-error :fatal _node-id :image validation/prop-nil? image "Image")
                              (validation/prop-error :fatal _node-id :material validation/prop-nil? material "Material")
                              (validation/prop-error :fatal _node-id :default-animation validation/prop-nil? default-animation "Default Animation")
                              (validation/prop-error :fatal _node-id :default-animation validation/prop-anim-missing? default-animation anim-ids)]
                             (remove nil?)
                             (seq))]
        (g/error-aggregate errors))
      [(pipeline/make-protobuf-build-target resource dep-build-targets
                                            Sprite$SpriteDesc
                                            {:tile-set          image
                                             :default-animation default-animation
                                             :material          material
                                             :blend-mode        blend-mode
                                             :size-mode         size-mode
                                             :size              (v3->v4 size)
                                             :slice9            slice9}
                                            [:tile-set :material])]))

(defn- sort-anim-ids
  [anim-ids]
  (sort-by str/lower-case anim-ids))

(g/defnode SpriteNode
  (inherits resource-node/ResourceNode)

  (property image resource/Resource
            (value (gu/passthrough image-resource))
            (set (fn [evaluation-context self old-value new-value]
                   (project/resource-setter evaluation-context self old-value new-value
                                            [:resource :image-resource]
                                            [:anim-data :anim-data]
                                            [:anim-ids :anim-ids]
                                            [:gpu-texture :gpu-texture]
                                            [:build-targets :dep-build-targets])))
            (dynamic error (g/fnk [_node-id image anim-data]
                                  (or (validation/prop-error :info _node-id :image validation/prop-nil? image "Image")
                                      (validation/prop-error :fatal _node-id :image validation/prop-resource-not-exists? image "Image"))))
            (dynamic edit-type (g/constantly
                                 {:type resource/Resource
                                  :ext ["atlas" "tilesource"]})))

  (property default-animation g/Str
            (dynamic error (g/fnk [_node-id image anim-ids default-animation]
                                  (when image
                                    (or (validation/prop-error :fatal _node-id :default-animation validation/prop-empty? default-animation "Default Animation")
                                        (validation/prop-error :fatal _node-id :default-animation validation/prop-anim-missing? default-animation anim-ids)))))
            (dynamic edit-type (g/fnk [anim-ids] (properties/->choicebox anim-ids))))

  (property material resource/Resource
            (value (gu/passthrough material-resource))
            (set (fn [evaluation-context self old-value new-value]
                   (project/resource-setter evaluation-context self old-value new-value
                                            [:resource :material-resource]
                                            [:shader :material-shader]
                                            [:samplers :material-samplers]
                                            [:build-targets :dep-build-targets])))
            (dynamic edit-type (g/constantly {:type resource/Resource :ext #{"material"}}))
            (dynamic error (g/fnk [_node-id material]
                                  (or (validation/prop-error :fatal _node-id :material validation/prop-nil? material "Material")
                                      (validation/prop-error :fatal _node-id :material validation/prop-resource-not-exists? material "Material")))))

  (property blend-mode g/Any (default :blend-mode-alpha)
            (dynamic tip (validation/blend-mode-tip blend-mode Sprite$SpriteDesc$BlendMode))
            (dynamic edit-type (g/constantly (properties/->pb-choicebox Sprite$SpriteDesc$BlendMode))))
  (property size-mode g/Keyword (default :size-mode-auto)
            (dynamic edit-type (g/constantly (properties/->pb-choicebox Sprite$SpriteDesc$SizeMode))))
  (property size types/Vec3 (default [0.0 0.0 0.0])
            (dynamic read-only? (g/fnk [size-mode] (= :size-mode-auto size-mode))))
  (property slice9 types/Vec4 (default [0.0 0.0 0.0 0.0]))

  (input image-resource resource/Resource)
  (input anim-data g/Any :substitute (fn [v] (assoc v :user-data "the Image has internal errors")))
  (input anim-ids g/Any)
  (input gpu-texture g/Any)
  (input dep-build-targets g/Any :array)

  (input material-resource resource/Resource)
  (input material-shader ShaderLifecycle)
  (input material-samplers g/Any)
  (input default-tex-params g/Any)

  (output tex-params g/Any (g/fnk [material-samplers material-shader default-tex-params]
                             (or (some-> material-samplers first material/sampler->tex-params)
                                 default-tex-params)))
  (output gpu-texture g/Any (g/fnk [gpu-texture tex-params] (texture/set-params gpu-texture tex-params)))
  (output animation g/Any (g/fnk [anim-data default-animation] (get anim-data default-animation))) ; TODO - use placeholder animation
  (output aabb AABB (g/fnk [animation] (if animation
                                         (let [animation-width (* 0.5 (:width animation))
                                               animation-height (* 0.5 (:height animation))]
                                           (geom/make-aabb (Point3d. (- animation-width) (- animation-height) 0)
                                                           (Point3d. animation-width animation-height 0)))
                                         geom/empty-bounding-box)))
  (output save-value g/Any produce-save-value)
  (output scene g/Any :cached produce-scene)
  (output build-targets g/Any :cached produce-build-targets))

(defn load-sprite [project self resource sprite]
  (let [image    (workspace/resolve-resource resource (:tile-set sprite))
        material (workspace/resolve-resource resource (:material sprite))]
    (concat
      (g/connect project :default-tex-params self :default-tex-params)
      (g/set-property self :image image)
      (g/set-property self :default-animation (:default-animation sprite))
      (g/set-property self :material material)
      (g/set-property self :blend-mode (:blend-mode sprite))
      (g/set-property self :size-mode (:size-mode sprite))
      (g/set-property self :size (v4->v3 (:size sprite)))
      (g/set-property self :slice9 (:slice9 sprite)))))

(defn register-resource-types [workspace]
  (resource-node/register-ddf-resource-type workspace
    :ext "sprite"
    :node-type SpriteNode
    :ddf-type Sprite$SpriteDesc
    :load-fn load-sprite
    :icon sprite-icon
    :view-types [:scene :text]
    :tags #{:component}
    :tag-opts {:component {:transform-properties #{:position :rotation}}}
    :label "Sprite"))
