(ns editor.collision-object
  (:require
   [clojure.string :as s]
   [dynamo.graph :as g]
   [editor.app-view :as app-view]
   [editor.collision-groups :as collision-groups]
   [editor.defold-project :as project]
   [editor.geom :as geom]
   [editor.gl :as gl]
   [editor.gl.pass :as pass]
   [editor.gl.shader :as shader]
   [editor.gl.vertex :as vtx]
   [editor.graph-util :as gu]
   [editor.handler :as handler]
   [editor.math :as math]
   [editor.outline :as outline]
   [editor.properties :as properties]
   [editor.protobuf :as protobuf]
   [editor.resource :as resource]
   [editor.resource-node :as resource-node]
   [editor.types :as types]
   [editor.validation :as validation]
   [editor.workspace :as workspace]
   [editor.scene :as scene]
   [editor.scene-tools :as scene-tools])
  (:import
   [com.dynamo.physics.proto Physics$CollisionObjectDesc
    Physics$CollisionObjectType
    Physics$CollisionShape$Shape]
   [com.jogamp.opengl GL GL2]
   [javax.vecmath Point3d Matrix4d Quat4d Vector3d]))

(set! *warn-on-reflection* true)

(defn- v4->euler [v]
  (math/quat->euler (doto (Quat4d.) (math/clj->vecmath v))))

(def collision-object-icon "icons/32/Icons_49-Collision-object.png")

