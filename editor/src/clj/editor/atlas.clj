(ns editor.atlas
  (:require [clojure.string :as str]
            [dynamo.graph :as g]
            [editor.image :as image]
            [editor.image-util :as image-util]
            [editor.geom :as geom]
            [editor.core :as core]
            [editor.colors :as colors]
            [editor.dialogs :as dialogs]
            [editor.handler :as handler]
            [editor.gl :as gl]
            [editor.gl.shader :as shader]
            [editor.gl.texture :as texture]
            [editor.gl.vertex :as vtx]
            [editor.defold-project :as project]
            [editor.math :as math]
            [editor.properties :as properties]
            [editor.types :as types]
            [editor.workspace :as workspace]
            [editor.resource :as resource]
            [editor.resource-node :as resource-node]
            [editor.pipeline :as pipeline]
            [editor.pipeline.texture-set-gen :as texture-set-gen]
            [editor.pipeline.tex-gen :as tex-gen]
            [editor.scene :as scene]
            [editor.texture-set :as texture-set]
            [editor.outline :as outline]
            [editor.validation :as validation]
            [editor.gl.pass :as pass]
            [editor.graph-util :as gu])
  (:import [com.dynamo.atlas.proto AtlasProto$Atlas]
           [com.dynamo.textureset.proto TextureSetProto$TextureSet]
           [com.dynamo.tile.proto Tile$Playback]
           [editor.types Animation Image AABB]
           [java.awt.image BufferedImage]
           [com.jogamp.opengl GL GL2]
           [javax.vecmath Point3d]))

(set! *warn-on-reflection* true)

(def ^:const atlas-icon "icons/32/Icons_13-Atlas.png")
(def ^:const animation-icon "icons/32/Icons_24-AT-Animation.png")
(def ^:const image-icon "icons/32/Icons_25-AT-Image.png")

(vtx/defvertex texture-vtx
  (vec4 position)
  (vec2 texcoord0))

(shader/defshader pos-uv-vert
  (attribute vec4 position)
  (attribute vec2 texcoord0)
  (varying vec2 var_texcoord0)
  (defn void main []
    (setq gl_Position (* gl_ModelViewProjectionMatrix position))
    (setq var_texcoord0 texcoord0)))

(shader/defshader pos-uv-frag
  (varying vec2 var_texcoord0)
  (uniform sampler2D texture)
  (defn void main []
    (setq gl_FragColor (texture2D texture var_texcoord0.xy))))

; TODO - macro of this
(def atlas-shader (shader/make-shader ::atlas-shader pos-uv-vert pos-uv-frag))

