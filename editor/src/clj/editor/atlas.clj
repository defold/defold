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

(ns editor.atlas
  (:require [dynamo.graph :as g]
            [editor.app-view :as app-view]
            [editor.attachment :as attachment]
            [editor.camera :as c]
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
            [editor.handler :as handler]
            [editor.image :as image]
            [editor.image-util :as image-util]
            [editor.localization :as localization]
            [editor.math :as math]
            [editor.outline :as outline]
            [editor.pipeline :as pipeline]
            [editor.pipeline.tex-gen :as tex-gen]
            [editor.pipeline.texture-set-gen :as texture-set-gen]
            [editor.properties :as properties]
            [editor.protobuf :as protobuf]
            [editor.resource :as resource]
            [editor.resource-dialog :as resource-dialog]
            [editor.resource-io :as resource-io]
            [editor.resource-node :as resource-node]
            [editor.scene-picking :as scene-picking]
            [editor.scene-tools :as scene-tools]
            [editor.texture-set :as texture-set]
            [editor.types :as types]
            [editor.validation :as validation]
            [editor.workspace :as workspace]
            [internal.util :as util]
            [schema.core :as s]
            [util.coll :as coll :refer [pair]]
            [util.digestable :as digestable]
            [util.eduction :as e]
            [util.fn :as fn]
            [util.murmur :as murmur])
  (:import [com.dynamo.bob.pipeline AtlasUtil ShaderUtil$Common ShaderUtil$VariantTextureArrayFallback]
           [com.dynamo.bob.textureset TextureSetGenerator$LayoutResult TextureSetLayout]
           [com.dynamo.gamesys.proto AtlasProto$Atlas AtlasProto$AtlasAnimation AtlasProto$AtlasImage TextureSetProto$TextureSet Tile$Playback]
           [com.jogamp.opengl GL GL2]
           [editor.gl.vertex2 VertexBuffer]
           [editor.types AABB Animation Image]
           [java.awt.image BufferedImage]
           [java.nio ByteBuffer]
           [java.util List]
           [javax.vecmath AxisAngle4d Matrix4d Point3d Vector3d]))

(set! *warn-on-reflection* true)

(def ^:const atlas-icon "icons/32/Icons_13-Atlas.png")
(def ^:const animation-icon "icons/32/Icons_24-AT-Animation.png")
(def ^:const image-icon "icons/32/Icons_25-AT-Image.png")

(g/deftype ^:private NameCounts {s/Str s/Int})

(vtx/defvertex texture-vtx
  (vec4 position)
  (vec2 texcoord0)
  (vec1 page_index))

(vtx/defvertex color-vtx
  (vec3 position)
  (vec4 color))

(shader/defshader pos-uv-vert
  (attribute vec4 position)
  (attribute vec2 texcoord0)
  (attribute float page_index)
  (varying vec2 var_texcoord0)
  (varying float var_page_index)
  (defn void main []
    (setq gl_Position (* gl_ModelViewProjectionMatrix position))
    (setq var_texcoord0 texcoord0)
    (setq var_page_index page_index)))

(shader/defshader pos-uv-frag
  (varying vec2 var_texcoord0)
  (varying float var_page_index)
  (uniform sampler2DArray texture_sampler)
  (defn void main []
    (setq gl_FragColor (texture2DArray texture_sampler (vec3 var_texcoord0.xy var_page_index)))))

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
; JG: This is copied from sprite, but does the same thing - merge them?
(def outline-shader (shader/make-shader ::outline-shader outline-vertex-shader outline-fragment-shader))

(defn- get-rect-page-offset [layout-width page-index]
  (let [page-margin 32]
    (+ (* page-margin page-index) (* layout-width page-index))))

(defn- get-rect-transform [width page-index]
  (let [page-offset (get-rect-page-offset width page-index)]
    (doto (Matrix4d.)
      (.setIdentity)
      (.setTranslation (Vector3d. page-offset 0.0 0.0)))))

; TODO - macro of this
(def atlas-shader
  ;; TODO(instancing): Shouldn't we be transforming all shaders like this, really?
  (let [transformed-shader-result (ShaderUtil$VariantTextureArrayFallback/transform pos-uv-frag ShaderUtil$Common/MAX_ARRAY_SAMPLERS)
        augmented-fragment-shader-source (.source transformed-shader-result)

        array-sampler-name->uniform-names
        (into {}
              (map (fn [array-sampler-name]
                     (pair array-sampler-name
                           (mapv #(str array-sampler-name "_" %)
                                 (range ShaderUtil$Common/MAX_ARRAY_SAMPLERS)))))
              (.arraySamplers transformed-shader-result))]

    (shader/make-shader ::atlas-shader pos-uv-vert augmented-fragment-shader-source {} array-sampler-name->uniform-names)))

(defn- render-rect
  [^GL2 gl rect color offset-x]
  (let [x0 (+ offset-x (:x rect))
        y0 (:y rect)
        x1 (+ x0 (:width rect))
        y1 (+ y0 (:height rect))
        [cr cg cb ca] color]
    (.glColor4d gl cr cg cb ca)
    (.glBegin gl GL2/GL_QUADS)
    (.glVertex3d gl x0 y0 0)
    (.glVertex3d gl x0 y1 0)
    (.glVertex3d gl x1 y1 0)
    (.glVertex3d gl x1 y0 0)
    (.glEnd gl)))

(defn- renderables->outline-vertex-component-count
  [renderables]
  (transduce (map (comp count :vertices :geometry :rect :user-data))
             +
             0
             renderables))

(defn- gen-outline-vertex [^Matrix4d wt ^Point3d pt x y cr cg cb]
  (.set pt x y 0)
  (.transform wt pt)
  (vector-of :float (.x pt) (.y pt) (.z pt) cr cg cb 1.0))

(defn- conj-outline-shape! [^ByteBuffer buf ^Matrix4d wt ^Point3d pt rect layout-width layout-height cr cg cb]
  (let [vertex-data (-> rect :geometry :vertices)
        vertex-points (partition 2 vertex-data)
        vertex-line-points (mapcat identity (partition 2 1 vertex-points vertex-points))
        width (:width rect)
        height (:height rect)
        page-offset-x (get-rect-page-offset layout-width (:page rect))]
    (doseq [p vertex-line-points]
      (let [x (+ (:x rect) (* 0.5 width) (* width (first p)) page-offset-x)
            y (+ (:y rect) (* 0.5 height) (* height (second p)))]
        (vtx/buf-push-floats! buf (gen-outline-vertex wt pt x y cr cg cb))))))

(defn- gen-outline-vertex-buffer [renderables count]
  (let [tmp-point (Point3d.)
        ^VertexBuffer vbuf (->color-vtx count)
        ^ByteBuffer buf (.buf vbuf)]
    (doseq [renderable renderables]
      (let [[cr cg cb] (colors/renderable-outline-color renderable)
            world-transform (:world-transform renderable)
            user-data (:user-data renderable)
            rect (:rect user-data)
            layout-width (:layout-width user-data)
            layout-height (:layout-height user-data)]
        (conj-outline-shape! buf world-transform tmp-point rect layout-width layout-height cr cg cb)))
    (vtx/flip! vbuf)))

(defn- render-image-outlines
  [^GL2 gl render-args renderables n]
  (condp = (:pass render-args)
    pass/outline
    (let [vertex-count (renderables->outline-vertex-component-count renderables)
          outline-vertex-binding (vtx/use-with ::atlas-image-outline (gen-outline-vertex-buffer renderables vertex-count) outline-shader)]
      (gl/with-gl-bindings gl render-args [outline-shader outline-vertex-binding]
        (gl/gl-draw-arrays gl GL/GL_LINES 0 vertex-count)))))

