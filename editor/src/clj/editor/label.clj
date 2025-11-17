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

(ns editor.label
  (:require [dynamo.graph :as g]
            [editor.build-target :as bt]
            [editor.colors :as colors]
            [editor.defold-project :as project]
            [editor.font :as font]
            [editor.geom :as geom]
            [editor.gl :as gl]
            [editor.gl.pass :as pass]
            [editor.gl.shader :as shader]
            [editor.gl.texture :as texture]
            [editor.gl.vertex2 :as vtx]
            [editor.graph-util :as gu]
            [editor.localization :as localization]
            [editor.material :as material]
            [editor.properties :as properties]
            [editor.protobuf :as protobuf]
            [editor.resource :as resource]
            [editor.resource-node :as resource-node]
            [editor.scene :as scene]
            [editor.scene-picking :as scene-picking]
            [editor.types :as types]
            [editor.validation :as validation]
            [editor.workspace :as workspace]
            [util.coll :refer [pair]])
  (:import [com.dynamo.gamesys.proto Label$LabelDesc Label$LabelDesc$BlendMode Label$LabelDesc$Pivot]
           [com.jogamp.opengl GL GL2]
           [editor.gl.shader ShaderLifecycle]))

(set! *warn-on-reflection* true)

(def label-icon "icons/32/Icons_39-GUI-Text-node.png")

; Render assets

(vtx/defvertex color-vtx
  (vec3 position)
  (vec4 color))

(shader/defshader vertex-shader
  (attribute vec4 position)
  (attribute vec4 color)
  (varying vec4 var_color)
  (defn void main []
    (setq gl_Position (* gl_ModelViewProjectionMatrix position))
    (setq var_color color)))

(shader/defshader fragment-shader
  (varying vec4 var_color)
  (defn void main []
    (setq gl_FragColor var_color)))

(def shader (shader/make-shader ::shader vertex-shader fragment-shader))

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

(shader/defshader label-id-vertex-shader
  (uniform mat4 view_proj)
  (attribute vec4 position)
  (defn void main []
    (setq gl_Position (* view_proj (vec4 position.xyz 1.0)))))

(shader/defshader label-id-fragment-shader
  (uniform vec4 id)
  (defn void main []
    (setq gl_FragColor id)))

(def id-shader (shader/make-shader ::label-id-shader label-id-vertex-shader label-id-fragment-shader {"view_proj" :view-proj "id" :id}))

; Vertex generation

(defn- gen-lines-vb
  [renderables]
  (let [vcount (transduce (map (comp count :line-data :user-data)) + renderables)]
    (when (pos? vcount)
      (vtx/flip! (reduce (fn [vb {:keys [world-transform user-data selected] :as renderable}]
                           (let [line-data (:line-data user-data)
                                 line-color (:line-color user-data)
                                 [r g b a] (or line-color (colors/renderable-outline-color renderable))]
                             (reduce (fn [vb [x y z]]
                                       (color-vtx-put! vb x y z r g b a))
                                     vb
                                     (geom/transf-p world-transform line-data))))
                         (->color-vtx vcount)
                         renderables)))))

(defn render-lines [^GL2 gl render-args renderables rcount]
  (when-let [vb (gen-lines-vb renderables)]
    (let [vertex-binding (vtx/use-with ::lines vb line-shader)]
      (gl/with-gl-bindings gl render-args [line-shader vertex-binding]
        (gl/gl-draw-arrays gl GL/GL_LINES 0 (count vb))))))