(def shape-type-ui
  {:type-sphere  {:label "Sphere"
                  :icon  "icons/32/Icons_45-Collistionshape-convex-Sphere.png"
                  :physics-types #{"2D" "3D"}}
   :type-box     {:label "Box"
                  :icon  "icons/32/Icons_44-Collistionshape-convex-Box.png"
                  :physics-types #{"2D" "3D"}}
   :type-capsule {:label "Capsule"
                  :icon  "icons/32/Icons_46-Collistionshape-convex-Cylinder.png"
                  :physics-types #{"3D"}}})

(defn- shape-type-label
  [shape-type]
  (get-in shape-type-ui [shape-type :label]))

(defn- shape-type-icon
  [shape-type]
  (get-in shape-type-ui [shape-type :icon]))

(defn- shape-type-physics-types
  [shape-type]
  (get-in shape-type-ui [shape-type :physics-types]))

(defn- project-physics-type [project-settings]
  (if (.equalsIgnoreCase "2D" (get project-settings ["physics" "type"] "2D"))
    "2D"
    "3D"))

(g/defnode Shape
  (inherits outline/OutlineNode)
  (inherits scene/SceneNode)

  (input color g/Any)
  
  (property shape-type g/Any
            (dynamic visible (g/constantly false)))

  (output transform-properties g/Any scene/produce-unscalable-transform-properties)
  (output shape-data g/Any :abstract)
  (output scene g/Any :abstract)

  (output shape g/Any (g/fnk [shape-type position rotation shape-data]
                        {:shape-type shape-type
                         :position position
                         :rotation rotation
                         :data shape-data}))

  (output node-outline outline/OutlineData :cached (g/fnk [_node-id shape-type]
                                                     {:node-id _node-id
                                                      :label (shape-type-label shape-type)
                                                      :icon (shape-type-icon shape-type)})))

;;--------------------------------------------------------------------
;; rendering

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

(def outline-alpha 1.0)
(def shape-alpha 0.1)
(def selected-outline-alpha 1.0)
(def selected-shape-alpha 0.3)

(defn- gen-vertex
  [^Matrix4d wt ^Point3d pt x y cr cg cb ca]
  (.set pt x y 0)
  (.transform wt pt)
  [(.x pt) (.y pt) (.z pt) cr cg cb ca])

(defn- gen-vertex-buffer
  "Generate a vertex buffer for count renderables.

  vbuf-fn is a two arity function:
  - ([count]) should return a vertex buffer with required capacity
  - ([vbuf renderable tmp-point]) should conj! vertices for given renderable"
  [renderables count vbuf-fn]
  (let [tmp-point (Point3d.)]
    (loop [[renderable & rest] renderables
           vbuf (vbuf-fn count)]
      (if-not renderable
        (persistent! vbuf)
        (recur rest (vbuf-fn vbuf renderable tmp-point))))))


;; sphere

(def disc-segments 32)

(defn- conj-disc-outline!
  [vbuf ^Matrix4d wt ^Point3d pt r cr cg cb ca]
  (reduce (fn [vbuf [x y _]]
            (conj! vbuf (gen-vertex wt pt x y cr cg cb ca)))
          vbuf
          (->> geom/origin-geom
               (geom/transl [0 r 0])
               (geom/circling disc-segments))))

(defn- gen-disc-outline-vertex-buffer
  ([count]
   (->color-vtx (* count disc-segments)))
  ([vbuf {:keys [selected world-transform user-data] :as renderable} tmp-point]
   (let [{:keys [sphere-diameter color]} user-data
         [cr cg cb] color
         ca (if selected selected-outline-alpha outline-alpha)
         r (* 0.5 sphere-diameter)]
     (conj-disc-outline! vbuf world-transform tmp-point r cr cg cb ca))))

(defn- gen-disc-vertex-buffer
  ([count]
   (->color-vtx (* count (+ 2 disc-segments))))
  ([vbuf {:keys [selected world-transform user-data] :as renderable} tmp-point]
   (let [{:keys [sphere-diameter color]} user-data
         [cr cg cb] color
         ca (if selected selected-shape-alpha shape-alpha)
         r (* 0.5 sphere-diameter)]
     (-> vbuf
         (conj! (gen-vertex world-transform tmp-point 0 0 cr cg cb ca))
         (conj-disc-outline! world-transform tmp-point r cr cg cb ca)
         (conj! (gen-vertex world-transform tmp-point 0 r cr cg cb ca))))))

(defn- render-sphere
  [^GL2 gl render-args renderables n]
  (let [pass (:pass render-args)]
    (condp = pass
      pass/outline
      (let [vbuf (gen-vertex-buffer renderables n gen-disc-outline-vertex-buffer)
            vbuf-binding (vtx/use-with ::box-outline vbuf shader)]
        (gl/with-gl-bindings gl render-args [shader vbuf-binding]
          (gl/gl-draw-arrays gl GL/GL_LINE_LOOP 0 (count vbuf))))

      pass/transparent
      (let [vbuf (gen-vertex-buffer renderables n gen-disc-vertex-buffer)
            vbuf-binding (vtx/use-with ::box vbuf shader)]
        (gl/with-gl-bindings gl render-args [shader vbuf-binding]
          (gl/gl-draw-arrays gl GL/GL_TRIANGLE_FAN 0 (count vbuf))))

      pass/selection
      (let [vbuf (gen-vertex-buffer renderables n gen-disc-vertex-buffer)
            vbuf-binding (vtx/use-with ::box vbuf shader)]
        (gl/with-gl-bindings gl render-args [shader vbuf-binding]
          (gl/gl-draw-arrays gl GL/GL_TRIANGLE_FAN 0 (count vbuf)))))))


;; box

(defn- conj-outline-quad! [vbuf ^Matrix4d wt ^Point3d pt width height cr cg cb ca]
  (let [x1 (* 0.5 width)
        y1 (* 0.5 height)
        x0 (- x1)
        y0 (- y1)
        v0 (gen-vertex wt pt x0 y0 cr cg cb ca)
        v1 (gen-vertex wt pt x1 y0 cr cg cb ca)
        v2 (gen-vertex wt pt x1 y1 cr cg cb ca)
        v3 (gen-vertex wt pt x0 y1 cr cg cb ca)]
    (-> vbuf (conj! v0) (conj! v1) (conj! v1) (conj! v2) (conj! v2) (conj! v3) (conj! v3) (conj! v0))))

(defn- gen-box-outline-vertex-buffer
  ([count]
   (->color-vtx (* count 8)))
  ([vbuf {:keys [selected world-transform user-data] :as renderable} tmp-point]
   (let [{:keys [box-width box-height color]} user-data
         [cr cg cb] color
         ca (if selected selected-outline-alpha outline-alpha)]
     (conj-outline-quad! vbuf world-transform tmp-point box-width box-height cr cg cb ca))))

(defn- gen-box-vertex-buffer
  ([count]
   (->color-vtx (* count 8)))
  ([vbuf {:keys [selected world-transform user-data] :as renderable} tmp-point]
   (let [{:keys [box-width box-height color]} user-data
         [cr cg cb] color
         ca (if selected selected-shape-alpha shape-alpha)]
     (conj-outline-quad! vbuf world-transform tmp-point box-width box-height cr cg cb ca))))

(defn render-box
  [^GL2 gl render-args renderables n]
  (let [pass (:pass render-args)]
    (condp = pass
      pass/outline
      (let [vbuf (gen-vertex-buffer renderables n gen-box-outline-vertex-buffer)
            vbuf-binding (vtx/use-with ::box-outline vbuf shader)]
        (gl/with-gl-bindings gl render-args [shader vbuf-binding]
          (gl/gl-draw-arrays gl GL/GL_LINES 0 (count vbuf))))

      pass/transparent
      (let [vbuf (gen-vertex-buffer renderables n gen-box-vertex-buffer)
            vbuf-binding (vtx/use-with ::box vbuf shader)]
        (gl/with-gl-bindings gl render-args [shader vbuf-binding]
          (gl/gl-draw-arrays gl GL2/GL_QUADS 0 (count vbuf))))

      pass/selection
      (let [vbuf (gen-vertex-buffer renderables n gen-box-vertex-buffer)
            vbuf-binding (vtx/use-with ::box vbuf shader)]
        (gl/with-gl-bindings gl render-args [shader vbuf-binding]
          (gl/gl-draw-arrays gl GL2/GL_QUADS 0 (count vbuf)))))))


;; capsule

(def capsule-segments 32) ; needs to be divisible by 4

(defn- capsule-geometry
  [r h]
  ;; generate a circle, split in quarters and translate the halfs
  ;; up/down, with some overlap between top/bottom for straight edges
  (let [ps  (->> geom/origin-geom
                 (geom/transl [0 r 0])
                 (geom/circling capsule-segments))
        quarter (/ capsule-segments 4)
        top-l    (subvec ps 0             (inc quarter))
        bottom-l (subvec ps quarter       (* 2 quarter))
        bottom-r (subvec ps (* 2 quarter) (inc (* 3 quarter)))
        top-r    (subvec ps (* 3 quarter) (* 4 quarter))
        ext-y (* 0.5 h)]
    (concat
     (geom/transl [0 ext-y 0] top-l)
     (geom/transl [0 (- ext-y) 0] bottom-l)
     (geom/transl [0 (- ext-y) 0] bottom-r)
     (geom/transl [0 ext-y 0] top-r))))

(defn- conj-capsule-outline!
  [vbuf ^Matrix4d wt ^Point3d pt r h cr cg cb ca]
  (reduce (fn [vbuf [x y _]]
            (conj! vbuf (gen-vertex wt pt x y cr cg cb ca)))
          vbuf
          (capsule-geometry r h)))

(defn- gen-capsule-outline-vbuf
  ([count]
   (->color-vtx (* count (+ 2 capsule-segments))))
  ([vbuf {:keys [selected world-transform user-data] :as renderable} tmp-point]
   (let [{:keys [capsule-diameter capsule-height color]} user-data
         [cr cg cb] color
         ca (if selected selected-outline-alpha outline-alpha)
         r (* 0.5 capsule-diameter)]
     (conj-capsule-outline! vbuf world-transform tmp-point r capsule-height cr cg cb ca))))

(defn gen-capsule-vbuf
  ([count]
   (->color-vtx (* count (+ 4 capsule-segments))))
  ([vbuf {:keys [selected world-transform user-data] :as renderable} tmp-point]
   (let [{:keys [capsule-diameter capsule-height color]} user-data
         [cr cg cb] color
         ca (if selected selected-shape-alpha shape-alpha)
         r (* 0.5 capsule-diameter)
         ext-y (* 0.5 capsule-height)]
     (-> vbuf
         (conj! (gen-vertex world-transform tmp-point 0 0 cr cg cb ca))
         (conj-capsule-outline! world-transform tmp-point r capsule-height cr cg cb ca)
         (conj! (gen-vertex world-transform tmp-point 0 (+ r ext-y) cr cg cb ca))))))

(defn render-capsule
  [^GL2 gl render-args renderables n]
  (let [pass (:pass render-args)
        segments 32]
    (condp = pass
      pass/outline
      (let [vbuf (gen-vertex-buffer renderables n gen-capsule-outline-vbuf)
            vertex-binding (vtx/use-with ::capsule-outline vbuf shader)]
        (gl/with-gl-bindings gl render-args [shader vertex-binding]
          (gl/gl-draw-arrays gl GL/GL_LINE_LOOP 0 (count vbuf))))

      pass/transparent
      (let [vbuf (gen-vertex-buffer renderables n gen-capsule-vbuf)
            vertex-binding (vtx/use-with ::capsule vbuf shader)]
        (gl/with-gl-bindings gl render-args [shader vertex-binding]
          (gl/gl-draw-arrays gl GL/GL_TRIANGLE_FAN 0 (count vbuf))))

      pass/selection
      (let [vbuf (gen-vertex-buffer renderables n gen-capsule-vbuf)
            vertex-binding (vtx/use-with ::capsule vbuf shader)]
        (gl/with-gl-bindings gl render-args [shader vertex-binding]
          (gl/gl-draw-arrays gl GL/GL_TRIANGLE_FAN 0 (count vbuf)))))))


(g/defnk produce-sphere-shape-scene
  [_node-id transform diameter color]
  {:node-id _node-id
   :transform transform
   :aabb (let [d (* 0.5 diameter)]
           (-> (geom/null-aabb)
               (geom/aabb-incorporate d d d)
               (geom/aabb-incorporate (- d) (- d) (- d))
               (geom/aabb-transform transform)))
   :renderable {:render-fn render-sphere
                :user-data {:sphere-diameter diameter
                            :color color}
                :passes [pass/outline pass/transparent pass/selection]}})

(g/defnk produce-box-shape-scene
  [_node-id transform dimensions color]
  (let [[w h d] dimensions]
    {:node-id _node-id
     :transform transform
     :aabb (let [ext-x (* 0.5 w)
                 ext-y (* 0.5 h)
                 ext-z (* 0.5 d)]
             (-> (geom/null-aabb)
                 (geom/aabb-incorporate ext-x ext-y ext-z)
                 (geom/aabb-incorporate (- ext-x) (- ext-y) (- ext-z))
                 (geom/aabb-transform transform)))
     :renderable {:render-fn render-box
                  :user-data {:box-width w
                              :box-height h
                              :color color}
                  :passes [pass/outline pass/transparent pass/selection]}}))

(g/defnk produce-capsule-shape-scene
  [_node-id transform diameter height color]
  {:node-id _node-id
   :transform transform
   :aabb (let [r (* 0.5 diameter)
               ext-y (+ (* 0.5 height) r)]
           (-> (geom/null-aabb)
               (geom/aabb-incorporate r ext-y r)
               (geom/aabb-incorporate (- r) (- ext-y) (- r))
               (geom/aabb-transform transform)))
   :renderable {:render-fn render-capsule
                :user-data {:capsule-diameter diameter
                            :capsule-height height
                            :color color}
                :passes [pass/outline pass/transparent pass/selection]}})

(g/defnode SphereShape
  (inherits Shape)

  (property diameter g/Num
            (dynamic error (validation/prop-error-fnk :fatal validation/prop-zero-or-below? diameter)))

  (display-order [Shape :diameter])

  (output scene g/Any produce-sphere-shape-scene)

  (output shape-data g/Any (g/fnk [diameter]
                             [(/ diameter 2)])))

(defmethod scene-tools/manip-scalable? ::SphereShape [_node-id] true)

(defmethod scene-tools/manip-scale ::SphereShape
  [evaluation-context node-id ^Vector3d delta]
  (let [diameter (g/node-value node-id :diameter evaluation-context)]
    (g/set-property node-id :diameter (properties/round-scalar (* diameter (Math/abs (.getX delta)))))))

(defmethod scene-tools/manip-scale-manips ::SphereShape
  [node-id]
  [:scale-xy])


(g/defnode BoxShape
  (inherits Shape)

  (property dimensions types/Vec3
            (dynamic error (validation/prop-error-fnk :fatal
                                                      (fn [d _] (when (some #(<= % 0.0) d)
                                                                  "All dimensions must be greater than zero"))
                                                      dimensions))
            (dynamic edit-type (g/constantly {:type types/Vec3 :labels ["W" "H" "D"]})))

  (display-order [Shape :dimensions])

  (output scene g/Any produce-box-shape-scene)

  (output shape-data g/Any (g/fnk [dimensions]
                             (let [[w h d] dimensions]
                               [(/ w 2) (/ h 2) (/ d 2)]))))

(defmethod scene-tools/manip-scalable? ::BoxShape [_node-id] true)

(defmethod scene-tools/manip-scale ::BoxShape
  [evaluation-context node-id ^Vector3d delta]
  (let [[w h d] (g/node-value node-id :dimensions evaluation-context)]
    (g/set-property node-id :dimensions [(properties/round-scalar (Math/abs (* w (.getX delta))))
                                         (properties/round-scalar (Math/abs (* h (.getY delta))))
                                         (properties/round-scalar (Math/abs (* d (.getZ delta))))])))

(g/defnode CapsuleShape
  (inherits Shape)

  (property diameter g/Num
            (dynamic error (validation/prop-error-fnk :fatal validation/prop-zero-or-below? diameter)))
  (property height g/Num
            (dynamic error (validation/prop-error-fnk :fatal validation/prop-zero-or-below? height)))

  (display-order [Shape :diameter :height])

  (output scene g/Any produce-capsule-shape-scene)

  (output shape-data g/Any (g/fnk [diameter height]
                             [(/ diameter 2) height])))

(defmethod scene-tools/manip-scalable? ::CapsuleShape [_node-id] true)

(defmethod scene-tools/manip-scale ::CapsuleShape
  [evaluation-context node-id ^Vector3d delta]
  (let [[d h] (mapv #(g/node-value node-id % evaluation-context) [:diameter :height])]
    (g/set-property node-id
                    :diameter (properties/round-scalar (Math/abs (* d (.getX delta))))
                    :height (properties/round-scalar (Math/abs (* h (.getY delta)))))))

(defmethod scene-tools/manip-scale-manips ::CapsuleShape
  [node-id]
  [:scale-x :scale-y :scale-xy])



(defn attach-shape-node
  [parent shape-node]
  (concat
   (g/connect shape-node :_node-id           parent :nodes)
   (g/connect shape-node :node-outline       parent :child-outlines)
   (g/connect shape-node :scene              parent :child-scenes)
   (g/connect shape-node :shape              parent :shapes)
   (g/connect parent :collision-group-color  shape-node :color)))

(defmulti decode-shape-data
  (fn [shape data] (:shape-type shape)))

(defmethod decode-shape-data :type-sphere
  [shape [r]]
  {:diameter (* 2 r)})

(defmethod decode-shape-data :type-box
  [shape [ext-x ext-y ext-z]]
  {:dimensions [(* 2 ext-x) (* 2 ext-y) (* 2 ext-z)]})

(defmethod decode-shape-data :type-capsule
  [shape [r h]]
  {:diameter (* 2 r)
   :height h})

(defn make-shape-node
  [parent {:keys [shape-type] :as shape} select-fn]
  (let [graph-id (g/node-id->graph-id parent)
        node-type (case shape-type
                    :type-sphere SphereShape
                    :type-box BoxShape
                    :type-capsule CapsuleShape)
        node-props (dissoc shape :index :count)]
    (g/make-nodes
     graph-id
     [shape-node [node-type node-props]]
     (attach-shape-node parent shape-node)
     (when select-fn
       (select-fn [shape-node])))))


(defn load-collision-object
  [project self resource co]
  (concat
    (g/set-property self
      :collision-shape (workspace/resolve-resource resource (:collision-shape co))
      :type (:type co)
      :mass (:mass co)
      :friction (:friction co)
      :restitution (:restitution co)
      :group (:group co)
      :mask (some->> (:mask co) (s/join ", "))
      :linear-damping (:linear-damping co)
      :angular-damping (:angular-damping co)
      :locked-rotation (:locked-rotation co))
    (g/connect self :collision-group-node project :collision-group-nodes)
    (g/connect project :collision-groups-data self :collision-groups-data)
    (g/connect project :settings self :project-settings)
    (when-let [embedded-shape (:embedded-collision-shape co)]
      (for [{:keys [index count] :as shape} (:shapes embedded-shape)]
        (let [data (subvec (:data embedded-shape) index (+ index count))
              shape-with-decoded-data (merge shape (decode-shape-data shape data))]
          (make-shape-node self shape-with-decoded-data nil))))))

(g/defnk produce-scene
  [_node-id child-scenes]
  {:node-id _node-id
   :aabb (reduce geom/aabb-union (geom/null-aabb) (keep :aabb child-scenes))
   :renderable {:passes [pass/selection]}
   :children child-scenes})

(defn- produce-embedded-collision-shape
  [shapes]
  (when (seq shapes)
    (loop [idx 0
           [shape & rest] shapes
           ret {:shapes [] :data []}]
      (if-not shape
        ret
        (let [data (:data shape)
              data-len (count data)
              shape-msg (-> shape
                            (assoc :index idx :count data-len)
                            (dissoc :data))]
          (recur (+ idx data-len)
                 rest
                 (-> ret
                     (update :shapes conj shape-msg)
                     (update :data into data))))))))

(g/defnk produce-pb-msg
  [collision-shape-resource type mass friction restitution
   group mask angular-damping linear-damping locked-rotation
   shapes]
  {:collision-shape (resource/resource->proj-path collision-shape-resource)
   :type type
   :mass mass
   :friction friction
   :restitution restitution
   :group group
   :mask (when mask (->> (s/split mask #",") (map s/trim) (remove s/blank?)))
   :linear-damping linear-damping
   :angular-damping angular-damping
   :locked-rotation locked-rotation
   :embedded-collision-shape (produce-embedded-collision-shape shapes)})

(defn build-collision-object
  [resource dep-resources user-data]
  (let [[shape] (vals dep-resources)
        pb-msg (cond-> (:pb-msg user-data)
                 shape (assoc :collision-shape (resource/proj-path shape)))]
    {:resource resource
     :content (protobuf/map->bytes Physics$CollisionObjectDesc pb-msg)}))

(defn- merge-convex-shape [collision-shape convex-shape]
  (if convex-shape
    (let [collision-shape (or collision-shape {:shapes [] :data []})
          shape {:shape-type (:shape-type convex-shape)
                 :position [0 0 0]
                 :rotation [0 0 0 1]
                 :index (count (:data collision-shape))
                 :count (count (:data convex-shape))}]
      (-> collision-shape
        (update :shapes conj shape)
        (update :data into (:data convex-shape))))
    collision-shape))

(g/defnk produce-build-targets
  [_node-id resource pb-msg collision-shape dep-build-targets mass type project-settings shapes]
  (let [dep-build-targets (flatten dep-build-targets)
        convex-shape (when (and collision-shape (= "convexshape" (:ext (resource/resource-type collision-shape))))
                       (get-in (first dep-build-targets) [:user-data :pb]))
        pb-msg (if convex-shape
                 (dissoc pb-msg :collision-shape) ; Convex shape will be merged into :embedded-collision-shape below.
                 pb-msg)
        dep-build-targets (if convex-shape [] dep-build-targets)
        deps-by-source (into {} (map #(let [res (:resource %)] [(:resource res) res]) dep-build-targets))
        dep-resources (if convex-shape
                        [] ; Convex shape is merged into :embedded-collision-shape
                        (map (fn [[label resource]]
                               [label (get deps-by-source resource)])
                             [[:collision-shape collision-shape]])) ; This is a tilemap resource.
        pb-msg (update pb-msg :embedded-collision-shape merge-convex-shape convex-shape)]
    (g/precluding-errors
      [(validation/prop-error :fatal _node-id :collision-shape validation/prop-resource-not-exists? collision-shape "Collision Shape")
       (when (= :collision-object-type-dynamic type)
         (validation/prop-error :fatal _node-id :mass validation/prop-zero-or-below? mass "Mass"))
       (when (and (empty? (:collision-shape pb-msg))
                  (empty? (:embedded-collision-shape pb-msg)))
         (g/->error _node-id :collision-shape :fatal collision-shape "Collision Object has no shapes"))
       (let [supported-physics-type (project-physics-type project-settings)]
         (sequence (comp (map :shape-type)
                         (distinct)
                         (remove #(contains? (shape-type-physics-types %) supported-physics-type))
                         (map #(format "%s shapes are not supported in %s physics" (shape-type-label %) supported-physics-type))
                         (map #(g/->error _node-id :shapes :fatal shapes %)))
                   shapes))]
      [{:node-id _node-id
        :resource (workspace/make-build-resource resource)
        :build-fn build-collision-object
        :user-data {:pb-msg pb-msg
                    :dep-resources dep-resources}
        :deps dep-build-targets}])))

(g/defnk produce-collision-group-color
  [collision-groups-data group]
  (collision-groups/color collision-groups-data group))

(g/defnode CollisionObjectNode
  (inherits resource-node/ResourceNode)

  (input shapes g/Any :array)
  (input child-scenes g/Any :array)
  (input collision-shape-resource resource/Resource)
  (input dep-build-targets g/Any :array)
  (input collision-groups-data g/Any)
  (input project-settings g/Any)

  (property collision-shape resource/Resource
            (value (gu/passthrough collision-shape-resource))
            (set (fn [_evaluation-context self old-value new-value]
                   (project/resource-setter self old-value new-value
                                            [:resource :collision-shape-resource]
                                            [:build-targets :dep-build-targets])))
            (dynamic edit-type (g/constantly {:type resource/Resource :ext #{"convexshape" "tilemap"}}))
            (dynamic error (g/fnk [_node-id collision-shape]
                                  (when collision-shape
                                    (validation/prop-error :fatal _node-id :collision-shape validation/prop-resource-not-exists? collision-shape "Collision Shape")))))

  (property type g/Any
            (dynamic edit-type (g/constantly (properties/->pb-choicebox Physics$CollisionObjectType))))

  (property mass g/Num
            (value (g/fnk [mass type]
                     (if (= :collision-object-type-dynamic type) mass 0.0)))
            (dynamic read-only? (g/fnk [type]
                                  (not= :collision-object-type-dynamic type)))
            (dynamic error (g/fnk [_node-id mass type]
                             (when (= :collision-object-type-dynamic type)
                               (validation/prop-error :fatal _node-id :mass validation/prop-zero-or-below? mass "Mass")))))

  (property friction g/Num)
  (property restitution g/Num)
  (property linear-damping g/Num
            (default 0))
  (property angular-damping g/Num
            (default 0))
  (property locked-rotation g/Bool
            (default false))

  (property group g/Str)
  (property mask g/Str)

  (output scene g/Any :cached produce-scene)

  (output node-outline outline/OutlineData :cached (g/fnk [_node-id child-outlines]
                                                     {:node-id _node-id
                                                      :label "Collision Object"
                                                      :icon collision-object-icon
                                                      :children (outline/natural-sort child-outlines)
                                                      :child-reqs [{:node-type Shape
                                                                    :tx-attach-fn attach-shape-node}]}))

  (output pb-msg g/Any :cached produce-pb-msg)
  (output save-value g/Any (gu/passthrough pb-msg))
  (output build-targets g/Any :cached produce-build-targets)
  (output collision-group-node g/Any :cached (g/fnk [_node-id group] {:node-id _node-id :collision-group group}))
  (output collision-group-color g/Any :cached produce-collision-group-color))

(defn- sanitize-collision-object [co]
  (let [embedded-shape (:embedded-collision-shape co)]
    (cond-> co
      (empty? (:shapes embedded-shape)) (dissoc co :embedded-collision-shape))))

(defn register-resource-types [workspace]
  (resource-node/register-ddf-resource-type workspace
                                    :ext "collisionobject"
                                    :node-type CollisionObjectNode
                                    :ddf-type Physics$CollisionObjectDesc
                                    :load-fn load-collision-object
                                    :sanitize-fn sanitize-collision-object
                                    :icon collision-object-icon
                                    :view-types [:scene :text]
                                    :view-opts {:scene {:grid true}}
                                    :tags #{:component}
                                    :tag-opts {:component {:transform-properties #{}}}
                                    :label "Collision Object"))

;; outline context menu

(defn- default-shape
  [shape-type]
  (merge (protobuf/pb->map (Physics$CollisionShape$Shape/getDefaultInstance))
         {:shape-type shape-type}
         (case shape-type
           :type-sphere {:diameter 20.0}
           :type-box {:dimensions [20.0 20.0 20.0]}
           :type-capsule {:diameter 20.0 :height 40.0})))

(defn- add-shape-handler
  [collision-object-node shape-type select-fn]
  (g/transact
   (concat
    (g/operation-label "Add Shape")
    (make-shape-node collision-object-node (default-shape shape-type) select-fn))))

(defn- selection->collision-object [selection]
  (handler/adapt-single selection CollisionObjectNode))

(handler/defhandler :add :workbench
  (label [user-data]
         (if-not user-data
           "Add Shape"
           (shape-type-label (:shape-type user-data))))
  (active? [selection] (selection->collision-object selection))
  (run [selection user-data app-view]
    (add-shape-handler (selection->collision-object selection) (:shape-type user-data) (fn [node-ids] (app-view/select app-view node-ids))))
  (options [selection user-data]
           (let [self (selection->collision-object selection)]
             (when-not user-data
               (->> shape-type-ui
                    (reduce-kv (fn [res shape-type {:keys [label icon]}]
                                 (conj res {:label label
                                            :icon icon
                                            :command :add
                                            :user-data {:_node-id self :shape-type shape-type}}))
                               [])
                    (sort-by :label)
                    (into []))))))