(defn- render-rect
  [^GL2 gl rect color]
  (let [x0 (:x rect)
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

(defn render-image-outline
  [^GL2 gl render-args renderables]
  (doseq [renderable renderables]
    (when (:selected renderable)
      (render-rect gl (-> renderable :user-data :rect) scene/selected-outline-color)))
  (doseq [renderable renderables]
    (when (= (-> renderable :updatable :state :frame) (-> renderable :user-data :order))
      (render-rect gl (-> renderable :user-data :rect) colors/defold-pink))))

(defn render-images
  [^GL2 gl render-args renderables n]
  (condp = (:pass render-args)
    pass/outline
    (render-image-outline gl render-args renderables)

    pass/selection
    (run! #(render-rect gl (-> % :user-data :rect) [1.0 1.0 1.0 1.0]) renderables)))

(g/defnk produce-image-scene
  [_node-id path order image-path->rect animation-updatable]
  (let [rect (get image-path->rect path)]
    {:node-id _node-id
     :aabb (geom/rect->aabb rect)
     :renderable {:render-fn render-images
                  :batch-key ::atlas-image
                  :user-data {:rect rect
                              :order order}
                  :passes [pass/outline pass/selection]}
     :updatable animation-updatable}))

(defn- path->id [path]
  (-> path
      (str/split #"/")
      last
      (str/split #"\.(?=[^\.]+$)")
      first))

(defn image->animation [image id]
  (types/map->Animation {:id              id
                         :images          [image]
                         :fps             30
                         :flip-horizontal false
                         :flip-vertical   false
                         :playback        :playback-once-forward}))

(g/defnode AtlasImage
  (inherits outline/OutlineNode)

  (property size types/Vec2
    (value (g/fnk [image-size] [(:width image-size 0) (:height image-size 0)]))
    (dynamic edit-type (g/constantly {:type types/Vec2 :labels ["W" "H"]}))
    (dynamic read-only? (g/constantly true)))

  (property image resource/Resource
            (value (gu/passthrough image-resource))
            (set (fn [_evaluation-context self old-value new-value]
                   (project/resource-setter self old-value new-value
                                            [:resource :image-resource]
                                            [:size :image-size])))
            (dynamic visible (g/constantly false)))

  (input image-resource resource/Resource)
  (output image-resource resource/Resource (gu/passthrough image-resource))

  (input image-size g/Any)
  (input image-path->rect g/Any)

  (input child->order g/Any)
  (output order g/Any (g/fnk [_node-id child->order]
                        (child->order _node-id)))

  (input animation-updatable g/Any)

  (output path g/Str (g/fnk [image-resource] (resource/proj-path image-resource)))
  (output id g/Str (g/fnk [path] (path->id path)))
  (output atlas-image Image (g/fnk [path image-size]
                              (Image. path nil (:width image-size) (:height image-size))))
  (output animation Animation (g/fnk [atlas-image id]
                                      (image->animation atlas-image id)))
  (output node-outline outline/OutlineData :cached (g/fnk [_node-id id image-resource order]
                                                          (cond-> {:node-id _node-id
                                                                   :label id
                                                                   :order order
                                                                   :icon image-icon}
                                                                  (resource/openable-resource? image-resource) (assoc :link image-resource :outline-reference? false))))
  (output ddf-message g/Any :cached (g/fnk [path order] {:image path :order order}))
  (output scene g/Any :cached produce-image-scene))

(defn- sort-by-and-strip-order [images]
  (->> images
       (sort-by :order)
       (map #(dissoc % :order))))

(g/defnk produce-anim-ddf [id fps flip-horizontal flip-vertical playback img-ddf]
  {:id id
   :fps fps
   :flip-horizontal flip-horizontal
   :flip-vertical flip-vertical
   :playback playback
   :images (sort-by-and-strip-order img-ddf)})

(defn- attach-image [parent labels image-node]
  (concat
    (for [[from to] labels]
      (g/connect image-node from parent to))
    (g/connect image-node :_node-id             parent     :nodes)
    (g/connect image-node :node-outline         parent     :child-outlines)
    (g/connect image-node :ddf-message          parent     :img-ddf)
    (g/connect image-node :scene                parent     :child-scenes)
    (g/connect image-node :image-resource       parent     :image-resources)
    (g/connect image-node :atlas-image          parent     :atlas-images)
    (g/connect parent     :image-path->rect     image-node :image-path->rect)
    (g/connect parent     :updatable            image-node :animation-updatable)
    (g/connect parent     :child->order         image-node :child->order)))

(defn- attach-animation [atlas-node animation-node]
  (concat
   (g/connect animation-node :_node-id             atlas-node :nodes)
   (g/connect animation-node :animation            atlas-node :animations)
   (g/connect animation-node :id                   atlas-node :animation-ids)
   (g/connect animation-node :node-outline         atlas-node :child-outlines)
   (g/connect animation-node :ddf-message          atlas-node :anim-ddf)
   (g/connect animation-node :scene                atlas-node :child-scenes)
   (g/connect animation-node :image-resources      atlas-node :image-resources)
   (g/connect atlas-node     :image-path->rect     animation-node :image-path->rect)
   (g/connect atlas-node     :anim-data            animation-node :anim-data)
   (g/connect atlas-node     :gpu-texture          animation-node :gpu-texture)))

(defn- tx-attach-image-to-animation [animation-node image-node]
  (attach-image animation-node
                []
                image-node))

(defn render-animation
  [^GL2 gl render-args renderables n]
  (condp = (:pass render-args)
    pass/selection
    nil

    pass/overlay
    (texture-set/render-animation-overlay gl render-args renderables n ->texture-vtx atlas-shader)))

(g/defnk produce-animation-updatable
  [_node-id id anim-data]
  (texture-set/make-animation-updatable _node-id "Atlas Animation" (get anim-data id)))

(g/defnk produce-animation-scene
  [_node-id id child-scenes gpu-texture updatable anim-data]
  {:node-id    _node-id
   :aabb       (reduce geom/aabb-union (geom/null-aabb) (keep :aabb child-scenes))
   :renderable {:render-fn render-animation
                :batch-key nil
                :user-data {:gpu-texture gpu-texture
                            :anim-id     id
                            :anim-data   (get anim-data id)}
                :passes    [pass/overlay pass/selection]}
   :updatable  updatable
   :children   child-scenes})

(g/defnode AtlasAnimation
  (inherits core/Scope)
  (inherits outline/OutlineNode)

  (property id g/Str
            (dynamic error (validation/prop-error-fnk :fatal validation/prop-empty? id)))
  (property fps g/Int
            (default 30)
            (dynamic error (validation/prop-error-fnk :fatal validation/prop-negative? fps)))
  (property flip-horizontal g/Bool)
  (property flip-vertical   g/Bool)
  (property playback        types/AnimationPlayback
            (dynamic edit-type (g/constantly (properties/->pb-choicebox Tile$Playback))))

  (output child->order g/Any :cached (g/fnk [nodes] (zipmap nodes (range))))

  (input atlas-images Image :array)
  (input img-ddf g/Any :array)
  (input child-scenes g/Any :array)

  (input anim-data g/Any)

  (input image-resources g/Any :array)
  (output image-resources g/Any (gu/passthrough image-resources))

  (input image-path->rect g/Any)
  (output image-path->rect g/Any (gu/passthrough image-path->rect))

  (input gpu-texture g/Any)

  (output animation Animation (g/fnk [this id atlas-images fps flip-horizontal flip-vertical playback]
                                      (types/->Animation id atlas-images fps flip-horizontal flip-vertical playback)))

  (output node-outline outline/OutlineData :cached
    (g/fnk [_node-id id child-outlines] {:node-id _node-id
                                         :label id
                                         :children (sort-by :order child-outlines)
                                         :icon animation-icon
                                         :child-reqs [{:node-type AtlasImage
                                                       :tx-attach-fn tx-attach-image-to-animation}]}))
  (output ddf-message g/Any :cached produce-anim-ddf)
  (output updatable g/Any :cached produce-animation-updatable)
  (output scene g/Any :cached produce-animation-scene))

(g/defnk produce-save-value [margin inner-padding extrude-borders img-ddf anim-ddf]
  {:margin margin
   :inner-padding inner-padding
   :extrude-borders extrude-borders
   :images (sort-by-and-strip-order img-ddf)
   :animations anim-ddf})

(g/defnk produce-texture-build-target [_node-id packed-image texture-profile build-settings]
  (let [project (project/get-project _node-id)
        workspace (project/workspace project)
        compress? (:compress-textures? build-settings false)]
    (image/make-texture-build-target workspace _node-id packed-image texture-profile compress?)))

(g/defnk produce-build-targets [_node-id resource texture-set texture-build-target]
  (let [pb-msg (assoc texture-set :texture (:resource texture-build-target))
        dep-build-targets [texture-build-target]]
    [(pipeline/make-pb-map-build-target _node-id resource dep-build-targets TextureSetProto$TextureSet pb-msg)]))

(defn gen-renderable-vertex-buffer
  [width height]
  (let [x0 0
        y0 0
        x1 width
        y1 height]
    (persistent!
      (doto (->texture-vtx 6)
           (conj! [x0 y0 0 1 0 0])
           (conj! [x0 y1 0 1 0 1])
           (conj! [x1 y1 0 1 1 1])

           (conj! [x1 y1 0 1 1 1])
           (conj! [x1 y0 0 1 1 0])
           (conj! [x0 y0 0 1 0 0])))))

(defn- render-atlas
  [^GL2 gl render-args [renderable] n]
  (let [{:keys [pass]} render-args]
    (condp = pass
      pass/transparent
      (let [{:keys [user-data]} renderable
            {:keys [vbuf gpu-texture]} user-data
            vertex-binding (vtx/use-with ::atlas-binding vbuf atlas-shader)]
        (gl/with-gl-bindings gl render-args [gpu-texture atlas-shader vertex-binding]
          (shader/set-uniform atlas-shader gl "texture" 0)
          (gl/gl-draw-arrays gl GL/GL_TRIANGLES 0 6)))

      pass/outline
      (let [{:keys [aabb]} renderable
            [x0 y0] (math/vecmath->clj (types/min-p aabb))
            [x1 y1] (math/vecmath->clj (types/max-p aabb))
            [cr cg cb ca] scene/outline-color]
        (.glColor4d gl cr cg cb ca)
        (.glBegin gl GL2/GL_QUADS)
        (.glVertex3d gl x0 y0 0)
        (.glVertex3d gl x0 y1 0)
        (.glVertex3d gl x1 y1 0)
        (.glVertex3d gl x1 y0 0)
        (.glEnd gl)))))

(g/defnk produce-scene
  [_node-id aabb layout-size gpu-texture child-scenes]
  (let [[width height] layout-size]
    {:aabb aabb
     :renderable {:render-fn render-atlas
                  :user-data {:gpu-texture gpu-texture
                              :vbuf        (gen-renderable-vertex-buffer width height)}
                  :passes [pass/transparent pass/outline]}
     :children child-scenes}))

(g/defnk produce-texture-set-data
  [_node-id animations all-atlas-images margin inner-padding extrude-borders]
  (or (when-let [errors (->> [[margin "Margin"]
                              [inner-padding "Inner Padding"]
                              [extrude-borders "Extrude Borders"]]
                             (keep (fn [[v name]]
                                     (validation/prop-error :fatal _node-id :layout-result validation/prop-negative? v name)))
                             not-empty)]
        (g/error-aggregate errors))
      (texture-set-gen/atlas->texture-set-data animations all-atlas-images margin inner-padding extrude-borders)))

(g/defnk produce-anim-data
  [texture-set uv-transforms]
  (texture-set/make-anim-data texture-set uv-transforms))

(g/defnk produce-image-path->rect
  [layout-size layout-rects]
  (let [[w h] layout-size]
    (into {} (map (fn [{:keys [path x y width height]}]
                    [path (types/->Rect path x (- h height y) width height)]))
          layout-rects)))

(defn- tx-attach-image-to-atlas [atlas-node image-node]
  (attach-image atlas-node
                [[:animation :animations]
                 [:id :animation-ids]]
                image-node))

(defn- atlas-outline-sort-by-fn [v]
  [(:name (g/node-type* (:node-id v)))])

(g/defnode AtlasNode
  (inherits resource-node/ResourceNode)

  (property size types/Vec2
            (value (g/fnk [layout-size] layout-size))
            (dynamic edit-type (g/constantly {:type types/Vec2 :labels ["W" "H"]}))
            (dynamic read-only? (g/constantly true)))
  (property margin g/Int
            (default 0)
            (dynamic error (validation/prop-error-fnk :fatal validation/prop-negative? margin)))
  (property inner-padding g/Int
            (default 0)
            (dynamic error (validation/prop-error-fnk :fatal validation/prop-negative? inner-padding)))
  (property extrude-borders g/Int
            (default 0)
            (dynamic error (validation/prop-error-fnk :fatal validation/prop-negative? extrude-borders)))

  (output child->order g/Any :cached (g/fnk [nodes] (zipmap nodes (range))))

  (input build-settings g/Any)
  (input texture-profiles g/Any)
  (input atlas-images g/Any :array)
  (input animations Animation :array)
  (input animation-ids g/Str :array)
  (input img-ddf g/Any :array)
  (input anim-ddf g/Any :array)
  (input child-scenes g/Any :array)
  (input image-resources g/Any :array)

  (output texture-profile g/Any (g/fnk [texture-profiles resource]
                                  (tex-gen/match-texture-profile texture-profiles (resource/proj-path resource))))

  (output all-atlas-images           [Image]             :cached (g/fnk [animations]
                                                                   (into [] (comp (mapcat :images) (distinct)) animations)))

  (output texture-set-data g/Any               :cached produce-texture-set-data)
  (output layout-size      g/Any               (g/fnk [texture-set-data] (:size texture-set-data)))
  (output texture-set      g/Any               (g/fnk [texture-set-data] (:texture-set texture-set-data)))
  (output uv-transforms    g/Any               (g/fnk [texture-set-data] (:uv-transforms texture-set-data)))
  (output layout-rects     g/Any               (g/fnk [texture-set-data] (:rects texture-set-data)))

  (output packed-image     BufferedImage       :cached (g/fnk [_node-id texture-set-data image-resources]
                                                         (let [id->image (reduce (fn [ret image-resource]
                                                                                   (let [id (resource/proj-path image-resource)
                                                                                         image (image-util/read-image image-resource)]
                                                                                     (assoc ret id image)))
                                                                                 {}
                                                                                 (flatten image-resources))]
                                                           (texture-set-gen/layout-images (:layout texture-set-data) id->image))))

  (output aabb             AABB                (g/fnk [layout-size]
                                                 (if (= [0 0] layout-size)
                                                   (geom/null-aabb)
                                                   (let [[w h] layout-size]
                                                     (types/->AABB (Point3d. 0 0 0) (Point3d. w h 0))))))

  (output gpu-texture      g/Any               :cached (g/fnk [_node-id packed-image texture-profile]
                                                         (let [texture-data (texture/make-preview-texture-data packed-image texture-profile)]
                                                           (texture/texture-data->gpu-texture _node-id
                                                                                              texture-data
                                                                                              {:min-filter gl/nearest
                                                                                               :mag-filter gl/nearest}))))

  (output gpu-texture-generator g/Any          (g/fnk [gpu-texture] (image/make-variant-gpu-texture-generator gpu-texture)))

  (output anim-data        g/Any               :cached produce-anim-data)
  (output image-path->rect g/Any               :cached produce-image-path->rect)
  (output anim-ids         g/Any               :cached (gu/passthrough animation-ids))
  (output node-outline     outline/OutlineData :cached (g/fnk [_node-id child-outlines]
                                                         {:node-id    _node-id
                                                          :label      "Atlas"
                                                          :children   (vec (sort-by atlas-outline-sort-by-fn child-outlines))
                                                          :icon       atlas-icon
                                                          :child-reqs [{:node-type    AtlasImage
                                                                        :tx-attach-fn tx-attach-image-to-atlas}
                                                                       {:node-type    AtlasAnimation
                                                                        :tx-attach-fn attach-animation}]}))
  (output save-value       g/Any          :cached produce-save-value)
  (output texture-build-target g/Any      :cached produce-texture-build-target)
  (output build-targets    g/Any          :cached produce-build-targets)
  (output updatable        g/Any          (g/fnk [] nil))
  (output scene            g/Any          :cached produce-scene))

(defn- attach-image-nodes
  [parent labels image-resources]
  (let [graph-id (g/node-id->graph-id parent)]
    (for [image-resource image-resources]
      (g/make-nodes
        graph-id
        [atlas-image [AtlasImage {:image image-resource}]]
        (attach-image parent labels atlas-image)))))


(defn add-images [atlas-node img-resources]
  ; used by tests
  (attach-image-nodes atlas-node
                      [[:animation :animations]
                       [:id :animation-ids]]
                      img-resources))

(defn- attach-atlas-animation [atlas-node anim]
  (let [graph-id (g/node-id->graph-id atlas-node)
        project (project/get-project atlas-node)
        workspace (project/workspace project)
        image-resources (keep #(workspace/resolve-workspace-resource workspace (:image %)) (:images anim))]
    (g/make-nodes
     graph-id
     [atlas-anim [AtlasAnimation :flip-horizontal (:flip-horizontal anim) :flip-vertical (:flip-vertical anim)
                  :fps (:fps anim) :playback (:playback anim) :id (:id anim)]]
     (attach-animation atlas-node atlas-anim)
     (attach-image-nodes atlas-anim [] image-resources))))

(defn- update-int->bool [keys m]
  (reduce (fn [m key]
            (if (contains? m key)
              (update m key (complement zero?))
              m))
            m
            keys))

(defn load-atlas [project self resource atlas]
  (let [workspace (project/workspace project)
        image-resources (keep #(workspace/resolve-workspace-resource workspace (:image %)) (:images atlas))]
    (concat
      (g/connect project :build-settings self :build-settings)
      (g/connect project :texture-profiles self :texture-profiles)
      (g/set-property self :margin (:margin atlas))
      (g/set-property self :inner-padding (:inner-padding atlas))
      (g/set-property self :extrude-borders (:extrude-borders atlas))
      (attach-image-nodes self
                          [[:animation :animations]
                           [:id :animation-ids]]
                          image-resources)
      (map (comp (partial attach-atlas-animation self)
                 (partial update-int->bool [:flip-horizontal :flip-vertical]))
           (:animations atlas)))))

(defn register-resource-types [workspace]
  (resource-node/register-ddf-resource-type workspace
                                    :ext "atlas"
                                    :label "Atlas"
                                    :build-ext "texturesetc"
                                    :node-type AtlasNode
                                    :ddf-type AtlasProto$Atlas
                                    :load-fn load-atlas
                                    :icon atlas-icon
                                    :view-types [:scene :text]
                                    :view-opts {:scene {:grid false}}))

(defn- selection->atlas [selection] (handler/adapt-single selection AtlasNode))
(defn- selection->animation [selection] (handler/adapt-single selection AtlasAnimation))
(defn- selection->image [selection] (handler/adapt-single selection AtlasImage))

(def ^:private default-animation
  {:flip-horizontal false
   :flip-vertical false
   :fps 60
   :playback :playback-loop-forward
   :id "New Animation"})

(defn- add-animation-group-handler [atlas-node]
  (g/transact
   (concat
    (g/operation-label "Add Animation Group")
    (attach-atlas-animation atlas-node default-animation))))

(handler/defhandler :add :workbench
  (label [] "Add Animation Group")
  (active? [selection] (selection->atlas selection))
  (run [selection] (add-animation-group-handler (selection->atlas selection))))

(defn- add-images-handler [workspace project labels parent] ; parent = new parent of images
  (when-let [image-resources (seq (dialogs/make-resource-dialog workspace project {:ext image/exts :title "Select Images" :selection :multiple}))]
    (g/transact
     (concat
      (g/operation-label "Add Images")
      (attach-image-nodes parent labels image-resources)))))

(handler/defhandler :add-from-file :workbench
  (label [] "Add Images...")
  (active? [selection] (or (selection->atlas selection) (selection->animation selection)))
  (run [project selection] (let [workspace (project/workspace project)]
                             (if-let [atlas-node (selection->atlas selection)]
                               (add-images-handler workspace project
                                                   [[:animation :animations]
                                                    [:id :animation-ids]]
                                                   atlas-node)
                               (when-let [animation-node (selection->animation selection)]
                                 (add-images-handler workspace project [] animation-node))))))

(defn- vec-move
  [v x offset]
  (let [current-index (.indexOf ^java.util.List v x)
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

(handler/defhandler :move-up :workbench
  (active? [selection] (move-active? selection))
  (run [selection] (move-node! (selection->image selection) -1)))

(handler/defhandler :move-down :workbench
  (active? [selection] (move-active? selection))
  (run [selection] (move-node! (selection->image selection) 1)))
