(ns editor.collision-object
  (:require
   [clojure.string :as s]
   [plumbing.core :as pc]
   [dynamo.graph :as g]
   [editor.colors :as colors]
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
   [editor.types :as types]
   [editor.validation :as validation]
   [editor.workspace :as workspace]
   [editor.scene :as scene]
   [editor.scene-tools :as scene-tools])
  (:import
   [com.dynamo.physics.proto Physics$CollisionObjectDesc
    Physics$CollisionObjectType
    Physics$CollisionShape
    Physics$CollisionShape$Shape
    Physics$CollisionShape$Type]
   [javax.media.opengl GL GL2]
   [javax.vecmath Point3d Matrix4d Quat4d Vector3d]
   [editor.types AABB]))

(set! *warn-on-reflection* true)

(defmacro validate-greater-than-zero [field message]
  `(g/fnk [~field]
     (when (<= ~field 0.0)
       (g/error-severe ~message))))

(defn- v4->euler [v]
  (math/quat->euler (doto (Quat4d.) (math/clj->vecmath v))))

(def collision-object-icon "icons/32/Icons_49-Collision-object.png")

(def shape-type-ui
  {:type-sphere  {:label "Sphere"
                  :icon  "icons/32/Icons_45-Collistionshape-convex-Sphere.png"}
   :type-box     {:label "Box"
                  :icon  "icons/32/Icons_44-Collistionshape-convex-Box.png"}
   :type-capsule {:label "Capsule"
                  :icon  "icons/32/Icons_46-Collistionshape-convex-Cylinder.png"}})

(defn- shape-type-label
  [shape-type]
  (get-in shape-type-ui [shape-type :label]))

(defn- shape-type-icon
  [shape-type]
  (get-in shape-type-ui [shape-type :icon]))

(g/defnode Shape
  (inherits outline/OutlineNode)
  (inherits scene/SceneNode)

  (input color g/Any)
  
  (property shape-type g/Any
            (dynamic visible (g/always false)))

  (output shape-data g/Any :abstract)
  (output scene g/Any :abstract)

  (output shape g/Any (g/fnk [shape-type position rotation-q4 shape-data]
                        {:shape-type shape-type
                         :position position
                         :rotation (math/vecmath->clj rotation-q4)
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
      (let [vbuf (gen-vertex-buffer renderables n  gen-disc-vertex-buffer)
            vbuf-binding (vtx/use-with ::box vbuf shader)]
        (gl/with-gl-bindings gl render-args [shader vbuf-binding]
          (gl/gl-draw-arrays gl GL/GL_TRIANGLE_FAN 0 (count vbuf))))

      pass/selection
      (let [vbuf (gen-vertex-buffer renderables n  gen-disc-vertex-buffer)
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
   :aabb (-> (geom/null-aabb)
             (geom/aabb-incorporate diameter diameter diameter)
             (geom/aabb-incorporate (- diameter) (- diameter) (- diameter)))
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
                 (geom/aabb-incorporate (- ext-x) (- ext-y) (- ext-z))))
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
               (geom/aabb-incorporate (- r) (- ext-y) (- r))))
   :renderable {:render-fn render-capsule
                :user-data {:capsule-diameter diameter
                            :capsule-height height
                            :color color}
                :passes [pass/outline pass/transparent pass/selection]}})

(g/defnode SphereShape
  (inherits Shape)

  (property diameter g/Num
            (validate (validate-greater-than-zero diameter "Diameter must be greater than zero")))

  (display-order [Shape :diameter])

  (output scene g/Any produce-sphere-shape-scene)

  (output shape-data g/Any (g/fnk [diameter]
                             [(/ diameter 2)])))

(defmethod scene-tools/manip-scale ::SphereShape
  [basis node-id ^Vector3d delta]
  (let [diameter (g/node-value node-id :diameter {:basis basis})]
    (g/set-property node-id :diameter (* diameter (Math/abs (.getX delta))))))

(defmethod scene-tools/manip-scale-manips ::SphereShape
  [node-id]
  [:scale-xy])


(g/defnode BoxShape
  (inherits Shape)

  (property dimensions types/Vec3
            (validate (g/fnk [dimensions]
                        (when (some #(<= % 0.0) dimensions)
                          (g/error-severe "All dimensions must be greater than zero"))))
            (dynamic edit-type (g/always {:type types/Vec3 :labels ["W" "H" "D"]})))

  (display-order [Shape :dimensions])

  (output scene g/Any produce-box-shape-scene)

  (output shape-data g/Any (g/fnk [dimensions]
                             (let [[w h d] dimensions]
                               [(/ w 2) (/ h 2) (/ d 2)]))))

(defmethod scene-tools/manip-scale ::BoxShape
  [basis node-id ^Vector3d delta]
  (let [[w h d] (g/node-value node-id :dimensions {:basis basis})]
    (g/set-property node-id :dimensions [(Math/abs (* w (.getX delta)))
                                         (Math/abs (* h (.getY delta)))
                                         (Math/abs (* d (.getZ delta)))])))

(g/defnode CapsuleShape
  (inherits Shape)

  (property diameter g/Num
            (validate (validate-greater-than-zero diameter "Diameter must be greater than zero")))
  (property height g/Num
            (validate (validate-greater-than-zero height "Height must be greater than zero")))

  (display-order [Shape :diameter :height])

  (output scene g/Any produce-capsule-shape-scene)

  (output shape-data g/Any (g/fnk [diameter height]
                             [(/ diameter 2) height])))

(defmethod scene-tools/manip-scale ::CapsuleShape
  [basis node-id ^Vector3d delta]
  (let [[d h] (mapv #(g/node-value node-id % {:basis basis}) [:diameter :height])]
    (g/set-property node-id
                    :diameter (Math/abs (* d (.getX delta)))
                    :height (Math/abs (* h (.getY delta))))))

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
  [parent {:keys [shape-type] :as shape}]
  (let [graph-id (g/node-id->graph-id parent)
        node-type (case shape-type
                    :type-sphere SphereShape
                    :type-box BoxShape
                    :type-capsule CapsuleShape)
        node-props (-> shape
                       (update :rotation v4->euler)
                       (dissoc :index :count))]
    (g/make-nodes
     graph-id
     [shape-node [node-type node-props]]
     (attach-shape-node parent shape-node))))


(defn load-collision-object
  [project self resource]
  (let [co (protobuf/read-text Physics$CollisionObjectDesc resource)]
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
     (when-let [embedded-shape (:embedded-collision-shape co)]
       (for [{:keys [index count] :as shape} (:shapes embedded-shape)]
         (let [data (subvec (:data embedded-shape) index (+ index count))
               shape-with-decoded-data (merge shape (decode-shape-data shape data))]
           (make-shape-node self shape-with-decoded-data)))))))

(g/defnk produce-scene
  [_node-id child-scenes]
  {:node-id _node-id
   :aabb (reduce geom/aabb-union (geom/null-aabb) (keep :aabb child-scenes))
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

(g/defnk produce-save-data [resource pb-msg]
  {:resource resource
   :content (protobuf/map->str Physics$CollisionObjectDesc pb-msg)})

(defn build-collision-object
  [self basis resource dep-resources user-data]
  {:resource resource
   :content (protobuf/map->bytes Physics$CollisionObjectDesc (:pb-msg user-data))})

(g/defnk produce-build-targets
  [_node-id resource pb-msg collision-shape dep-build-targets]
  (let [dep-build-targets (flatten dep-build-targets)
        deps-by-source (into {} (map #(let [res (:resource %)] [(:resource res) res]) dep-build-targets))
        dep-resources (map (fn [[label resource]]
                             [label (get deps-by-source resource)])
                           [[:collision-shape collision-shape]])]
    [{:node-id _node-id
      :resource (workspace/make-build-resource resource)
      :build-fn build-collision-object
      :user-data {:pb-msg pb-msg
                  :dep-resources dep-resources}
      :deps dep-build-targets}]))

(g/defnk produce-collision-group-color
  [collision-groups-data group]
  (collision-groups/color collision-groups-data group))

(g/defnode CollisionObjectNode
  (inherits project/ResourceNode)

  (input shapes g/Any :array)
  (input child-scenes g/Any :array)
  (input collision-shape-resource resource/Resource)
  (input dep-build-targets g/Any :array)
  (input collision-groups-data g/Any)

  (property collision-shape resource/Resource
            (value (gu/passthrough collision-shape-resource))
            (set (fn [basis self old-value new-value]
                   (project/resource-setter basis self old-value new-value
                                            [:resource :collision-shape-resource]
                                            [:build-targets :dep-build-targets])))
            (dynamic edit-type (g/always {:type resource/Resource :ext "tilemap"})))

  (property type g/Any
            (dynamic edit-type (g/always (properties/->pb-choicebox Physics$CollisionObjectType))))

  (property mass g/Num
            (value (g/fnk [mass type]
                     (if (= :collision-object-type-dynamic type) mass 0)))
            (dynamic read-only? (g/fnk [type]
                                  (not= :collision-object-type-dynamic type)))
            (validate (g/fnk [mass type]
                        (when (and (= :collision-object-type-dynamic type) (< mass 1))
                          (g/error-severe "Must be greater than zero")))))

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
                                                      :children child-outlines}))

  (output pb-msg g/Any :cached produce-pb-msg)
  (output save-data g/Any :cached produce-save-data)
  (output build-targets g/Any :cached produce-build-targets)
  (output collision-group-node g/Any :cached (g/fnk [_node-id group] {:node-id _node-id :collision-group group}))
  (output collision-group-color g/Any :cached produce-collision-group-color))

(defn register-resource-types [workspace]
  (workspace/register-resource-type workspace
                                    :ext "collisionobject"
                                    :node-type CollisionObjectNode
                                    :load-fn load-collision-object
                                    :icon collision-object-icon
                                    :view-types [:scene :text]
                                    :view-opts {:scene {:grid true}}
                                    :tags #{:component}
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
  [collision-object-node shape-type]
  (g/transact
   (concat
    (g/operation-label "Add Shape")
    (make-shape-node collision-object-node (default-shape shape-type)))))

(handler/defhandler :add :global
  (label [user-data]
         (if-not user-data
           "Add Shape"
           (shape-type-label (:shape-type user-data))))
  (active? [selection] (some->> (first selection) (g/node-instance? CollisionObjectNode)))
  (run [selection user-data] (add-shape-handler (first selection) (:shape-type user-data)))
  (options [selection user-data]
           (let [self (first selection)]
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

nil