(defn- render-image-selection
  [^GL2 gl render-args renderables n]
  (assert (= (:pass render-args) pass/selection))
  (assert (= n 1))
  (let [renderable (first renderables)
        picking-id (:picking-id renderable)
        id-color (scene-picking/picking-id->color picking-id)
        user-data (-> renderable :user-data)
        rect (:rect user-data)
        page-offset-x (get-rect-page-offset (:layout-width user-data) (:page rect))]
    (render-rect gl (:rect user-data) id-color page-offset-x)))

(defn- atlas-rect->editor-rect [rect]
  (types/->Rect (:path rect) (:x rect) (:y rect) (:width rect) (:height rect)))

(g/defnk produce-image-scene
  [_node-id image-resource order layout-size image-path->rect animation-updatable]
  (let [path (resource/proj-path image-resource)
        rect (get image-path->rect path)
        editor-rect (atlas-rect->editor-rect rect)
        [layout-width layout-height] layout-size
        page-index (:page rect)
        page-offset-x (get-rect-page-offset layout-width page-index)
        adjusted-editor-rect (assoc editor-rect :x (+ (:x editor-rect) page-offset-x))
        aabb (geom/rect->aabb adjusted-editor-rect)]
    {:node-id _node-id
     :aabb aabb
     :renderable {:render-fn render-image-outlines
                  :tags #{:atlas :outline}
                  :batch-key ::atlas-image
                  :user-data {:rect rect
                              :order order
                              :layout-width layout-width
                              :layout-height layout-height
                              :page-index page-index}
                  :passes [pass/outline]}
     :children [{:aabb aabb
                 :node-id _node-id
                 :renderable {:render-fn render-image-selection
                              :tags #{:atlas}
                              :user-data {:rect rect
                                          :layout-width layout-width}
                              :passes [pass/selection]}}]
     :updatable animation-updatable}))

(defn make-animation [id images]
  (types/map->Animation {:id              id
                         :images          images
                         :fps             30
                         :flip-horizontal false
                         :flip-vertical   false
                         :playback        :playback-none}))

(defn- unique-id-error [node-id id id-counts]
  (or (validation/prop-error :fatal node-id :id validation/prop-empty? id "Id")
      (validation/prop-error :fatal node-id :id (partial validation/prop-id-duplicate? id-counts) id)))

(defn- validate-image-id [node-id id id-counts]
  (when (and (some? id) (some? id-counts))
    (unique-id-error node-id id id-counts)))

(defn- validate-image-resource [node-id image-resource]
  (validation/prop-error :fatal node-id :image validation/prop-resource-missing? image-resource "Image"))

(g/defnode AtlasImage
  (inherits outline/OutlineNode)

  (property id g/Str ; Required protobuf field.
            (value (g/fnk [maybe-image-resource rename-patterns]
                     (some-> maybe-image-resource (texture-set-gen/resource-id rename-patterns))))
            (dynamic read-only? (g/constantly true))
            (dynamic error (g/fnk [_node-id id id-counts] (validate-image-id _node-id id id-counts))))

  (property size types/Vec2 ; Just for presentation.
            (value (g/fnk [maybe-image-size] [(:width maybe-image-size 0) (:height maybe-image-size 0)]))
            (dynamic edit-type (g/constantly {:type types/Vec2 :labels ["W" "H"]}))
            (dynamic read-only? (g/constantly true)))

  (property pivot types/Vec2
            (dynamic label (properties/label-dynamic :atlas.image :pivot))
            (dynamic tooltip (properties/tooltip-dynamic :atlas.image :pivot))
            (value (g/fnk [pivot-x pivot-y] [pivot-x pivot-y]))
            (set (fn [_evaluation-context self old-value new-value]
                   (concat
                     (g/set-property self :pivot-x (new-value 0))
                     (g/set-property self :pivot-y (new-value 1)))))
            (dynamic edit-type (g/constantly {:type types/Vec2 :labels ["X" "Y"]
                                              :precision 0.1})))

  (property pivot-x g/Num
            (default (protobuf/default AtlasProto$AtlasImage :pivot-x))
            (dynamic visible (g/constantly false)))
  (property pivot-y g/Num
            (default (protobuf/default AtlasProto$AtlasImage :pivot-y))
            (dynamic visible (g/constantly false)))

  (property sprite-trim-mode g/Keyword (default (protobuf/default AtlasProto$AtlasImage :sprite-trim-mode))
            (dynamic label (properties/label-dynamic :atlas.image :sprite-trim-mode))
            (dynamic tooltip (properties/tooltip-dynamic :atlas.image :sprite-trim-mode))
            (dynamic edit-type (g/constantly texture-set-gen/sprite-trim-mode-edit-type)))

  (property image resource/Resource ; Required protobuf field.
            (value (gu/passthrough maybe-image-resource))
            (set (fn [evaluation-context self old-value new-value]
                   (project/resource-setter evaluation-context self old-value new-value
                                            [:resource :maybe-image-resource]
                                            [:size :maybe-image-size])))
            (dynamic error (g/fnk [_node-id maybe-image-resource]
                             (validate-image-resource _node-id maybe-image-resource)))
            (dynamic edit-type (g/constantly
                                 {:type resource/Resource
                                  :ext image/exts})))

  (input id-counts NameCounts)
  (input maybe-image-resource resource/Resource)
  (output image-resource resource/Resource (g/fnk [_node-id maybe-image-resource maybe-image-size]
                                             ;; Depending on maybe-image-size provides ErrorValues from the image/ImageNode,
                                             ;; but we also want to guard against a non-assigned Image here.
                                             (or (validation/prop-error :fatal _node-id :image validation/prop-nil? maybe-image-resource "Image")
                                                 maybe-image-resource)))

  (input maybe-image-size g/Any)
  (input image-path->rect g/Any)
  (input rename-patterns g/Str)

  (input child->order g/Any)
  (output order g/Any (g/fnk [_node-id child->order]
                        (child->order _node-id)))

  (input animation-updatable g/Any)

  (input layout-size g/Any)

  (output atlas-image Image (g/fnk [_node-id image-resource maybe-image-size pivot-x pivot-y sprite-trim-mode]
                              (with-meta
                                (Image. image-resource nil (:width maybe-image-size) (:height maybe-image-size) pivot-x pivot-y sprite-trim-mode)
                                {:error-node-id _node-id})))
  (output atlas-images [Image] (g/fnk [atlas-image] [atlas-image]))
  (output animation Animation (g/fnk [atlas-image id]
                                (make-animation id [atlas-image])))
  (output node-outline outline/OutlineData :cached (g/fnk [_node-id build-errors id maybe-image-resource order]
                                                     (cond-> {:node-id _node-id
                                                              :node-outline-key (or id "<No Image>")
                                                              :label (or id (localization/message "outline.atlas.no-image"))
                                                              :order order
                                                              :icon image-icon
                                                              :outline-error? (g/error-fatal? build-errors)}

                                                             (resource/resource? maybe-image-resource)
                                                             (assoc :link maybe-image-resource :outline-show-link? true))))
  (output ddf-message g/Any (g/fnk [maybe-image-resource order sprite-trim-mode pivot-x pivot-y]
                              (-> (protobuf/make-map-without-defaults AtlasProto$AtlasImage
                                    :image (resource/resource->proj-path maybe-image-resource)
                                    :sprite-trim-mode sprite-trim-mode
                                    :pivot-x pivot-x
                                    :pivot-y pivot-y)
                                  (assoc :order order))))
  (output scene g/Any produce-image-scene)
  (output build-errors g/Any (g/fnk [_node-id id id-counts maybe-image-resource]
                               (g/package-errors _node-id
                                                 (validate-image-resource _node-id maybe-image-resource)
                                                 (validate-image-id _node-id id id-counts)))))

