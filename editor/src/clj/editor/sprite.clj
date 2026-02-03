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

(ns editor.sprite
  (:require [dynamo.graph :as g]
            [editor.colors :as colors]
            [editor.defold-project :as project]
            [editor.geom :as geom]
            [editor.gl :as gl]
            [editor.gl.pass :as pass]
            [editor.gl.shader :as shader]
            [editor.gl.texture :as texture]
            [editor.gl.vertex2 :as vtx]
            [editor.graph-util :as gu]
            [editor.graphics :as graphics]
            [editor.graphics.types :as graphics.types]
            [editor.localization :as localization]
            [editor.material :as material]
            [editor.pipeline :as pipeline]
            [editor.properties :as properties]
            [editor.protobuf :as protobuf]
            [editor.resource :as resource]
            [editor.resource-node :as resource-node]
            [editor.scene-picking :as scene-picking]
            [editor.texture-set :as texture-set]
            [editor.types :as types]
            [editor.validation :as validation]
            [editor.workspace :as workspace]
            [internal.util :as util]
            [util.coll :as coll]
            [util.fn :as fn])
  (:import [com.dynamo.bob.pipeline ShaderUtil$Common ShaderUtil$VariantTextureArrayFallback]
           [com.dynamo.gamesys.proto Sprite$SpriteDesc Sprite$SpriteDesc$BlendMode Sprite$SpriteDesc$SizeMode]
           [com.jogamp.opengl GL GL2]
           [editor.gl.shader ShaderLifecycle]
           [editor.gl.vertex2 VertexBuffer]
           [editor.types AABB]
           [java.nio ByteBuffer]
           [javax.vecmath Matrix4d Point3d]))

(set! *warn-on-reflection* true)

(def sprite-icon "icons/32/Icons_14-Sprite.png")

(defn- v3->v4 [v]
  (conj v 0.0))

(defn- v4->v3 [v4]
  (subvec v4 0 3))

; Render assets
(vtx/defvertex texture-vtx
  (vec4 position)
  (vec2 texcoord0)
  (vec1 page_index))

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

(defn- renderable-data [renderable]
  (let [{:keys [world-transform updatable user-data]} renderable
        {:keys [scene-infos size-mode size slice9 vertex-attribute-bytes]} user-data
        frame-index (get-in updatable [:state :frame] 0)

        texcoord-datas
        (if (coll/empty? scene-infos)
          [(texture-set/vertex-data nil size-mode size slice9 :pivot-center)]
          (mapv (fn [{:keys [animation]}]
                  (let [animation-frame (get-in animation [:frames frame-index])]
                    (texture-set/vertex-data animation-frame size-mode size slice9 :pivot-center)))
                scene-infos))]

    (conj {:texcoord-datas texcoord-datas
           :world-transform world-transform
           :vertex-attribute-bytes vertex-attribute-bytes}
          (select-keys (first texcoord-datas) [:position-data :line-data]))))

(defn- gen-outline-vertex [^Matrix4d wt ^Point3d pt x y cr cg cb]
  (.set pt x y 0.0)
  (.transform wt pt)
  (vector-of :float (.x pt) (.y pt) (.z pt) cr cg cb 1.0))

(defn- conj-outline-slice9-quad! [buf line-data ^Matrix4d world-transform tmp-point cr cg cb]
  (let [outline-points (map (fn [[x y]]
                              (gen-outline-vertex world-transform tmp-point x y cr cg cb))
                            line-data)]
    (doseq [outline-point outline-points]
      (vtx/buf-push-floats! buf outline-point))))

(defn- gen-outline-vertex-buffer [renderables count]
  (let [tmp-point (Point3d.)
        ^VertexBuffer vbuf (->color-vtx (* count 8))
        ^ByteBuffer buf (.buf vbuf)]
    (doseq [renderable renderables]
      (let [[cr cg cb] (colors/renderable-outline-color renderable)
            world-transform (:world-transform renderable)
            {:keys [animation size size-mode slice9]} (:user-data renderable)
            animation-frame-index (or (some-> renderable :updatable :state :frame) 0)
            animation-frame (get-in animation [:frames animation-frame-index])
            vertex-data (texture-set/vertex-data animation-frame size-mode size slice9 :pivot-center)
            line-data (:line-data vertex-data)]
        (conj-outline-slice9-quad! buf line-data world-transform tmp-point cr cg cb)))
    (vtx/flip! vbuf)))