(defn- gen-vb [^GL2 gl renderables]
  (let [user-data (get-in renderables [0 :user-data])
        font-data (get-in user-data [:text-data :font-data])
        text-entries (mapv (fn [r] (let [text-data (get-in r [:user-data :text-data])
                                         alpha (get-in text-data [:color 3])]
                                     (-> text-data
                                         (assoc :world-transform (:world-transform r))
                                         (update-in [:outline 3] * alpha)
                                         (update-in [:shadow 3] * alpha))))
                           renderables)
        node-ids (into #{} (map :node-id) renderables)]
    (font/request-vertex-buffer gl node-ids font-data text-entries)))

(defn render-tris [^GL2 gl render-args renderables rcount]
  (let [renderable (first renderables)
        user-data (:user-data renderable)
        gpu-texture (or (get user-data :gpu-texture) @texture/white-pixel)
        vb (gen-vb gl renderables)
        vcount (count vb)]
    (when (> vcount 0)
      (condp = (:pass render-args)
        pass/transparent
        (let [material-shader (get user-data :material-shader)
              blend-mode (get user-data :blend-mode)
              shader (or material-shader shader)
              vertex-binding (vtx/use-with ::tris vb shader)]
          (gl/with-gl-bindings gl render-args [shader vertex-binding gpu-texture]
            (gl/set-blend-mode gl blend-mode)
            (gl/gl-draw-arrays gl GL/GL_TRIANGLES 0 vcount)
            (.glBlendFunc gl GL/GL_SRC_ALPHA GL/GL_ONE_MINUS_SRC_ALPHA)))

        pass/selection
        (let [vertex-binding (vtx/use-with ::tris-selection vb id-shader)]
          (gl/with-gl-bindings gl (assoc render-args :id (scene-picking/renderable-picking-id-uniform renderable)) [id-shader vertex-binding gpu-texture]
            (gl/gl-draw-arrays gl GL/GL_TRIANGLES 0 vcount)))))))

; Node defs

(defn- pivot->h-align [pivot]
  (case pivot
    (:pivot-e :pivot-ne :pivot-se) :right
    (:pivot-center :pivot-n :pivot-s) :center
    (:pivot-w :pivot-nw :pivot-sw) :left))

(defn- pivot->v-align [pivot]
  (case pivot
    (:pivot-ne :pivot-n :pivot-nw) :top
    (:pivot-e :pivot-center :pivot-w) :middle
    (:pivot-se :pivot-s :pivot-sw) :bottom))

(defn- pivot-offset [pivot size]
  (let [h-align (pivot->h-align pivot)
        v-align (pivot->v-align pivot)
        xs (case h-align
             :right -1.0
             :center -0.5
             :left 0.0)
        ys (case v-align
             :top -1.0
             :middle -0.5
             :bottom 0.0)]
    (mapv * size [xs ys 1])))

(g/defnk produce-save-value [text size color outline shadow leading tracking pivot blend-mode line-break font material]
  (protobuf/make-map-without-defaults Label$LabelDesc
    :text text
    :size (protobuf/vector3->vector4-zero size)
    :color color
    :outline outline
    :shadow shadow
    :leading leading
    :tracking tracking
    :pivot pivot
    :blend-mode blend-mode
    :line-break line-break
    :font (resource/resource->proj-path font)
    :material (resource/resource->proj-path material)))

(g/defnk produce-scene
  [_node-id aabb size gpu-texture material-shader blend-mode pivot text-data]
  (let [scene {:node-id _node-id
               :aabb aabb}
        font-map (get-in text-data [:font-data :font-map])
        texture-recip-uniform (font/get-texture-recip-uniform font-map)
        material-shader (assoc-in material-shader [:uniforms "texture_size_recip"] texture-recip-uniform)]
    (if text-data
      (let [[w h _] size
            offset (pivot-offset pivot size)
            lines (mapv conj (apply concat (take 4 (partition 2 1 (cycle (geom/transl offset [[0 0] [w 0] [w h] [0 h]]))))) (repeat 0))]
        (assoc scene :renderable {:render-fn render-tris
                                  :tags #{:text}
                                  :batch-key {:blend-mode blend-mode :gpu-texture gpu-texture :material-shader material-shader}
                                  :select-batch-key _node-id
                                  :user-data {:material-shader material-shader
                                              :blend-mode blend-mode
                                              :gpu-texture gpu-texture
                                              :line-data lines
                                              :text-data text-data}
                                  :passes [pass/transparent pass/selection]}
               :children [{:node-id _node-id
                           :aabb aabb
                           :renderable {:render-fn render-lines
                                        :tags #{:text :outline}
                                        :batch-key ::outline
                                        :user-data {:line-data lines}
                                        :passes [pass/outline]}}]))
      scene)))

(defn- build-label [resource dep-resources user-data]
  (let [pb (:proto-msg user-data)
        pb (reduce #(assoc %1 (first %2) (second %2)) pb (map (fn [[label res]] [label (resource/proj-path (get dep-resources res))]) (:dep-resources user-data)))]
    {:resource resource :content (protobuf/map->bytes Label$LabelDesc pb)}))

(g/defnk produce-build-targets [_node-id resource font material save-value dep-build-targets]
  (or (when-let [errors (->> [[font :font "Font"]
                              [material :material "Material"]]
                          (keep (fn [[v prop-kw name]]
                                  (validation/prop-error :fatal _node-id prop-kw validation/prop-nil? v name)))
                          not-empty)]
        (g/error-aggregate errors))
      (let [dep-build-targets (flatten dep-build-targets)
            deps-by-source (into {} (map #(let [res (:resource %)] [(:resource res) res]) dep-build-targets))
            dep-resources (map (fn [[label resource]] [label (get deps-by-source resource)]) [[:font font] [:material material]])]
        [(bt/with-content-hash
           {:node-id _node-id
            :resource (workspace/make-build-resource resource)
            :build-fn build-label
            :user-data {:proto-msg save-value
                        :dep-resources dep-resources}
            :deps dep-build-targets})])))

(g/defnode LabelNode
  (inherits resource-node/ResourceNode)

  ;; Ignored, except data during migration. See details below.
  (property legacy-scale types/Vec3 ; Nil is valid default.
            (dynamic visible (g/constantly false)))

  (property text g/Str (default (protobuf/default Label$LabelDesc :text))
            (dynamic edit-type (g/constantly {:type :multi-line-text}))
            (dynamic label (properties/label-dynamic :label :text))
            (dynamic tooltip (properties/tooltip-dynamic :label :text)))
  (property size types/Vec3 ; Required protobuf field.
            (dynamic label (properties/label-dynamic :label :size))
            (dynamic tooltip (properties/tooltip-dynamic :label :size)))
  (property color types/Color (default (protobuf/default Label$LabelDesc :color))
            (dynamic label (properties/label-dynamic :label :color))
            (dynamic tooltip (properties/tooltip-dynamic :label :color)))
  (property outline types/Color (default (protobuf/default Label$LabelDesc :outline))
            (dynamic label (properties/label-dynamic :label :outline))
            (dynamic tooltip (properties/tooltip-dynamic :label :outline)))
  (property shadow types/Color (default (protobuf/default Label$LabelDesc :shadow))
            (dynamic label (properties/label-dynamic :label :shadow))
            (dynamic tooltip (properties/tooltip-dynamic :label :shadow)))
  (property leading g/Num (default (protobuf/default Label$LabelDesc :leading))
            (dynamic label (properties/label-dynamic :label :leading))
            (dynamic tooltip (properties/tooltip-dynamic :label :leading)))
  (property tracking g/Num (default (protobuf/default Label$LabelDesc :tracking))
            (dynamic label (properties/label-dynamic :label :tracking))
            (dynamic tooltip (properties/tooltip-dynamic :label :tracking)))
  (property pivot g/Keyword (default (protobuf/default Label$LabelDesc :pivot))
            (dynamic edit-type (g/constantly (properties/->pb-choicebox Label$LabelDesc$Pivot)))
            (dynamic label (properties/label-dynamic :label :pivot))
            (dynamic tooltip (properties/tooltip-dynamic :label :pivot)))
  (property blend-mode g/Any (default (protobuf/default Label$LabelDesc :blend-mode))
            (dynamic tip (validation/blend-mode-tip blend-mode Label$LabelDesc$BlendMode))
            (dynamic edit-type (g/constantly (properties/->pb-choicebox Label$LabelDesc$BlendMode))))
  (property line-break g/Bool (default (protobuf/default Label$LabelDesc :line-break))
            (dynamic label (properties/label-dynamic :label :line-break))
            (dynamic tooltip (properties/tooltip-dynamic :label :line-break)))

  (property font resource/Resource ; Always assigned in load-fn.
            (value (gu/passthrough font-resource))
            (set (fn [evaluation-context self old-value new-value]
                   (project/resource-setter evaluation-context self old-value new-value
                                            [:resource :font-resource]
                                            [:gpu-texture :gpu-texture]
                                            [:font-map :font-map]
                                            [:font-data :font-data]
                                            [:build-targets :dep-build-targets])))
            (dynamic error (g/fnk [_node-id font]
                                  (or (validation/prop-error :info _node-id :image validation/prop-nil? font "Font")
                                      (validation/prop-error :fatal _node-id :image validation/prop-resource-not-exists? font "Font"))))
            (dynamic edit-type (g/constantly
                                 {:type resource/Resource
                                  :ext ["font"]}))
            (dynamic label (properties/label-dynamic :label :font))
            (dynamic tooltip (properties/tooltip-dynamic :label :font)))
  (property material resource/Resource ; Always assigned in load-fn.
            (value (gu/passthrough material-resource))
            (set (fn [evaluation-context self old-value new-value]
                   (project/resource-setter evaluation-context self old-value new-value
                                            [:resource :material-resource]
                                            [:shader :material-shader]
                                            [:samplers :material-samplers]
                                            [:build-targets :dep-build-targets])))
            (dynamic error (g/fnk [_node-id material]
                                  (or (validation/prop-error :info _node-id :image validation/prop-nil? material "Material")
                                      (validation/prop-error :fatal _node-id :image validation/prop-resource-not-exists? material "Material"))))
            (dynamic edit-type (g/constantly
                                 {:type resource/Resource
                                  :ext ["material"]})))

  (input font-resource resource/Resource)
  (input material-resource resource/Resource)
  (input dep-build-targets g/Any :array)
  (input gpu-texture g/Any)
  (input font-map g/Any)
  (input font-data font/FontData)
  (input material-shader ShaderLifecycle)
  (input material-samplers g/Any)

  (output save-value g/Any :cached produce-save-value)
  (output text-layout g/Any :cached (g/fnk [size font-map text line-break leading tracking]
                                           (font/layout-text font-map text line-break (first size) tracking leading)))
  (output text-data g/KeywordMap (g/fnk [text-layout font-data line-break color outline shadow pivot]
                                        (let [text-size [(:width text-layout) (:height text-layout) 0]]
                                          (cond-> {:text-layout text-layout
                                                   :font-data font-data
                                                   :color color
                                                   :outline outline
                                                   :shadow shadow
                                                   :align (pivot->h-align pivot)}
                                            font-data (assoc :offset (let [[x y] (pivot-offset pivot text-size)
                                                                           h (second text-size)]
                                                                       [x (+ y (- h (get-in font-data [:font-map :max-ascent])))]))))))
  (output aabb g/Any :cached (g/fnk [pivot size]
                               (let [offset-fn (partial mapv + (pivot-offset pivot size))
                                     [min-x min-y _] (offset-fn [0 0 0])
                                     [max-x max-y _] (offset-fn size)]
                                 (geom/coords->aabb [min-x min-y 0]
                                                    [max-x max-y 0]))))
  (output scene g/Any :cached produce-scene)
  (output build-targets g/Any :cached produce-build-targets)
  (output tex-params g/Any :cached (g/fnk [material-samplers]
                                     (some-> material-samplers first material/sampler->tex-params)))
  (output gpu-texture g/Any :cached (g/fnk [_node-id gpu-texture tex-params]
                                      (texture/set-params gpu-texture tex-params))))

(defn load-label [_project self resource label]
  {:pre [(map? label)]} ; Label$LabelDesc in map format.
  (let [resolve-resource #(workspace/resolve-resource resource %)]
    (gu/set-properties-from-pb-map self Label$LabelDesc label
      text :text
      size (protobuf/vector4->vector3 :size)
      legacy-scale (protobuf/vector4->vector3 :scale) ; Legacy field. Migrated to ComponentDesc or EmbeddedComponentDesc in PrototypeDesc when saving.
      color :color
      outline :outline
      shadow :shadow
      leading :leading
      tracking :tracking
      pivot :pivot
      blend-mode :blend-mode
      line-break :line-break
      font (resolve-resource :font)
      material (resolve-resource :material))))

(defn- sanitize-label [label-desc]
  (let [legacy-scale-v3 (some-> label-desc :scale protobuf/vector4->vector3)
        sanitized-label (protobuf/sanitize label-desc :size protobuf/sanitize-required-vector4-zero-as-vector3)
        sanitized-label (if (scene/significant-scale? legacy-scale-v3)
                          (assoc sanitized-label :scale (protobuf/vector3->vector4-one legacy-scale-v3))
                          (dissoc sanitized-label :scale))]
    sanitized-label))

(defn- sanitize-embedded-label-component [embedded-component-desc label-desc]
  (let [sanitized-embedded-component-desc
        (if (scene/significant-scale? (:scale embedded-component-desc))
          embedded-component-desc
          (let [label-legacy-scale-v3 (some-> label-desc :scale protobuf/vector4->vector3)]
            (if (scene/significant-scale? label-legacy-scale-v3)
              (assoc embedded-component-desc :scale label-legacy-scale-v3)
              (dissoc embedded-component-desc :scale))))
        sanitized-label-desc (dissoc label-desc :scale)]
    (pair sanitized-embedded-component-desc
          sanitized-label-desc)))

(def ^:private add-to-set (fnil conj #{}))

(defn- replace-default-scale-with-label-legacy-scale [evaluation-context referenced-component-scale referenced-component-node-id label-node-id]
  ;; The scale used to be stored in the LabelDesc before we had scale on the
  ;; ComponentDesc. Here we transfer the scale value from the LabelDesc to the
  ;; ComponentDesc if the ComponentDesc does not already have a significant
  ;; scale value, and the LabelDesc does.
  (if (= scene/default-scale referenced-component-scale)
    (let [label-legacy-scale (g/node-value label-node-id :legacy-scale evaluation-context)]
      (if (scene/significant-scale? label-legacy-scale)
        (do
          (g/flag-nodes-as-migrated! evaluation-context [referenced-component-node-id label-node-id])
          label-legacy-scale)
        referenced-component-scale))
    referenced-component-scale))

(defn- alter-referenced-label-component [referenced-component-node-id label-node-id]
  ;; The Label scale value has been moved to the GameObject$ComponentDesc or
  ;; GameObject$EmbeddedComponentDesc to reside alongside the existing position
  ;; and rotation values.
  ;;
  ;; We still retain the legacy scale value in the legacy-scale property of the
  ;; LabelNode so that it can be transferred to the ReferencedComponent node as
  ;; it references a `.label` resource at load time, but we don't include the
  ;; legacy scale value in the LabelNode save-data. This means that both the
  ;; `.label` file that contained a legacy scale value and any `.go` or
  ;; `.collection` files that refers to the `.label` will have unsaved changes
  ;; immediately after the project has been loaded. This ensures both files will
  ;; be saved.
  ;;
  ;; You might think we should use the established sanitation-mechanism to
  ;; perform the migration - and we do for embedded components where all the
  ;; migrated data is confined to a single file - but we can't do that for
  ;; migrations that span multiple files.
  ;;
  ;; If we do not force all the files involved to be saved, we could end up in a
  ;; "partial migration" scenario where we strip the legacy scale value from the
  ;; `.label` resource but do not save it to the `.go` or `.collection` file
  ;; that hosts the ComponentDesc. This can happen if the user edits the
  ;; `.label` but not the `.go` or `.collection` file and saves the project.
  (g/update-property-ec referenced-component-node-id :scale replace-default-scale-with-label-legacy-scale referenced-component-node-id label-node-id))

(defn register-resource-types [workspace]
  (resource-node/register-ddf-resource-type workspace
    :label (localization/message "resource.type.label")
    :ext "label"
    :node-type LabelNode
    :ddf-type Label$LabelDesc
    :load-fn load-label
    :sanitize-fn sanitize-label
    :icon label-icon
    :icon-class :design
    :view-types [:scene :text]
    :tags #{:component}
    :tag-opts {:component {:transform-properties #{:position :rotation :scale}
                           :alter-referenced-component-fn alter-referenced-label-component
                           :sanitize-embedded-component-fn sanitize-embedded-label-component}}))