(defn- sort-by-and-strip-order [images]
  (->> images
       (sort-by :order)
       (mapv #(dissoc % :order))))

(g/defnk produce-anim-ddf [id fps flip-horizontal flip-vertical playback img-ddf]
  (protobuf/make-map-without-defaults AtlasProto$AtlasAnimation
    :id id
    :fps fps
    :flip-horizontal (protobuf/boolean->int flip-horizontal)
    :flip-vertical (protobuf/boolean->int flip-vertical)
    :playback playback
    :images (sort-by-and-strip-order img-ddf)))

(defn- attach-image-to-atlas [atlas-node image-node]
  (concat
    (g/connect image-node :_node-id         atlas-node :nodes)
    (g/connect image-node :animation        atlas-node :animations)
    (g/connect image-node :atlas-images     atlas-node :animation-images)
    (g/connect image-node :build-errors     atlas-node :child-build-errors)
    (g/connect image-node :ddf-message      atlas-node :img-ddf)
    (g/connect image-node :id               atlas-node :animation-ids)
    (g/connect image-node :image-resource   atlas-node :image-resources)
    (g/connect image-node :node-outline     atlas-node :child-outlines)
    (g/connect image-node :scene            atlas-node :child-scenes)
    (g/connect atlas-node :layout-size      image-node :layout-size)
    (g/connect atlas-node :child->order     image-node :child->order)
    (g/connect atlas-node :id-counts        image-node :id-counts)
    (g/connect atlas-node :image-path->rect image-node :image-path->rect)
    (g/connect atlas-node :updatable        image-node :animation-updatable)
    (g/connect atlas-node :rename-patterns  image-node :rename-patterns)))

(defn- attach-image-to-animation [animation-node image-node]
  (concat
    (g/connect image-node     :_node-id         animation-node :nodes)
    (g/connect image-node     :atlas-image      animation-node :atlas-images)
    (g/connect image-node     :build-errors     animation-node :child-build-errors)
    (g/connect image-node     :ddf-message      animation-node :img-ddf)
    (g/connect image-node     :image-resource   animation-node :image-resources)
    (g/connect image-node     :node-outline     animation-node :child-outlines)
    (g/connect image-node     :scene            animation-node :child-scenes)
    (g/connect animation-node :child->order     image-node     :child->order)
    (g/connect animation-node :image-path->rect image-node     :image-path->rect)
    (g/connect animation-node :layout-size      image-node     :layout-size)
    (g/connect animation-node :updatable        image-node     :animation-updatable)
    (g/connect animation-node :rename-patterns  image-node     :rename-patterns)))

(defn- attach-animation-to-atlas [atlas-node animation-node]
  (concat
    (g/connect animation-node :_node-id         atlas-node     :nodes)
    (g/connect animation-node :animation        atlas-node     :animations)
    (g/connect animation-node :atlas-images     atlas-node     :animation-images)
    (g/connect animation-node :build-errors     atlas-node     :child-build-errors)
    (g/connect animation-node :ddf-message      atlas-node     :anim-ddf)
    (g/connect animation-node :id               atlas-node     :animation-ids)
    (g/connect animation-node :image-resources  atlas-node     :image-resources)
    (g/connect animation-node :node-outline     atlas-node     :child-outlines)
    (g/connect animation-node :scene            atlas-node     :child-scenes)
    (g/connect atlas-node     :anim-data        animation-node :anim-data)
    (g/connect atlas-node     :gpu-texture      animation-node :gpu-texture)
    (g/connect atlas-node     :id-counts        animation-node :id-counts)
    (g/connect atlas-node     :layout-size      animation-node :layout-size)
    (g/connect atlas-node     :image-path->rect animation-node :image-path->rect)
    (g/connect atlas-node     :rename-patterns  animation-node :rename-patterns)))

(defn render-animation
  [^GL2 gl render-args renderables n]
  (texture-set/render-animation-overlay gl render-args renderables n ->texture-vtx atlas-shader))

(g/defnk produce-animation-updatable
  [_node-id id anim-data]
  (texture-set/make-animation-updatable _node-id "Atlas Animation" (get anim-data id)))

(g/defnk produce-animation-scene
  [_node-id id child-scenes gpu-texture updatable anim-data]
  {:node-id    _node-id
   :aabb       geom/null-aabb
   :renderable {:render-fn render-animation
                :tags #{:atlas}
                :batch-key nil
                :user-data {:gpu-texture gpu-texture
                            :anim-id     id
                            :anim-data   (get anim-data id)}
                :passes    [pass/overlay pass/selection]}
   :updatable  updatable
   :children   child-scenes})

(defn- validate-animation-id [node-id id id-counts]
  (unique-id-error node-id id id-counts))

(defn- validate-animation-fps [node-id fps]
  (validation/prop-error :fatal node-id :fps validation/prop-negative? fps "Fps"))

(g/defnode AtlasAnimation
  (inherits core/Scope)
  (inherits outline/OutlineNode)

  (property id g/Str ; Required protobuf field.
            (dynamic error (g/fnk [_node-id id id-counts] (validate-animation-id _node-id id id-counts))))
  (property fps g/Int (default (protobuf/default AtlasProto$AtlasAnimation :fps))
            (dynamic label (properties/label-dynamic :atlas.animation :fps))
            (dynamic tooltip (properties/tooltip-dynamic :atlas.animation :fps))
            (dynamic error (g/fnk [_node-id fps] (validate-animation-fps _node-id fps))))
  (property flip-horizontal g/Bool (default (protobuf/int->boolean (protobuf/default AtlasProto$AtlasAnimation :flip-horizontal)))
            (dynamic label (properties/label-dynamic :atlas.animation :flip-horizontal))
            (dynamic tooltip (properties/tooltip-dynamic :atlas.animation :flip-horizontal)))
  (property flip-vertical g/Bool (default (protobuf/int->boolean (protobuf/default AtlasProto$AtlasAnimation :flip-vertical)))
            (dynamic label (properties/label-dynamic :atlas.animation :flip-vertical))
            (dynamic tooltip (properties/tooltip-dynamic :atlas.animation :flip-vertical)))
  (property playback        types/AnimationPlayback (default (protobuf/default AtlasProto$AtlasAnimation :playback))
            (dynamic label (properties/label-dynamic :atlas.animation :playback))
            (dynamic tooltip (properties/tooltip-dynamic :atlas.animation :playback))
            (dynamic edit-type (g/constantly (properties/->pb-choicebox Tile$Playback))))

  (output child->order g/Any :cached (g/fnk [nodes] (zipmap nodes (range))))

  (input atlas-images Image :array)
  (output atlas-images [Image] (gu/passthrough atlas-images))

  (input img-ddf g/Any :array)
  (input child-scenes g/Any :array)
  (input child-build-errors g/Any :array)
  (input id-counts NameCounts)
  (input anim-data g/Any)
  (input layout-size g/Any)
  (output layout-size g/Any (gu/passthrough layout-size))

  (input rename-patterns g/Str)
  (output rename-patterns g/Str (gu/passthrough rename-patterns))

  (input image-resources g/Any :array)
  (output image-resources g/Any (gu/passthrough image-resources))

  (input image-path->rect g/Any)
  (output image-path->rect g/Any (gu/passthrough image-path->rect))

  (input gpu-texture g/Any)

  (output animation Animation (g/fnk [id atlas-images fps flip-horizontal flip-vertical playback]
                                (types/->Animation id atlas-images fps flip-horizontal flip-vertical playback)))

  (output node-outline outline/OutlineData :cached
          (g/fnk [_node-id child-outlines id own-build-errors]
            {:node-id _node-id
             :node-outline-key id
             :label id
             :children (sort-by :order child-outlines)
             :icon animation-icon
             :outline-error? (g/error-fatal? own-build-errors)
             :child-reqs [{:node-type AtlasImage
                           :tx-attach-fn attach-image-to-animation}]}))
  (output ddf-message g/Any :cached produce-anim-ddf)
  (output updatable g/Any :cached produce-animation-updatable)
  (output scene g/Any :cached produce-animation-scene)
  (output own-build-errors g/Any (g/fnk [_node-id fps id id-counts]
                                   (g/package-errors _node-id
                                                     (validate-animation-id _node-id id id-counts)
                                                     (validate-animation-fps _node-id fps))))
  (output build-errors g/Any (g/fnk [_node-id child-build-errors own-build-errors]
                               (g/package-errors _node-id
                                                 child-build-errors
                                                 own-build-errors))))

(g/defnk produce-save-value [margin inner-padding extrude-borders max-page-size img-ddf anim-ddf rename-patterns]
  (protobuf/make-map-without-defaults AtlasProto$Atlas
    :margin margin
    :inner-padding inner-padding
    :extrude-borders extrude-borders
    :images (sort-by-and-strip-order img-ddf)
    :animations anim-ddf
    :rename-patterns rename-patterns
    :max-page-width (max-page-size 0)
    :max-page-height (max-page-size 1)))

(defn- validate-margin [node-id margin]
  (validation/prop-error :fatal node-id :margin validation/prop-negative? margin "Margin"))

(defn- validate-inner-padding [node-id inner-padding]
  (validation/prop-error :fatal node-id :inner-padding validation/prop-negative? inner-padding "Inner Padding"))

(defn- validate-extrude-borders [node-id extrude-borders]
  (validation/prop-error :fatal node-id :extrude-borders validation/prop-negative? extrude-borders "Extrude Borders"))

(defn- max-page-size-error-message [[x y]]
  (cond
    (neg? x) "'Max Page Width' cannot be negative"
    (neg? y) "'Max Page Height' cannot be negative"
    (> x TextureSetLayout/MAX_ATLAS_DIMENSION) (format "'Max Page Width' cannot exceed %d" TextureSetLayout/MAX_ATLAS_DIMENSION)
    (> y TextureSetLayout/MAX_ATLAS_DIMENSION) (format "'Max Page Height' cannot exceed %d" TextureSetLayout/MAX_ATLAS_DIMENSION)
    :else nil))

(defn- validate-max-page-size [node-id page-size]
  (validation/prop-error :fatal node-id :validate-max-page-size max-page-size-error-message page-size))

(defn- validate-layout-properties [node-id margin inner-padding extrude-borders]
  (when-some [errors (->> [(validate-margin node-id margin)
                           (validate-inner-padding node-id inner-padding)
                           (validate-extrude-borders node-id extrude-borders)]
                          (filter some?)
                          (not-empty))]
    (g/error-aggregate errors)))

(defn- validate-rename-patterns [node-id rename-patterns]
  (try
    (AtlasUtil/validatePatterns rename-patterns)
    (catch Exception error
      (validation/prop-error :fatal node-id :rename-patterns identity (.getMessage error)))))

(g/defnk produce-build-targets [_node-id resource texture-set texture-page-count packed-page-images-generator texture-profile build-settings build-errors]
  (g/precluding-errors build-errors
    (let [project           (project/get-project _node-id)
          workspace         (project/workspace project)
          compress?         (:compress-textures? build-settings false)
          texture-target    (image/make-array-texture-build-target workspace _node-id packed-page-images-generator texture-profile texture-page-count compress?)
          pb-msg            (assoc texture-set :texture (-> texture-target :resource :resource))
          dep-build-targets [texture-target]]
      [(pipeline/make-protobuf-build-target _node-id resource TextureSetProto$TextureSet pb-msg dep-build-targets)])))

(g/defnk produce-atlas-texture-set-pb [texture-set]
  (let [pb-msg            texture-set
        texture-path      "" ; We don't have it at this point, as it's generated.
        content-pb        (protobuf/map->bytes TextureSetProto$TextureSet (assoc pb-msg :texture texture-path))]
    content-pb))

(defn gen-renderable-vertex-buffer
  [width height page-index]
  (let [page-offset-x (get-rect-page-offset width page-index)
        x0 page-offset-x
        y0 0
        x1 (+ width page-offset-x)
        y1 height
        v0 [x0 y0 0 1 0 0 page-index]
        v1 [x0 y1 0 1 0 1 page-index]
        v2 [x1 y1 0 1 1 1 page-index]
        v3 [x1 y0 0 1 1 0 page-index]
        ^VertexBuffer vbuf (->texture-vtx 6)
        ^ByteBuffer buf (.buf vbuf)]
    (doto buf
      (vtx/buf-push-floats! v0)
      (vtx/buf-push-floats! v1)
      (vtx/buf-push-floats! v2)
      (vtx/buf-push-floats! v2)
      (vtx/buf-push-floats! v3)
      (vtx/buf-push-floats! v0))
    (vtx/flip! vbuf)))

;; This is for rendering atlas texture pages!
(defn- render-atlas
  [^GL2 gl render-args [renderable] n]
  (let [{:keys [pass]} render-args]
    (condp = pass
      pass/transparent
      (let [{:keys [user-data]} renderable
            {:keys [vbuf gpu-texture]} user-data
            vertex-binding (vtx/use-with ::atlas-binding vbuf atlas-shader)]
        (gl/with-gl-bindings gl render-args [atlas-shader vertex-binding gpu-texture]
          (shader/set-samplers-by-index atlas-shader gl 0 (:texture-units gpu-texture))
          (gl/gl-draw-arrays gl GL/GL_TRIANGLES 0 6))))))

(defn- render-atlas-outline
  [^GL2 gl render-args [renderable] n]
  (let [{:keys [pass]} render-args]
    (condp = pass
      pass/outline
      (let [{:keys [aabb]} renderable
            [x0 y0] (math/vecmath->clj (types/min-p aabb))
            [x1 y1] (math/vecmath->clj (types/max-p aabb))
            [cr cg cb ca] colors/outline-color]
        (.glColor4d gl cr cg cb ca)
        (.glBegin gl GL2/GL_QUADS)
        (.glVertex3d gl x0 y0 0)
        (.glVertex3d gl x0 y1 0)
        (.glVertex3d gl x1 y1 0)
        (.glVertex3d gl x1 y0 0)
        (.glEnd gl)))))

(defn- produce-page-renderables
  [aabb layout-width layout-height page-index page-rects gpu-texture]
  {:aabb aabb
   :transform (get-rect-transform layout-width page-index)
   :renderable {:render-fn render-atlas
                :user-data {:gpu-texture gpu-texture
                            :vbuf (gen-renderable-vertex-buffer layout-width layout-height page-index)}
                :tags #{:atlas}
                :passes [pass/transparent]}
   :children [{:aabb aabb
               :renderable {:render-fn render-atlas-outline
                            :tags #{:atlas :outline}
                            :passes [pass/outline]}}]})

(g/defnk produce-scene
  [_node-id aabb layout-rects layout-size gpu-texture child-scenes texture-profile]
  (let [[width height] layout-size
        pages (group-by :page layout-rects)
        child-renderables (into [] (for [[page-index page-rects] pages] (produce-page-renderables aabb width height page-index page-rects gpu-texture)))]
    {:aabb aabb
     :info-text (format "%d x %d (%s profile)" width height (:name texture-profile))
     :children (into child-renderables
                     child-scenes)}))

(defn- generate-texture-set-data [{:keys [digest-ignored/error-node-id animations all-atlas-images margin inner-padding extrude-borders max-page-size]}]
  (try
    (texture-set-gen/atlas->texture-set-data animations all-atlas-images margin inner-padding extrude-borders max-page-size)
    (catch Exception error
      (g/->error error-node-id :max-page-size :fatal nil (.getMessage error)))))

(defn- call-generator [generator]
  ((:f generator) (:args generator)))

(defn- generate-packed-page-images [{:keys [digest-ignored/error-node-id image-resources layout-data-generator]}]
  (let [buffered-images (mapv #(resource-io/with-error-translation % error-node-id nil
                                 (image-util/read-image %))
                              image-resources)]
    (g/precluding-errors buffered-images
      (let [layout-data (call-generator layout-data-generator)]
        (g/precluding-errors layout-data
          (let [id->image (zipmap (map resource/proj-path image-resources) buffered-images)]
            (texture-set-gen/layout-atlas-pages (:layout layout-data) id->image)))))))

(g/defnk produce-layout-data-generator
  [_node-id animation-images all-atlas-images extrude-borders inner-padding margin max-page-size :as args]
  ;; The TextureSetGenerator.calculateLayout() method inherited from Bob also
  ;; compiles a TextureSetProto$TextureSet including the animation data in
  ;; addition to generating the layout. This means that modifying a property on
  ;; an animation will unnecessarily invalidate the layout, which in turn
  ;; invalidates the packed image texture. For the editor, we're only interested
  ;; in the layout-related properties of the produced TextureSetResult. To break
  ;; the dependency on animation properties, we supply a list of fake animations
  ;; to the TextureSetGenerator.calculateLayout() method that only includes data
  ;; that can affect the layout.
  (or (validate-layout-properties _node-id margin inner-padding extrude-borders)
      (let [fake-animations (mapv make-animation
                                  (repeat "")
                                  animation-images)
            augmented-args (-> args
                               (dissoc :_node-id :animation-images)
                               (assoc :animations fake-animations
                                      :digest-ignored/error-node-id _node-id))]
        {:f generate-texture-set-data
         :args augmented-args})))

(g/defnk produce-packed-page-images-generator
  [_node-id extrude-borders image-resources inner-padding margin layout-data-generator]
  (let [flat-image-resources (filterv some? (flatten image-resources))
        image-sha1s (map (fn [resource]
                           (resource-io/with-error-translation resource _node-id nil
                             (resource/resource->path-inclusive-sha1-hex resource)))
                         flat-image-resources)
        errors (filter g/error? image-sha1s)]
    (if (seq errors)
      (g/error-aggregate errors)
      (let [packed-image-sha1 (digestable/sha1-hash
                                {:extrude-borders extrude-borders
                                 :image-sha1s image-sha1s
                                 :inner-padding inner-padding
                                 :margin margin
                                 :type :packed-atlas-image})]
        {:f generate-packed-page-images
         :sha1 packed-image-sha1
         :args {:digest-ignored/error-node-id _node-id
                :image-resources flat-image-resources
                :layout-data-generator layout-data-generator}}))))

(defn- complete-ddf-animation [ddf-animation {:keys [flip-horizontal flip-vertical fps id playback] :as _animation}]
  (assert (boolean? flip-horizontal))
  (assert (boolean? flip-vertical))
  (assert (integer? fps))
  (assert (string? id))
  (assert (protobuf/val->pb-enum Tile$Playback playback))
  (assoc ddf-animation
    :flip-horizontal (if flip-horizontal 1 0)
    :flip-vertical (if flip-vertical 1 0)
    :fps (int fps)
    :id id
    :playback playback))

(g/defnk produce-texture-set-data
  ;; The TextureSetResult we generated in produce-layout-data-generator does not
  ;; contain the animation metadata since it was produced from fake animations.
  ;; In order to produce a valid TextureSetResult, we complete the protobuf
  ;; animations inside the embedded TextureSet with our animation properties.
  [animations layout-data all-atlas-images rename-patterns]
  (let [incomplete-ddf-texture-set (:texture-set layout-data)
        incomplete-ddf-animations (:animations incomplete-ddf-texture-set)
        animation-present-in-ddf? (comp coll/not-empty :images)
        animations-in-ddf (filter animation-present-in-ddf?
                                  animations)
        complete-ddf-animations (mapv complete-ddf-animation
                                      incomplete-ddf-animations
                                      animations-in-ddf)
        ;; Texture set must contain hashed references to images:
        ;; - for stand-alone images: as simple `base-name`
        ;; - for images in animations: as `animation/base-name`
        ;; Base name might be affected by rename patterns. Since we don't
        ;; provide animation names to Bob's layout algorithm, we need to fill
        ;; them here. The same image (i.e. rect) might be referenced multiple
        ;; times if it's used in different animations, that's fine and this is
        ;; how Bob does it in TextureSetGenerator. See also: comments in
        ;; texture_set_ddf.proto about `image_name_hashes` field
        fixed-image-name-hashes (-> []
                                    (into
                                      (map #(-> % :path (texture-set-gen/resource-id rename-patterns) murmur/hash64))
                                      all-atlas-images)
                                    (into
                                      (mapcat
                                        (fn [{:keys [id images]}]
                                          (e/map #(-> % :path (texture-set-gen/resource-id id rename-patterns) murmur/hash64)
                                                 images)))
                                      animations))
        complete-ddf-texture-set (assoc incomplete-ddf-texture-set
                                   :animations complete-ddf-animations
                                   :image-name-hashes fixed-image-name-hashes)]
    (assoc layout-data
      :texture-set complete-ddf-texture-set)))

(g/defnk produce-anim-data
  [texture-set uv-transforms]
  (texture-set/make-anim-data texture-set uv-transforms))

(s/defrecord AtlasRect
  [path     :- s/Any
   x        :- types/Int32
   y        :- types/Int32
   width    :- types/Int32
   height   :- types/Int32
   page     :- types/Int32
   geometry :- s/Any])

(defn rotate-vertices-90-cw [vertices]
  (into []
        (comp (partition-all 2)
              (mapcat (fn [[x y]]
                        [y (- x)])))
        vertices))

(g/defnk produce-image-path->rect
  [layout-size layout-rects texture-set]
  (let [[w h] layout-size
        geometries (:geometries texture-set)]
    (into {} (map (fn [{:keys [path x y width height index page]}]
                    (let [geometry (get geometries index)
                          rotated-vertices (if (:rotated geometry)
                                             (rotate-vertices-90-cw (:vertices geometry))
                                             (:vertices geometry))]
                      [path (->AtlasRect path x (- h height y) width height page
                                         (assoc geometry :vertices rotated-vertices))])))
          layout-rects)))

(defn- atlas-outline-sort-by-fn [basis v]
  ;; NOTE: unsafe basis from node output! Only use for node type access!
  (g/node-type-kw basis (:node-id v)))

(def ^:private default-max-page-size
  [(protobuf/default AtlasProto$Atlas :max-page-width)
   (protobuf/default AtlasProto$Atlas :max-page-height)])

(g/defnode AtlasNode
  (inherits resource-node/ResourceNode)

  (property size types/Vec2 ; Just for presentation.
            (value (g/fnk [layout-size] layout-size))
            (dynamic edit-type (g/constantly {:type types/Vec2 :labels ["W" "H"]}))
            (dynamic read-only? (g/constantly true)))
  (property margin g/Int (default (protobuf/default AtlasProto$Atlas :margin))
            (dynamic label (properties/label-dynamic :atlas :margin))
            (dynamic tooltip (properties/tooltip-dynamic :atlas :margin))
            (dynamic error (g/fnk [_node-id margin] (validate-margin _node-id margin))))
  (property inner-padding g/Int (default (protobuf/default AtlasProto$Atlas :inner-padding))
            (dynamic label (properties/label-dynamic :atlas :inner-padding))
            (dynamic tooltip (properties/tooltip-dynamic :atlas :inner-padding))
            (dynamic error (g/fnk [_node-id inner-padding] (validate-inner-padding _node-id inner-padding))))
  (property extrude-borders g/Int (default (protobuf/default AtlasProto$Atlas :extrude-borders))
            (dynamic label (properties/label-dynamic :atlas :extrude-borders))
            (dynamic tooltip (properties/tooltip-dynamic :atlas :extrude-borders))
            (dynamic error (g/fnk [_node-id extrude-borders] (validate-extrude-borders _node-id extrude-borders))))
  (property max-page-size types/Vec2 (default default-max-page-size)
            (dynamic label (properties/label-dynamic :atlas :max-page-size))
            (dynamic tooltip (properties/tooltip-dynamic :atlas :max-page-size))
            (dynamic edit-type (g/constantly {:type types/Vec2 :labels ["W" "H"]}))
            (dynamic error (g/fnk [_node-id max-page-size] (validate-max-page-size _node-id max-page-size))))
  (property rename-patterns g/Str (default (protobuf/default AtlasProto$Atlas :rename-patterns))
            (dynamic label (properties/label-dynamic :atlas :rename-patterns))
            (dynamic tooltip (properties/tooltip-dynamic :atlas :rename-patterns))
            (dynamic error (g/fnk [_node-id rename-patterns] (validate-rename-patterns _node-id rename-patterns))))

  (output child->order g/Any :cached (g/fnk [nodes] (zipmap nodes (range))))

  (input build-settings g/Any)
  (input texture-profiles g/Any)
  (input animations Animation :array)
  (input animation-ids g/Str :array)
  (input animation-images [Image] :array)
  (input img-ddf g/Any :array)
  (input anim-ddf g/Any :array)
  (input child-scenes g/Any :array)
  (input child-build-errors g/Any :array)
  (input image-resources g/Any :array)

  (output texture-profile g/Any (g/fnk [texture-profiles resource]
                                  (tex-gen/match-texture-profile texture-profiles (resource/proj-path resource))))

  (output all-atlas-images [Image] :cached (g/fnk [animation-images]
                                             (into [] (distinct) (flatten animation-images))))

  (output layout-data-generator g/Any          produce-layout-data-generator)
  (output texture-set-data g/Any               :cached produce-texture-set-data)
  (output layout-data      g/Any               :cached (g/fnk [layout-data-generator] (call-generator layout-data-generator)))
  (output layout-size      g/Any               (g/fnk [layout-data] (:size layout-data)))
  (output texture-set      g/Any               (g/fnk [texture-set-data] (:texture-set texture-set-data)))
  (output uv-transforms    g/Any               (g/fnk [layout-data] (:uv-transforms layout-data)))
  (output layout-rects     g/Any               (g/fnk [layout-data] (:rects layout-data)))

  (output texture-page-count g/Int             (g/fnk [layout-data max-page-size]
                                                 (if (every? pos? max-page-size)
                                                   (count (.layouts ^TextureSetGenerator$LayoutResult (:layout layout-data)))
                                                   texture/non-paged-page-count)))

  (output packed-page-images-generator g/Any   produce-packed-page-images-generator)

  (output packed-page-images [BufferedImage]   :cached (g/fnk [packed-page-images-generator] (call-generator packed-page-images-generator)))

  (output texture-set-pb   g/Any               :cached produce-atlas-texture-set-pb)

  (output aabb             AABB                (g/fnk [layout-size layout-rects]
                                                 (if (or (= [0 0] layout-size) (empty? layout-rects))
                                                   geom/null-aabb
                                                   (let [[w h] layout-size]
                                                     (types/->AABB (Point3d. 0 0 0) (Point3d. w h 0))))))

  (output gpu-texture      g/Any               :cached (g/fnk [_node-id packed-page-images texture-profile]
                                                         (let [page-texture-images+texture-bytes
                                                               (mapv #(tex-gen/make-preview-texture-image % texture-profile)
                                                                     packed-page-images)]
                                                           (texture/texture-images->gpu-texture
                                                             _node-id
                                                             page-texture-images+texture-bytes
                                                             {:min-filter gl/nearest
                                                              :mag-filter gl/nearest}))))

  (output anim-data        g/Any               :cached produce-anim-data)
  (output image-path->rect g/Any               :cached produce-image-path->rect)

  (output anim-ids         g/Any               :cached (g/fnk [animation-ids] (filter some? animation-ids)))
  (output id-counts        NameCounts          :cached (g/fnk [anim-ids] (frequencies anim-ids)))
  (output node-outline     outline/OutlineData :cached (g/fnk [^:unsafe _evaluation-context _node-id child-outlines own-build-errors]
                                                         ;; We use evaluation context to get child node types that should never change
                                                         {:node-id          _node-id
                                                          :node-outline-key "Atlas"
                                                          :label            (localization/message "outline.atlas")
                                                          :children         (vec (sort-by (partial atlas-outline-sort-by-fn (:basis _evaluation-context))  child-outlines))
                                                          :icon             atlas-icon
                                                          :outline-error?   (g/error-fatal? own-build-errors)
                                                          :child-reqs       [{:node-type    AtlasImage
                                                                              :tx-attach-fn attach-image-to-atlas}
                                                                             {:node-type    AtlasAnimation
                                                                              :tx-attach-fn attach-animation-to-atlas}]}))
  (output save-value       g/Any          :cached produce-save-value)
  (output build-targets    g/Any          :cached produce-build-targets)
  (output updatable        g/Any          (g/fnk [] nil))
  (output scene            g/Any          :cached produce-scene)
  (output own-build-errors g/Any          (g/fnk [_node-id extrude-borders inner-padding margin max-page-size rename-patterns]
                                            (g/package-errors _node-id
                                                              (validate-margin _node-id margin)
                                                              (validate-inner-padding _node-id inner-padding)
                                                              (validate-extrude-borders _node-id extrude-borders)
                                                              (validate-max-page-size _node-id max-page-size)
                                                              (validate-rename-patterns _node-id rename-patterns))))
  (output build-errors     g/Any          (g/fnk [_node-id child-build-errors own-build-errors]
                                            (g/package-errors _node-id
                                                              child-build-errors
                                                              own-build-errors))))

(defn- make-image-nodes
  [attach-fn parent image-msgs]
  (let [graph-id (g/node-id->graph-id parent)]
    (for [image-msg image-msgs]
      (g/make-nodes
        graph-id
        [atlas-image AtlasImage]
        (gu/set-properties-from-pb-map atlas-image AtlasProto$AtlasImage image-msg
          image :image
          sprite-trim-mode :sprite-trim-mode
          pivot-x :pivot-x
          pivot-y :pivot-y)
        (attach-fn parent atlas-image)))))

(def ^:private make-image-nodes-in-atlas (partial make-image-nodes attach-image-to-atlas))
(def ^:private make-image-nodes-in-animation (partial make-image-nodes attach-image-to-animation))
(def ^:private default-image-msg (protobuf/make-map-without-defaults AtlasProto$AtlasImage))

(defn add-images [atlas-node image-resources]
  ; used by tests
  (let [image-msgs (map #(assoc default-image-msg :image %) image-resources)]
    (make-image-nodes-in-atlas atlas-node image-msgs)))

(defn- resolve-image-msgs [workspace image-msgs remove-duplicates]
  (let [resolve-workspace-resource (partial workspace/resolve-workspace-resource workspace)]
    (into []
          (comp (remove (comp empty? :image))
                (if remove-duplicates
                  (util/distinct-by :image)
                  identity)
                (map (fn [atlas-image-msg]
                       (update atlas-image-msg :image resolve-workspace-resource))))
          image-msgs)))

(defn- make-atlas-animation [atlas-node atlas-animation]
  {:pre [(map? atlas-animation)]} ; AtlasProto$AtlasAnimation in map format.
  (let [graph-id (g/node-id->graph-id atlas-node)
        project (project/get-project atlas-node)
        workspace (project/workspace project)
        image-msgs (resolve-image-msgs workspace (:images atlas-animation) false)]
    (g/make-nodes
      graph-id
      [animation-node AtlasAnimation]
      (gu/set-properties-from-pb-map animation-node AtlasProto$AtlasAnimation atlas-animation
        id :id
        flip-horizontal (protobuf/int->boolean :flip-horizontal)
        flip-vertical (protobuf/int->boolean :flip-vertical)
        fps :fps
        playback :playback)
      (attach-animation-to-atlas atlas-node animation-node)
      (make-image-nodes-in-animation animation-node image-msgs))))

(defn load-atlas [project self resource atlas]
  {:pre [(map? atlas)]} ; AtlasProto$Atlas in map format.
  (let [workspace (resource/workspace resource)
        image-msgs (resolve-image-msgs workspace (:images atlas) true)]
    (concat
      (g/connect project :build-settings self :build-settings)
      (g/connect project :texture-profiles self :texture-profiles)
      (gu/set-properties-from-pb-map self AtlasProto$Atlas atlas
        margin :margin
        inner-padding :inner-padding
        extrude-borders :extrude-borders
        rename-patterns :rename-patterns)
      (let [{:keys [max-page-width max-page-height]} atlas]
        (when (or max-page-width max-page-height)
          (g/set-property self :max-page-size [(or max-page-width (default-max-page-size 0))
                                               (or max-page-height (default-max-page-size 1))])))
      (make-image-nodes-in-atlas self image-msgs)
      (map (partial make-atlas-animation self)
           (:animations atlas)))))

(defn- selection->atlas [selection] (handler/adapt-single selection AtlasNode))
(defn- selection->animation [selection] (handler/adapt-single selection AtlasAnimation))
(defn- selection->image [selection] (handler/adapt-single selection AtlasImage))

(def ^:private default-animation
  (protobuf/make-map-without-defaults AtlasProto$AtlasAnimation
    :id "New Animation"
    :playback :playback-loop-forward))

(defn- select!
  [app-view nodes op-seq]
  (g/transact
    (concat
      (g/operation-sequence op-seq)
      (app-view/select app-view nodes))))

(defn- add-animation-group-handler [app-view atlas-node]
  (let [op-seq (gensym)
        [animation-node] (g/tx-nodes-added
                           (g/transact
                             (concat
                               (g/operation-sequence op-seq)
                               (g/operation-label (localization/message "operation.atlas.add-animation"))
                               (make-atlas-animation atlas-node default-animation))))]
    (select! app-view [animation-node] op-seq)))

(handler/defhandler :edit.add-embedded-component :workbench
  :label (localization/message "command.edit.add-embedded-component.variant.atlas")
  (active? [selection] (selection->atlas selection))
  (run [app-view selection] (add-animation-group-handler app-view (selection->atlas selection))))

(defn- add-images-handler [app-view workspace project parent accept-fn] ; parent = new parent of images
  (when-some [image-resources (seq (resource-dialog/make workspace project
                                                         {:ext image/exts
                                                          :title (localization/message "dialog.add-atlas-images.title")
                                                          :selection :multiple
                                                          :accept-fn accept-fn}))]
    (let [op-seq (gensym)
          image-msgs (map #(assoc default-image-msg :image %) image-resources)
          image-nodes (g/tx-nodes-added
                        (g/transact
                          (concat
                            (g/operation-sequence op-seq)
                            (g/operation-label (localization/message "operation.atlas.add-images"))
                            (cond
                              (g/node-instance? AtlasNode parent)
                              (make-image-nodes-in-atlas parent image-msgs)

                              (g/node-instance? AtlasAnimation parent)
                              (make-image-nodes-in-animation parent image-msgs)

                              :else
                              (let [parent-node-type @(g/node-type* parent)]
                                (throw (ex-info (str "Unsupported parent type " (:name parent-node-type))
                                                {:parent-node-type parent-node-type})))))))]
      (select! app-view image-nodes op-seq))))

(handler/defhandler :edit.add-referenced-component :workbench
  :label (localization/message "command.edit.add-referenced-component.variant.atlas")
  (active? [selection] (or (selection->atlas selection) (selection->animation selection)))
  (run [app-view project selection]
    (let [atlas (selection->atlas selection)]
      (when-some [parent-node (or atlas (selection->animation selection))]
        (let [workspace (project/workspace project)
              accept-fn (if atlas
                          (complement (set (g/node-value atlas :image-resources)))
                          fn/constantly-true)]
          (add-images-handler app-view workspace project parent-node accept-fn))))))

(defn- vec-move
  [v x offset]
  (let [current-index (.indexOf ^List v x)
        new-index (max 0 (+ current-index offset))
        [before after] (split-at new-index (remove #(= x %) v))]
    (vec (concat before [x] after))))

(defn- move-node!
  [node-id offset]
  (let [parent (core/scope node-id)
        children (vec (g/node-value parent :nodes))
        new-children (vec-move children node-id offset)
        connections (keep (fn [[source source-label target target-label]]
                            (when (and (= source node-id)
                                       (= target parent))
                              [source-label target-label]))
                          (g/outputs node-id))]
    (g/transact
      (concat
        (for [child children
              [source target] connections]
          (g/disconnect child source parent target))
        (for [child new-children
              [source target] connections]
          (g/connect child source parent target))))))

(defn- move-active? [selection]
  (some->> selection
    selection->image
    core/scope
    (g/node-instance? AtlasAnimation)))

(handler/defhandler :edit.reorder-up :workbench
  (active? [selection] (move-active? selection))
  (enabled? [selection] (let [node-id (selection->image selection)
                              parent (core/scope node-id)
                              ^List children (vec (g/node-value parent :nodes))
                              node-child-index (.indexOf children node-id)]
                          (pos? node-child-index)))
  (run [selection] (move-node! (selection->image selection) -1)))

(handler/defhandler :edit.reorder-down :workbench
  (active? [selection] (move-active? selection))
  (enabled? [selection] (let [node-id (selection->image selection)
                              parent (core/scope node-id)
                              ^List children (vec (g/node-value parent :nodes))
                              node-child-index (.indexOf children node-id)]
                          (< node-child-index (dec (.size children)))))
  (run [selection] (move-node! (selection->image selection) 1)))

(defn- snap-axis
  [^double threshold ^double v]
  (or (->> [0.0 0.5 1.0]
           (some (fn [^double snap-value]
                   (when (< (Math/abs (- v snap-value)) threshold)
                     snap-value))))
      v))

(defn- updated-pivot
  [rect ^Vector3d delta snap-enabled snap-threshold]
  (let [{:keys [geometry ^double width ^double height]} rect
        {:keys [rotated ^double pivot-x ^double pivot-y]} geometry
        pivot-x (+ 0.5 pivot-x)
        pivot-y (- 0.5 pivot-y)
        delta-x (/ (cond-> ^double (.x delta) rotated -) width)
        delta-y (/ ^double (.y delta) height)
        pivot (if rotated
                [(- pivot-x delta-y) (+ pivot-y delta-x)]
                [(+ pivot-x delta-x) (- pivot-y delta-y)])]
    (cond->> pivot
      snap-enabled (mapv (partial snap-axis snap-threshold))
      :always (mapv properties/round-scalar))))

(defn- rect->absolute-pivot-pos
  [rect layout-width]
  (let [{:keys [geometry ^double x ^double y ^double width ^double height page]} rect
        {:keys [rotated ^double pivot-x ^double pivot-y]} geometry
        absolute-pivot-x (* pivot-x (if rotated height width))
        absolute-pivot-y (* pivot-y (if rotated width height))
        page-offset-x (get-rect-page-offset layout-width page)
        x (+ x page-offset-x (if rotated (- width absolute-pivot-y) absolute-pivot-x))
        y (+ y (- height (if rotated absolute-pivot-x absolute-pivot-y)))]
    [x y 0.0]))

(defn- pivot-handle
  [position]
  (concat
    (scene-tools/vtx-add position (scene-tools/vtx-scale [7.0 7.0 1.0] (scene-tools/gen-circle 64)))
    (scene-tools/vtx-add position (scene-tools/gen-point))))

(defn- action->pos [action ^Vector3d manip-pos manip-dir]
  (let [{:keys [world-pos world-dir]} action]
    (math/line-plane-intersection world-pos world-dir manip-pos manip-dir)))

(g/defnk produce-manip-delta
  [selected-renderables manip-space start-action action]
  (if (and start-action action)
    (let [{:keys [world-rotation world-transform]} (peek selected-renderables)
          manip-origin (math/translation world-transform)
          manip-rotation (scene-tools/get-manip-rotation manip-space world-rotation)
          lead-transform (doto (Matrix4d.) (.set manip-origin))
          manip-dir (math/rotate manip-rotation (Vector3d. 0.0 0.0 1.0))
          _ (.transform lead-transform manip-dir)
          _ (.normalize manip-dir)
          manip-pos (Point3d. (math/translation lead-transform))
          [start-pos pos] (mapv #(action->pos % manip-pos manip-dir) [start-action action])]
      (doto (Vector3d.) (.sub pos start-pos)))
    (Vector3d. 0.0 0.0 0.0)))

(defn- show-pivot? [selected-renderables]
  (-> selected-renderables util/only :user-data :rect :geometry :pivot-x))

(g/defnk produce-renderables [_node-id manip-space camera viewport selected-renderables manip-delta snap-enabled snap-threshold]
  (if-not (show-pivot? selected-renderables)
    {}
    (let [reference-renderable (last selected-renderables)
          {:keys [world-rotation]} reference-renderable
          {:keys [rect layout-width]} (:user-data reference-renderable)
          [pivot-x pivot-y] (updated-pivot rect manip-delta snap-enabled snap-threshold)
          rect (-> rect
                   (assoc-in [:geometry :pivot-x] pivot-x)
                   (assoc-in [:geometry :pivot-y] pivot-y))
          pivot-pos (rect->absolute-pivot-pos rect layout-width)
          scale (scene-tools/scale-factor camera viewport (Vector3d. (first pivot-pos) (second pivot-pos) 0.0))
          world-transform (scene-tools/manip-world-transform reference-renderable manip-space scale)
          adjusted-pivot-pos (mapv #(/ ^double % scale) pivot-pos)
          vertices (pivot-handle adjusted-pivot-pos)
          inv-view (doto (c/camera-view-matrix camera) (.invert))
          renderables [(scene-tools/gen-manip-renderable _node-id :move-pivot manip-space world-rotation world-transform (AxisAngle4d.) vertices colors/defold-turquoise inv-view)]]
      {pass/manipulator renderables
       pass/manipulator-selection renderables})))

(g/defnk produce-scale [selected-renderables camera viewport]
  (when (show-pivot? selected-renderables)
    (let [{:keys [rect layout-width]} (-> selected-renderables util/only :user-data)
          pivot-pos (rect->absolute-pivot-pos rect layout-width)]
      (scene-tools/scale-factor camera viewport (Vector3d. (first pivot-pos) (second pivot-pos) 0.0)))))

(defn- image-resources->image-msgs
  [image-resources]
  (mapv (partial hash-map :image) image-resources))

(defn- create-dropped-images
  [parent image-resources]
  (condp g/node-instance? parent
    AtlasNode (let [existing-image-resources (set (g/node-value parent :image-resources))
                    new-image? (complement existing-image-resources)]
                (->> (filter new-image? image-resources)
                     (image-resources->image-msgs)
                     (make-image-nodes-in-atlas parent)))
    AtlasAnimation (->> (image-resources->image-msgs image-resources)
                        (make-image-nodes-in-animation parent))))

(defn- handle-drop
  [root-id selection _workspace _world-pos resources]
  (let [parent (or (handler/adapt-single selection AtlasAnimation)
                   (some #(core/scope-of-type % AtlasAnimation) selection)
                   root-id)]
    (->> resources
         (e/filter image/image-resource?)
         (create-dropped-images parent))))

(defn handle-input [self action selection-data]
  (case (:type action)
    :mouse-pressed (if (first (get selection-data self))
                     (do
                       (g/transact
                         (concat
                           (g/set-property self :start-action action)
                           (g/set-property self :action action)
                           (g/set-property self :op-seq (gensym))))
                       nil)
                     action)
    :mouse-released (if (g/node-value self :start-action)
                      (let [selected-renderables (g/node-value self :selected-renderables)
                            op-seq (g/node-value self :op-seq)
                            manip-delta (g/node-value self :manip-delta)
                            snap-enabled (g/node-value self :snap-enabled)
                            snap-threshold (g/node-value self :snap-threshold)
                            reference-renderable (last selected-renderables)
                            rect (-> reference-renderable :user-data :rect)
                            pivot (updated-pivot rect manip-delta snap-enabled snap-threshold)]
                        (g/transact
                          (concat
                            (g/operation-label (localization/message "operation.atlas.move-pivot-point"))
                            (g/operation-sequence op-seq)
                            (g/set-property (:node-id reference-renderable) :pivot pivot)))
                        (g/transact
                          (g/set-property self :start-action nil))
                        nil)
                      action)
    :mouse-moved (if (g/node-value self :start-action)
                   (do
                     (g/transact
                       (g/set-property self :action action))
                     nil)
                   action)
    action))

(g/defnode AtlasToolController
  (property prefs g/Any)
  (property start-action g/Any)
  (property action g/Any)
  (property op-seq g/Any)

  (input active-tool g/Keyword)
  (input manip-space g/Keyword)
  (input camera g/Any)
  (input viewport g/Any)
  (input selected-renderables g/Any)

  (output scale g/Any :cached produce-scale)
  (output snap-threshold g/Any :cached (g/fnk [scale] (cond-> 0.1 scale (* ^double scale))))
  (output snap-enabled g/Bool :cached (g/fnk [start-action action] (and start-action (:shift action))))
  (output manip-delta g/Any :cached produce-manip-delta)
  (output renderables pass/RenderData :cached produce-renderables)
  (output input-handler Runnable :cached (g/constantly handle-input))
  (output info-text g/Str (g/constantly nil)))

(defn- get-animation-images [animation evaluation-context]
  (let [child->order (g/node-value animation :child->order evaluation-context)]
    (vec (sort-by child->order (keys child->order)))))

(defn register-resource-types [workspace]
  (concat
    (attachment/register
      workspace AtlasAnimation :images
      :add {AtlasImage attach-image-to-animation}
      :get get-animation-images)
    (attachment/register
      workspace AtlasNode :animations
      :add {AtlasAnimation attach-animation-to-atlas}
      :get (attachment/nodes-by-type-getter AtlasAnimation))
    (attachment/register
      workspace AtlasNode :images
      :add {AtlasImage attach-image-to-atlas}
      :get (attachment/nodes-by-type-getter AtlasImage))
    (resource-node/register-ddf-resource-type workspace
      :ext "atlas"
      :label "Atlas"
      :build-ext "a.texturesetc"
      :node-type AtlasNode
      :ddf-type AtlasProto$Atlas
      :load-fn load-atlas
      :icon atlas-icon
      :icon-class :design
      :view-types [:scene :text]
      :view-opts {:scene {:drop-fn handle-drop
                          :tool-controller AtlasToolController}})))