; Rendering

(shader/defshader sprite-id-vertex-shader
  (uniform mat4 view_proj)
  (attribute vec4 position)
  (attribute vec2 texcoord0)
  (attribute float page_index)
  (varying vec2 var_texcoord0)
  (varying float var_page_index)
  (defn void main []
    (setq gl_Position (* view_proj (vec4 position.xyz 1.0)))
    (setq var_texcoord0 texcoord0)
    (setq var_page_index page_index)))

(shader/defshader sprite-id-fragment-shader
  (varying vec2 var_texcoord0)
  (varying float var_page_index)
  (uniform vec4 id)
  (uniform sampler2DArray texture_sampler)
  (defn void main []
    (setq vec4 color (texture2DArray texture_sampler (vec3 var_texcoord0 var_page_index)))
    (if (> color.a 0.05)
      (setq gl_FragColor id)
      (discard))))

(def id-shader
  (let [augmented-fragment-shader-source (.source (ShaderUtil$VariantTextureArrayFallback/transform sprite-id-fragment-shader ShaderUtil$Common/MAX_ARRAY_SAMPLERS))]
    (shader/make-shader ::sprite-id-shader sprite-id-vertex-shader augmented-fragment-shader-source {"view_proj" :view-proj "id" :id})))

(defn- quad-count [size-mode slice9]
  (let [[^double x0 ^double y0 ^double x1 ^double y1] slice9
        columns (cond-> 1 (pos? x0) inc (pos? x1) inc)
        rows (cond-> 1 (pos? y0) inc (pos? y1) inc)]
    (if (= :size-mode-auto size-mode)
      1
      (* columns rows))))

(defn- count-quads [renderables]
  (transduce (map (comp :quad-count :user-data))
             +
             0
             renderables))

(defn- count-vertices [renderable-datas]
  (transduce (map (comp count :position-data))
             +
             0
             renderable-datas))

(defn render-sprites [^GL2 gl render-args renderables _count]
  (let [user-data (:user-data (first renderables))
        scene-infos (:scene-infos user-data)
        pass (:pass render-args)
        renderable-datas (mapv renderable-data renderables)
        num-vertices (count-vertices renderable-datas)]
    (condp = pass
      pass/transparent
      (let [{:keys [blend-mode material-attribute-infos shader]} user-data
            shader-attribute-reflection-infos (shader/attribute-reflection-infos shader gl)
            combined-attribute-infos (graphics/combined-attribute-infos shader-attribute-reflection-infos material-attribute-infos :coordinate-space-world)
            vertex-description (graphics.types/make-vertex-description combined-attribute-infos)
            vbuf (graphics/put-attributes! (vtx/make-vertex-buffer vertex-description :dynamic num-vertices) renderable-datas)
            vertex-binding (vtx/use-with ::sprite-trans vbuf shader)]
        (gl/with-gl-bindings gl render-args [shader vertex-binding]
          (doseq [{:keys [gpu-texture sampler]} scene-infos]
            (gl/bind gl gpu-texture render-args)
            (shader/set-samplers-by-name shader gl sampler (:texture-units gpu-texture)))
          (gl/set-blend-mode gl blend-mode)
          (gl/gl-draw-arrays gl GL/GL_TRIANGLES 0 num-vertices)
          (.glBlendFunc gl GL/GL_SRC_ALPHA GL/GL_ONE_MINUS_SRC_ALPHA)
          (doseq [{:keys [gpu-texture]} scene-infos]
            (gl/unbind gl gpu-texture render-args))))

      pass/selection
      (let [vbuf (graphics/put-attributes! (->texture-vtx num-vertices) renderable-datas)
            vertex-binding (vtx/use-with ::sprite-selection vbuf id-shader)
            gpu-texture (:gpu-texture (first scene-infos))]
        (gl/with-gl-bindings gl (assoc render-args :id (scene-picking/renderable-picking-id-uniform (first renderables))) [id-shader vertex-binding gpu-texture]
          (shader/set-samplers-by-index id-shader gl 0 (:texture-units gpu-texture))
          (gl/gl-draw-arrays gl GL/GL_TRIANGLES 0 num-vertices))))))

(defn- render-sprite-outlines [^GL2 gl render-args renderables _count]
  (assert (= pass/outline (:pass render-args)))
  (let [num-quads (count-quads renderables)
        outline-vertex-binding (vtx/use-with ::sprite-outline (gen-outline-vertex-buffer renderables num-quads) outline-shader)]
    (gl/with-gl-bindings gl render-args [outline-shader outline-vertex-binding]
      (gl/gl-draw-arrays gl GL/GL_LINES 0 (* num-quads 8)))))

; Node defs

(g/defnk produce-save-value [textures default-animation material material-attribute-infos blend-mode size-mode manual-size slice9 offset playback-rate vertex-attribute-overrides]
  (protobuf/make-map-without-defaults Sprite$SpriteDesc
    :default-animation default-animation
    :material (resource/resource->proj-path material)
    :blend-mode blend-mode
    :slice9 slice9
    :size-mode size-mode
    :size (v3->v4 manual-size)
    :offset offset
    :playback-rate playback-rate
    :attributes (graphics/vertex-attribute-overrides->save-values vertex-attribute-overrides material-attribute-infos)
    :textures (mapv #(update % :texture resource/proj-path) textures)))

(g/defnk produce-scene
  [_node-id aabb material-shader scene-infos blend-mode size-mode size slice9 material-attribute-infos vertex-attribute-bytes]
  (let [first-animation (:animation (first scene-infos))]
    (cond-> {:node-id _node-id
             :aabb aabb
             :renderable {:render-fn render-sprites
                          :batch-key [(mapv :gpu-texture scene-infos) blend-mode material-shader]
                          :select-batch-key _node-id
                          :tags #{:sprite}
                          :user-data {:shader material-shader
                                      :scene-infos scene-infos
                                      :material-attribute-infos material-attribute-infos
                                      :vertex-attribute-bytes vertex-attribute-bytes
                                      :blend-mode blend-mode
                                      :size-mode size-mode
                                      :size size
                                      :slice9 slice9
                                      :quad-count (quad-count size-mode slice9)}
                          :passes [pass/transparent pass/selection]}
             :children [{:node-id _node-id
                         :aabb aabb
                         :renderable {:render-fn render-sprite-outlines
                                      :batch-key [outline-shader]
                                      :tags #{:sprite :outline}
                                      :select-batch-key _node-id
                                      :user-data {:animation first-animation
                                                  :size-mode size-mode
                                                  :size size
                                                  :slice9 slice9
                                                  :quad-count (quad-count size-mode slice9)}
                                      :passes [pass/outline]}}]}

            (< 1 (count (:frames first-animation)))
            (assoc :updatable (texture-set/make-animation-updatable _node-id "Sprite" first-animation)))))

(defn- validate-material [_node-id material]
  (or (validation/prop-error :fatal _node-id :material validation/prop-nil? material "Material")
      (validation/prop-error :fatal _node-id :material validation/prop-resource-not-exists? material "Material")))

(g/defnk produce-build-targets [_node-id resource textures texture-binding-infos default-animation material material-attribute-infos material-max-page-count material-samplers material-shader blend-mode size-mode manual-size slice9 offset playback-rate vertex-attribute-bytes vertex-attribute-overrides]
  (g/precluding-errors
    (let [sampler-name->texture-binding-info (coll/pair-map-by :sampler texture-binding-infos)
          is-paged-material (and (shader/shader-lifecycle? material-shader)
                                 (shader/is-using-array-samplers? material-shader))
          unassigned-default-animation-error (validation/prop-error :fatal _node-id :default-animation validation/prop-empty? default-animation "Default Animation")]
      (concat
        ;; Validate material assignment.
        [(validate-material _node-id material)]

        ;; Validate sampler assignments. All samplers declared by the material
        ;; must be assigned a compatible texture, and the default animation must
        ;; be present in all the assigned textures.
        (mapcat
          (fn [sampler]
            (let [sampler-name (:name sampler)
                  label (if (= 1 (count material-samplers)) "Image" sampler-name)
                  {:keys [anim-data texture texture-page-count]} (sampler-name->texture-binding-info sampler-name)
                  unassigned-texture-error (or (validation/prop-error :fatal _node-id :textures validation/prop-nil? texture label)
                                               (validation/prop-error :fatal _node-id :textures validation/prop-resource-not-exists? texture label))]
              (if unassigned-texture-error
                [unassigned-texture-error]
                [(validation/prop-error :fatal _node-id :textures shader/page-count-mismatch-error-message is-paged-material texture-page-count material-max-page-count label)
                 (when (and anim-data (nil? unassigned-default-animation-error))
                   (validation/prop-error :fatal _node-id :textures validation/prop-anim-missing-in? default-animation anim-data label))])))
          material-samplers)

        ;; Validate default-animation assignment.
        (when (and (pos? (count material-samplers))
                   (pos? (count textures)))
          [unassigned-default-animation-error])))

    [(pipeline/make-protobuf-build-target
       _node-id resource Sprite$SpriteDesc
       {:default-animation default-animation
        :material material
        :blend-mode blend-mode
        :size-mode size-mode
        :size (v3->v4 manual-size)
        :slice9 slice9
        :offset offset
        :playback-rate playback-rate
        :attributes (graphics/vertex-attribute-overrides->build-target vertex-attribute-overrides vertex-attribute-bytes material-attribute-infos)
        :textures textures}
       nil
       (into [(resource/proj-path material)]
             (map (comp resource/proj-path :texture))
             textures))]))

(def ^:private fake-resource
  (reify resource/Resource
    (children [_])
    (ext [_] "")
    (resource-type [_])
    (source-type [_])
    (exists? [_] false)
    (read-only? [_] true)
    (symlink? [_] false)
    (path [_] "")
    (abs-path [_] "")
    (proj-path [_] "")
    (resource-name [_] "")
    (workspace [_])
    (resource-hash [_])
    (openable? [_] false)
    (editable? [_] false)
    (loaded? [_] false)))

(g/defnode TextureBinding
  (property sampler g/Str) ; Required protobuf field.
  (property texture resource/Resource ; Required protobuf field.
            (value (gu/passthrough texture-resource))
            (set (fn [evaluation-context self old-value new-value]
                   (project/resource-setter evaluation-context self old-value new-value
                                            [:resource :texture-resource]
                                            [:anim-data :anim-data]
                                            [:anim-ids :anim-ids]
                                            [:gpu-texture :gpu-texture]
                                            [:texture-page-count :texture-page-count]))))
  (input texture-resource resource/Resource)
  (input anim-data g/Any)
  (input anim-ids g/Any)
  (input gpu-texture g/Any)
  (input texture-page-count g/Int)
  (output texture-binding-info g/Any (g/fnk [_node-id sampler texture ^:try anim-data ^:try anim-ids ^:try texture-page-count :as info]
                                       (cond->
                                         info
                                         (g/error-value? anim-data) (dissoc :anim-data)
                                         (g/error-value? anim-ids) (dissoc :anim-ids)
                                         (g/error-value? texture-page-count) (dissoc :texture-page-count))))
  (output texture-binding-save-value g/Any (g/fnk [sampler texture :as info] info))
  (output scene-info g/Any (g/fnk [sampler gpu-texture anim-data :as info] info)))

(defn- create-texture-binding-tx [sprite sampler texture]
  (g/make-nodes (g/node-id->graph-id sprite) [texture-binding [TextureBinding
                                                               :sampler sampler
                                                               :texture texture]]
    (g/connect texture-binding :_node-id sprite :copied-nodes)
    (g/connect texture-binding :texture-binding-info sprite :texture-binding-infos)
    (g/connect texture-binding :texture-binding-save-value sprite :texture-binding-save-values)
    (g/connect texture-binding :scene-info sprite :scene-infos)))

(defn- clear-texture-binding-node-id [texture-binding-node-id _]
  (g/delete-node texture-binding-node-id))

(defn- set-texture-binding-id [sampler-name _ node-id _ new]
  (create-texture-binding-tx node-id sampler-name new))

(g/defnk produce-properties [^:unsafe _evaluation-context _declared-properties _node-id default-animation material-attribute-infos material-max-page-count material-samplers material-shader resource texture-binding-infos vertex-attribute-overrides]
  (let [workspace (resource/workspace resource)
        extension (workspace/resource-kind-extensions workspace :atlas _evaluation-context)
        is-paged-material (and (shader/shader-lifecycle? material-shader)
                               (shader/is-using-array-samplers? material-shader))
        texture-binding-index (util/name-index texture-binding-infos :sampler)
        material-sampler-index (if (g/error-value? material-samplers)
                                 {}
                                 (util/name-index material-samplers :name))
        combined-name+indices (-> #{}
                                  (into (map key) texture-binding-index)
                                  (into (map key) material-sampler-index))

        texture-binding-properties
        (->> combined-name+indices
             sort
             (mapv
               (fn [[sampler-name order :as name+order]]
                 (let [has-single-sampler (= 1 (count combined-name+indices))
                       property-key (keyword (str "__sampler__" sampler-name "__" order))
                       property-name (if has-single-sampler "Image" sampler-name)
                       label (if has-single-sampler (localization/message "property.image") sampler-name)]
                   (if-let [i (texture-binding-index name+order)]
                     ;; texture binding exists
                     (let [{:keys [anim-data sampler texture texture-page-count]
                            texture-binding-node-id :_node-id} (texture-binding-infos i)
                           should-be-deleted (nil? (material-sampler-index name+order))]
                       [property-key
                        (cond->
                          {:node-id texture-binding-node-id
                           :label label
                           :type resource/Resource
                           :value (cond-> texture should-be-deleted (or fake-resource))
                           :prop-kw :texture
                           :error (or
                                    (when should-be-deleted
                                      (g/->error _node-id :textures :warning texture
                                                 (format "'%s' is not defined in the material. Use the \"Clear Override\" command from the label's context menu to remove the property. If the sampler is necessary for the shader, add a missing sampler in the material"
                                                         sampler)))
                                    (validation/prop-error :info _node-id :textures validation/prop-nil? texture property-name)
                                    (validation/prop-error :fatal _node-id :textures validation/prop-resource-not-exists? texture property-name)
                                    (when (nil? texture-page-count)  ; nil from :try producing error-value
                                      (g/->error _node-id :textures :fatal texture "the assigned Image has internal errors"))
                                    (validation/prop-error :fatal _node-id :textures shader/page-count-mismatch-error-message is-paged-material texture-page-count material-max-page-count property-name)
                                    (when-not (coll/empty? default-animation)
                                      (validation/prop-error :fatal _node-id :textures validation/prop-anim-missing-in? default-animation anim-data property-name)))

                           :edit-type {:type resource/Resource
                                       :ext extension
                                       :clear-fn clear-texture-binding-node-id}}
                          should-be-deleted
                          (assoc :original-value fake-resource))])
                     ;; needed texture binding does not exist
                     [property-key
                      {:node-id _node-id
                       :label label
                       :value nil
                       :type resource/Resource
                       :error (validation/prop-error :info _node-id :texture validation/prop-nil? nil property-name)
                       :edit-type {:type resource/Resource
                                   :ext extension
                                   :set-fn (fn/partial set-texture-binding-id sampler-name)}}])))))

        attribute-properties
        (when-not (g/error-value? material-attribute-infos)
          (graphics/attribute-property-entries _node-id material-attribute-infos 0 vertex-attribute-overrides))]

    (-> _declared-properties
        (update :properties (fn [props]
                              (-> props
                                  (into texture-binding-properties)
                                  (into attribute-properties))))
        (update :display-order (fn [order]
                                 (-> []
                                     (into (map first) texture-binding-properties)
                                     (into order)
                                     (into (map first) attribute-properties)))))))

(defn- detect-and-apply-renames [texture-binding-infos material-samplers]
  (util/detect-and-apply-renames texture-binding-infos :sampler material-samplers :name))

(defmethod material/handle-sampler-names-changed ::SpriteNode
  [evaluation-context sprite-node old-name-index _new-name-index sampler-renames sampler-deletions]
  (let [texture-binding-infos (g/node-value sprite-node :texture-binding-infos evaluation-context)
        texture-binding-name-index (util/name-index texture-binding-infos :sampler)
        implied-texture-binding-info-renames (util/detect-renames texture-binding-name-index old-name-index)]
    (into []
          (mapcat
            (fn [[name+order index]]
              ;; Texture binding could be implicitly renamed if its name does
              ;; not match the material sampler name (can happen on load)
              (let [name+order (implied-texture-binding-info-renames name+order name+order)]
                (concat
                  (when-let [[new-name] (sampler-renames name+order)]
                    (g/set-property (:_node-id (texture-binding-infos index)) :sampler new-name))
                  (when (sampler-deletions name+order)
                    (g/delete-node (:_node-id (texture-binding-infos index))))))))
          texture-binding-name-index)))

(g/defnk produce-scene-infos [scene-infos default-animation material-samplers default-tex-params]
  (let [infos (util/detect-renames-and-update-all
                scene-infos :sampler
                material-samplers :name
                (fn [scene-info maybe-material-sampler]
                  (let [scene-info (assoc scene-info :animation (get (:anim-data scene-info) default-animation))]
                    (if maybe-material-sampler
                      (-> scene-info
                          (assoc :sampler (:name maybe-material-sampler))
                          (update :gpu-texture texture/set-params (material/sampler->tex-params maybe-material-sampler default-tex-params)))
                      (update scene-info :gpu-texture texture/set-params default-tex-params)))))
        infos (filterv :animation infos)]
    (reduce
      (fn [acc i]
        (let [start-texture-unit (-> (acc (dec i)) :gpu-texture :texture-units peek inc)]
          (update-in acc [i :gpu-texture] texture/set-base-unit start-texture-unit)))
      infos
      (range 1 (count infos)))))

(g/defnode SpriteNode
  (inherits resource-node/ResourceNode)
  (property default-animation g/Str ; Required protobuf field.
            (dynamic label (properties/label-dynamic :sprite :default-animation))
            (dynamic tooltip (properties/tooltip-dynamic :sprite :default-animation))
            (dynamic error (g/fnk [_node-id textures primary-texture-binding-info default-animation]
                             (when (pos? (count textures))
                               (or (validation/prop-error :info _node-id :default-animation validation/prop-empty? default-animation "Default Animation")
                                   (let [{:keys [anim-data sampler]} primary-texture-binding-info
                                         image-property-label (if (= 1 (count textures)) "Image" sampler)]
                                     (validation/prop-error :fatal _node-id :default-animation validation/prop-anim-missing-in? default-animation anim-data image-property-label))))))
            (dynamic edit-type (g/fnk [anim-ids] (properties/->choicebox anim-ids))))

  (property material resource/Resource ; Default assigned in load-fn.
            (value (gu/passthrough material-resource))
            (set (fn [evaluation-context self old-value new-value]
                   (project/resource-setter evaluation-context self old-value new-value
                                            [:resource :material-resource]
                                            [:shader :material-shader]
                                            [:samplers :material-samplers]
                                            [:max-page-count :material-max-page-count]
                                            [:attribute-infos :material-attribute-infos])))
            (dynamic edit-type (g/constantly {:type resource/Resource :ext #{"material"}}))
            (dynamic error (g/fnk [_node-id material]
                             (validate-material _node-id material))))

  (property blend-mode g/Any (default (protobuf/default Sprite$SpriteDesc :blend-mode))
            (dynamic tip (validation/blend-mode-tip blend-mode Sprite$SpriteDesc$BlendMode))
            (dynamic edit-type (g/constantly (properties/->pb-choicebox Sprite$SpriteDesc$BlendMode))))
  (property size-mode g/Keyword (default (protobuf/default Sprite$SpriteDesc :size-mode))
            (dynamic label (properties/label-dynamic :sprite :size-mode))
            (dynamic tooltip (properties/tooltip-dynamic :sprite :size-mode))
            (set (fn [evaluation-context self old-value new-value]
                   ;; Use the texture size for the :manual-size when the user switches
                   ;; from :size-mode-auto to :size-mode-manual.
                   (when (and (= :size-mode-auto old-value)
                              (= :size-mode-manual new-value)
                              (properties/user-edit? self :size-mode evaluation-context))
                     (when-some [animation (:animation (first (g/node-value self :scene-infos evaluation-context)))]
                       (let [texture-size [(double (:width animation)) (double (:height animation)) 0.0]]
                         (g/set-property self :manual-size texture-size))))))
            (dynamic edit-type (g/constantly (properties/->pb-choicebox Sprite$SpriteDesc$SizeMode))))
  (property manual-size types/Vec3 (default (v4->v3 (protobuf/default Sprite$SpriteDesc :size)))
            (dynamic visible (g/constantly false)))
  (property size types/Vec3 ; Just for presentation.
            (value (g/fnk [manual-size size-mode ^:try scene-infos]
                     (let [first-animation (when-not (g/error-value? scene-infos)
                                             (:animation (first scene-infos)))]
                       (if (and (some? first-animation)
                                (or (= :size-mode-auto size-mode)
                                    (= [0.0 0.0 0.0] manual-size)))
                         [(double (:width first-animation)) (double (:height first-animation)) 0.0]
                         manual-size))))
            (set (fn [_evaluation-context self _old-value new-value]
                   (g/set-property self :manual-size new-value)))
            (dynamic read-only? (g/fnk [size-mode] (= :size-mode-auto size-mode))))
  (property slice9 types/Vec4 (default (protobuf/default Sprite$SpriteDesc :slice9))
            (dynamic label (properties/label-dynamic :sprite :slice9))
            (dynamic tooltip (properties/tooltip-dynamic :sprite :slice9))
            (dynamic read-only? (g/fnk [size-mode] (= :size-mode-auto size-mode)))
            (dynamic edit-type (g/constantly {:type types/Vec4 :labels ["L" "T" "R" "B"]})))
  (property playback-rate g/Num (default (protobuf/default Sprite$SpriteDesc :playback-rate))
            (dynamic label (properties/label-dynamic :sprite :playback-rate))
            (dynamic tooltip (properties/tooltip-dynamic :sprite :playback-rate)))
  (property offset g/Num (default (protobuf/default Sprite$SpriteDesc :offset))
            (dynamic label (properties/label-dynamic :sprite :offset))
            (dynamic tooltip (properties/tooltip-dynamic :sprite :offset))
            (dynamic edit-type (g/constantly {:type :slider
                                              :min 0.0
                                              :max 1.0
                                              :precision 0.01})))
  (property vertex-attribute-overrides g/Any (default {})
            (dynamic visible (g/constantly false)))

  (input texture-binding-infos g/Any :array)
  (input texture-binding-save-values g/Any :array)
  (input scene-infos g/Any :array)
  (output scene-infos g/Any :cached produce-scene-infos)

  (output texture-binding-infos g/Any (g/fnk [texture-binding-infos ^:try material-samplers]
                                        (if (g/error-value? material-samplers)
                                          texture-binding-infos
                                          (detect-and-apply-renames texture-binding-infos material-samplers))))
  (output texture-binding-save-values g/Any (g/fnk [texture-binding-save-values ^:try material-samplers]
                                              (if (g/error-value? material-samplers)
                                                texture-binding-save-values
                                                (util/detect-and-apply-renames texture-binding-save-values :sampler material-samplers :name))))
  (output primary-texture-binding-info g/Any (g/fnk [texture-binding-infos ^:try material-samplers]
                                               (if (g/error-value? material-samplers)
                                                 (first texture-binding-infos)
                                                 (let [primary-sampler-name (:name (first material-samplers))]
                                                   (or (some (fn [texture-binding-info]
                                                               (when (= primary-sampler-name (:sampler texture-binding-info))
                                                                 texture-binding-info))
                                                             texture-binding-infos)
                                                       (first texture-binding-infos))))))
  (output textures g/Any (g/fnk [texture-binding-save-values]
                           (filterv :texture texture-binding-save-values)))
  (output anim-ids g/Any (g/fnk [primary-texture-binding-info] (:anim-ids primary-texture-binding-info)))

  (input material-resource resource/Resource)
  (input material-shader ShaderLifecycle)
  (input material-samplers g/Any)
  (input material-max-page-count g/Int)
  (input material-attribute-infos g/Any)
  (input default-tex-params g/Any)

  (input copied-nodes g/Any :array :cascade-delete)

  (output aabb AABB (g/fnk [size ^:try scene-infos]
                      (let [[^double width ^double height ^double depth] size
                            first-animation (when-not (g/error-value? scene-infos)
                                              (:animation (first scene-infos)))
                            anim-pivot (when first-animation
                                         (get-in first-animation [:frames 0 :pivot]))
                            [pivot-x pivot-y] (if anim-pivot
                                                anim-pivot
                                                [0.0 0.0])
                            half-width (* 0.5 width)
                            half-height (* 0.5 height)
                            half-depth (* 0.5 depth)

                            ; Adjusted calculations for min-x and min-y
                            min-x (- (* (- width) pivot-x) half-width)
                            min-y (- (* (- height) pivot-y) half-height)]
                        (geom/make-aabb (Point3d. min-x min-y (- half-depth))
                                        (Point3d. (+ min-x width) (+ min-y height) half-depth)))))
  (output save-value g/Any produce-save-value)
  (output scene g/Any :cached produce-scene)
  (output build-targets g/Any :cached produce-build-targets)
  (output _properties g/Properties :cached produce-properties)
  (output vertex-attribute-bytes g/Any :cached (g/fnk [_node-id material-attribute-infos vertex-attribute-overrides]
                                                 (graphics/attribute-bytes-by-attribute-key _node-id material-attribute-infos 0 vertex-attribute-overrides))))

(def ^:private default-material-proj-path (protobuf/default Sprite$SpriteDesc :material))

(defn- sanitize-sprite [{:keys [size size-mode slice9 material textures tile-set] :as sprite-desc}]
  {:pre [(map? sprite-desc)]} ; Sprite$SpriteDesc in map format.
  (cond-> (dissoc sprite-desc :tile-set)

          (= protobuf/vector4-zero slice9)
          (dissoc :slice9)

          (= :size-mode-auto size-mode)
          (dissoc :size-mode :size)

          (= protobuf/vector4-zero size)
          (dissoc :size)

          (nil? material)
          (assoc :material default-material-proj-path)

          (and (zero? (count textures))
               (pos? (count tile-set)))
          (assoc :textures [{:sampler "texture_sampler"
                             :texture tile-set}])))

(defn- load-sprite [project self resource sprite-desc]
  {:pre [(map? sprite-desc)]} ; Sprite$SpriteDesc in map format.
  (let [resolve-resource #(workspace/resolve-resource resource %)]
    (concat
      (g/connect project :default-tex-params self :default-tex-params)
      (gu/set-properties-from-pb-map self Sprite$SpriteDesc sprite-desc
        default-animation :default-animation
        material (resolve-resource (:material :or default-material-proj-path))
        blend-mode :blend-mode
        size-mode :size-mode
        manual-size (v4->v3 :size)
        slice9 :slice9
        offset :offset
        playback-rate :playback-rate
        vertex-attribute-overrides (graphics/override-attributes->vertex-attribute-overrides :attributes))
      (for [{:keys [sampler texture]} (:textures sprite-desc)]
        (create-texture-binding-tx self sampler (resolve-resource texture))))))

(defn register-resource-types [workspace]
  (resource-node/register-ddf-resource-type workspace
    :ext "sprite"
    :node-type SpriteNode
    :ddf-type Sprite$SpriteDesc
    :load-fn load-sprite
    :sanitize-fn sanitize-sprite
    :icon sprite-icon
    :icon-class :design
    :category (localization/message "resource.category.components")
    :view-types [:scene :text]
    :tags #{:component}
    :tag-opts {:component {:transform-properties #{:position :rotation :scale}}}
    :label (localization/message "resource.type.sprite")))
