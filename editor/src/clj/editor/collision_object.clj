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

(ns editor.collision-object
  (:require [clojure.string :as string]
            [dynamo.graph :as g]
            [editor.app-view :as app-view]
            [editor.attachment :as attachment]
            [editor.build-target :as bt]
            [editor.collision-groups :as collision-groups]
            [editor.defold-project :as project]
            [editor.editor-extensions.graph :as ext-graph]
            [editor.editor-extensions.node-types :as node-types]
            [editor.geom :as geom]
            [editor.gl.pass :as pass]
            [editor.gl.vertex2 :as vtx]
            [editor.graph-util :as gu]
            [editor.graphics.types :as graphics.types]
            [editor.handler :as handler]
            [editor.localization :as localization]
            [editor.math :as math]
            [editor.model-loader :as model-loader]
            [editor.outline :as outline]
            [editor.properties :as properties]
            [editor.protobuf :as protobuf]
            [editor.resource :as resource]
            [editor.resource-node :as resource-node]
            [editor.scene :as scene]
            [editor.scene-shapes :as scene-shapes]
            [editor.scene-tools :as scene-tools]
            [editor.types :as types]
            [editor.validation :as validation]
            [editor.workspace :as workspace]
            [schema.core :as s]
            [util.array :as array]
            [util.coll :as coll :refer [pair]]
            [util.murmur :as murmur])
  (:import [com.dynamo.bob.pipeline CollisionMeshCompiler]
           [com.dynamo.gamesys.proto Physics$CollisionObjectDesc Physics$CollisionObjectType Physics$CollisionShape$Shape]
           [com.dynamo.rig.proto Rig$MeshSet]
           [com.jogamp.opengl GL2]
           [javax.vecmath Matrix4d Point3d Quat4d Vector3d]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(def collision-object-icon "icons/32/Icons_49-Collision-object.png")
(def ^:private collision-shape-message (properties/label-message :collision-object :collision-shape))
(def ^:private diameter-message (properties/label-message :collision-object.shape :diameter))
(def ^:private height-message (properties/label-message :collision-object.shape :height))
(def ^:private mass-message (properties/label-message :collision-object :mass))
(def ^:private mesh-scene-message (properties/label-message :collision-object.shape :mesh-scene))

(g/deftype ^:private NameCounts {s/Str s/Int})

(def shape-type-ui
  {:type-sphere  {:label "Sphere"
                  :message (localization/message "command.edit.add-embedded-component.variant.collision-object.option.sphere")
                  :icon  "icons/32/Icons_45-Collistionshape-convex-Sphere.png"
                  :physics-types #{"2D" "3D"}}
   :type-box     {:label "Box"
                  :message (localization/message "command.edit.add-embedded-component.variant.collision-object.option.box")
                  :icon  "icons/32/Icons_44-Collistionshape-convex-Box.png"
                  :physics-types #{"2D" "3D"}}
   :type-capsule {:label "Capsule"
                  :message (localization/message "command.edit.add-embedded-component.variant.collision-object.option.capsule")
                  :icon  "icons/32/Icons_46-Collistionshape-convex-Cylinder.png"
                  :physics-types #{"3D"}}
   :type-hull    {:label "Hull"
                  :message (localization/message "command.edit.add-embedded-component.variant.collision-object.option.hull")
                  :icon "icons/32/Icons_51-Collision-shape-convex.png"
                  :physics-types #{"3D"}}
   :type-mesh    {:label "Mesh"
                  :message (localization/message "command.edit.add-embedded-component.variant.collision-object.option.mesh")
                  :icon "icons/32/Icons_27-AT-Mesh.png"
                  :physics-types #{"3D"}}})

(defn- mesh-source-shape-type?
  [shape-type]
  (contains? #{:type-hull :type-mesh} shape-type))

(defn- shape-type-label
  [shape-type]
  (get-in shape-type-ui [shape-type :label]))

(defn- shape-type-message
  [shape-type]
  (get-in shape-type-ui [shape-type :message]))

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

(g/deftype PhysicsType (s/enum "2D" "3D"))

(defn- unique-id-error [node-id id id-counts]
  (validation/prop-error :fatal node-id :id (partial validation/prop-id-duplicate? id-counts) id))

(defn- validate-image-id [node-id id id-counts]
  (when (and (not-empty id) (some? id-counts))
    (unique-id-error node-id id id-counts)))

(defn- collision-mesh-choicebox [collision-meshes]
  (let [collision-meshes (model-loader/named-meshes collision-meshes)
        name-counts (frequencies (into [] (map :name) collision-meshes))]
    {:type :choicebox
     :options (into [[-1 ""]]
                    (map (fn [{:keys [index name]}]
                           [index (if (= 1 (get name-counts name))
                                    name
                                    (localization/message "property.mesh-selection.option.raw-index"
                                                          {"mesh" name
                                                           "index" index}))]))
                    collision-meshes)}))

(defn- set-mesh-index [evaluation-context self _old-value new-value]
  (when (properties/user-edit? self :mesh-index evaluation-context)
    (let [collision-meshes (g/node-value self :collision-meshes evaluation-context)
          selected-mesh (when-not (g/error-value? collision-meshes)
                          (coll/first-where #(= new-value (:index %))
                                            (model-loader/named-meshes collision-meshes)))]
      (g/set-property self :mesh-name (or (:name selected-mesh) "")))))

(defn- set-mesh-scene [evaluation-context self old-value new-value]
  (into (project/resource-setter evaluation-context self old-value new-value
                                 [:resource :mesh-scene-resource]
                                 [:collision-mesh-renderables :collision-mesh-renderables]
                                 [:collision-mesh-set :collision-mesh-set]
                                 [:collision-meshes :collision-meshes])
        (when (properties/user-edit? self :mesh-scene evaluation-context)
          (g/set-properties self :mesh-name "" :mesh-index -1))))

(g/defnk produce-shape
  [shape-type position rotation id shape-data mesh-scene mesh-name mesh-index]
  (cond-> (-> (protobuf/make-map-without-defaults Physics$CollisionShape$Shape
                :shape-type shape-type
                :position position
                :rotation rotation
                :id id)
              (assoc :data shape-data))
          (mesh-source-shape-type? shape-type)
          (assoc :mesh-scene (resource/resource->proj-path mesh-scene)
                 :mesh-name mesh-name)

          (and (mesh-source-shape-type? shape-type) (not= -1 mesh-index))
          (assoc :mesh-index mesh-index)))

(g/defnode Shape
  (inherits outline/OutlineNode)
  (inherits scene/SceneNode)

  (input color g/Any)
  (input collision-mesh-renderables g/Any)
  (input collision-mesh-set g/Any)
  (input collision-meshes g/Any)
  (input project-physics-type PhysicsType)
  (input id-counts NameCounts)
  (input mesh-scene-resource resource/Resource)

  (property shape-type g/Any ; Required protobuf field.
            (dynamic visible (g/constantly false)))
  (property node-outline-key g/Str ; No protobuf counterpart.
            (dynamic visible (g/constantly false)))
  (property id g/Str (default (protobuf/default Physics$CollisionShape$Shape :id))
            (dynamic tooltip (properties/tooltip-dynamic :collision-object.shape :id))
            (dynamic error (g/fnk [_node-id id id-counts] (validate-image-id _node-id id id-counts))))
  (property mesh-scene resource/Resource
            (value (gu/passthrough mesh-scene-resource))
            (set set-mesh-scene)
            (dynamic visible (g/fnk [shape-type] (mesh-source-shape-type? shape-type)))
            (dynamic edit-type (g/constantly {:type resource/Resource :ext #{"glb" "gltf"}}))
            (dynamic error (g/fnk [_node-id mesh-scene shape-type]
                             (when (mesh-source-shape-type? shape-type)
                               (validation/prop-error :fatal _node-id :mesh-scene validation/prop-resource-not-exists? mesh-scene mesh-scene-message))))
            (dynamic label (properties/label-dynamic :collision-object.shape :mesh-scene))
            (dynamic tooltip (properties/tooltip-dynamic :collision-object.shape :mesh-scene)))
  (property mesh-name g/Str
            (default "")
            (dynamic visible (g/constantly false)))
  (property mesh-index g/Int
            (default -1)
            (value (g/fnk [^:try collision-meshes mesh-index mesh-name]
                     (if (g/error-value? collision-meshes)
                       mesh-index
                       (or (:index (model-loader/resolve-named-mesh collision-meshes mesh-name mesh-index))
                           mesh-index))))
            (set set-mesh-index)
            (dynamic visible (g/fnk [shape-type] (mesh-source-shape-type? shape-type)))
            (dynamic read-only? (g/fnk [mesh-scene ^:try collision-meshes]
                                  (or (nil? mesh-scene)
                                      (g/error-value? collision-meshes))))
            (dynamic edit-type (g/fnk [^:try collision-meshes]
                                 (if (g/error-value? collision-meshes)
                                   (collision-mesh-choicebox [])
                                   (collision-mesh-choicebox collision-meshes))))
            (dynamic label (properties/label-dynamic :collision-object.shape :mesh))
            (dynamic tooltip (properties/tooltip-dynamic :collision-object.shape :mesh)))

  (display-order [:shape-type :node-outline-key :id scene/SceneNode :mesh-scene :mesh-name :mesh-index])

  (output transform-properties g/Any scene/produce-unscalable-transform-properties)
  (output shape-data g/Any :abstract)
  (output scene g/Any :abstract)

  (output shape g/Any produce-shape)
  (output selected-collision-mesh g/Any
          (g/fnk [^:try collision-meshes mesh-name mesh-index]
            (when-not (g/error-value? collision-meshes)
              (model-loader/resolve-named-mesh collision-meshes mesh-name mesh-index))))
  (output selected-collision-mesh-renderable g/Any
          (g/fnk [selected-collision-mesh ^:try collision-mesh-renderables]
            (when (and selected-collision-mesh
                       (not (g/error-value? collision-mesh-renderables)))
              (coll/first-where #(= (:index selected-collision-mesh) (:index %))
                                collision-mesh-renderables))))
  (output collision-mesh-set-info g/Any
          (g/fnk [shape-type mesh-scene collision-mesh-set]
            (when (and (mesh-source-shape-type? shape-type)
                       mesh-scene
                       collision-mesh-set)
              {:mesh-scene (resource/resource->proj-path mesh-scene)
               :mesh-set collision-mesh-set})))

  (output node-outline outline/OutlineData :cached (g/fnk [_node-id shape-type id node-outline-key]
                                                     {:node-id _node-id
                                                      :node-outline-key node-outline-key
                                                      :label (if (empty? id)
                                                               (localization/message "outline.unnamed-collision-shape" {"shape" (shape-type-message shape-type)})
                                                               id)
                                                      :icon (shape-type-icon shape-type)})))

(defn unify-scale [renderable]
  (let [{:keys [^Quat4d world-rotation
                ^Vector3d world-scale
                ^Vector3d world-translation]} renderable
        min-scale (min (.-x world-scale) (.-y world-scale) (.-z world-scale))
        physics-world-transform (doto (Matrix4d.)
                                  (.setIdentity)
                                  (.setScale min-scale)
                                  (.setTranslation world-translation)
                                  (.setRotation world-rotation))]
    (assoc renderable :world-transform physics-world-transform)))

(defn wrap-uniform-scale [render-fn]
  (fn [gl render-args renderables n]
    (render-fn gl render-args (map unify-scale renderables) n)))

;; We cannot batch-render the collision shapes, since we need to cancel out non-
;; uniform scaling on the individual shape transforms. Thus, we are free to use
;; shared object-space vertex buffers and transform them in the vertex shader.

(def ^:private render-lines-uniform-scale (wrap-uniform-scale scene-shapes/render-lines))

(def ^:private render-triangles-uniform-scale (wrap-uniform-scale scene-shapes/render-triangles))

(def ^:private render-points-uniform-scale (wrap-uniform-scale scene-shapes/render-points))

(defn- preview-sphere-shape-renderable
  [visibility-aabb user-data prop-kw->override-value]
  (let [^Point3d ext-override
        (when-some [diameter-override (:diameter prop-kw->override-value)]
          (let [radius (* 0.5 (double diameter-override))
                ext-z (if (:is-2d user-data) 0.0 radius)]
            (Point3d. radius radius ext-z)))

        point-scale-override
        (when ext-override
          (array/of-floats (.x ext-override) (.y ext-override) (.z ext-override)))

        visibility-aabb
        (if ext-override
          (geom/mirrored-point->aabb ext-override)
          visibility-aabb)

        user-data
        (cond-> user-data
                point-scale-override (assoc :point-scale point-scale-override))]

    (pair visibility-aabb user-data)))

(g/defnk produce-sphere-shape-scene
  [_node-id pose diameter color node-outline-key project-physics-type]
  (let [is-2d (= "2D" project-physics-type)

        ;; The preview-fn technically takes and returns a visibility-aabb, but
        ;; we can also use it for our local-aabb.
        [local-aabb user-data]
        (preview-sphere-shape-renderable
          geom/null-aabb
          {:is-2d is-2d}
          {:diameter diameter})]

    {:node-id _node-id
     :node-outline-key node-outline-key
     :pose pose
     :aabb local-aabb
     :renderable {:render-fn render-triangles-uniform-scale
                  :preview-fn preview-sphere-shape-renderable
                  :tags #{:collision-shape :gizmo}
                  :passes [pass/transparent pass/selection]
                  :user-data (assoc user-data
                               :color color
                               :double-sided is-2d
                               :geometry (if is-2d
                                           scene-shapes/disc-triangles
                                           scene-shapes/capsule-triangles))}
     :children [{:node-id _node-id
                 :aabb local-aabb
                 :renderable {:render-fn render-lines-uniform-scale
                              :preview-fn preview-sphere-shape-renderable
                              :tags #{:collision-shape :gizmo :outline}
                              :passes [pass/outline]
                              :user-data (assoc user-data
                                           :color color
                                           :geometry (if is-2d
                                                       scene-shapes/disc-lines
                                                       scene-shapes/capsule-lines))}}]}))


(defn- preview-box-shape-renderable
  [visibility-aabb user-data prop-kw->override-value]
  (let [^Point3d ext-override
        (when-some [dimensions-override (:dimensions prop-kw->override-value)]
          (let [[^double w ^double h ^double d] dimensions-override
                ext-x (* 0.5 w)
                ext-y (* 0.5 h)
                ext-z (if (:is-2d user-data) 0.0 (* 0.5 d))]
            (Point3d. ext-x ext-y ext-z)))

        point-scale-override
        (when ext-override
          (array/of-floats (.x ext-override) (.y ext-override) (.z ext-override)))

        visibility-aabb
        (if ext-override
          (geom/mirrored-point->aabb ext-override)
          visibility-aabb)

        user-data
        (cond-> user-data
                point-scale-override (assoc :point-scale point-scale-override))]

    (pair visibility-aabb user-data)))

(g/defnk produce-box-shape-scene
  [_node-id pose dimensions color node-outline-key project-physics-type]
  (let [is-2d (= "2D" project-physics-type)

        ;; The preview-fn technically takes and returns a visibility-aabb, but
        ;; we can also use it for our local-aabb.
        [local-aabb user-data]
        (preview-box-shape-renderable
          geom/null-aabb
          {:is-2d is-2d}
          {:dimensions dimensions})]

    {:node-id _node-id
     :node-outline-key node-outline-key
     :pose pose
     :aabb local-aabb
     :renderable {:render-fn render-triangles-uniform-scale
                  :preview-fn preview-box-shape-renderable
                  :tags #{:collision-shape :gizmo}
                  :passes [pass/transparent pass/selection]
                  :user-data (cond-> (assoc user-data
                                       :color color
                                       :geometry scene-shapes/box-triangles)

                                     is-2d
                                     (assoc :double-sided true
                                            :point-count 6))}
     :children [{:node-id _node-id
                 :aabb local-aabb
                 :renderable {:render-fn render-lines-uniform-scale
                              :preview-fn preview-box-shape-renderable
                              :tags #{:collision-shape :gizmo :outline}
                              :passes [pass/outline]
                              :user-data (cond-> (assoc user-data
                                                   :color color
                                                   :geometry scene-shapes/box-lines)

                                                 is-2d
                                                 (assoc :point-count 8))}}]}))

(defn- preview-capsule-shape-renderable
  [visibility-aabb user-data prop-kw->override-value]
  (let [radius-override
        (when-some [diameter-override (:diameter prop-kw->override-value)]
          (* 0.5 (double diameter-override)))

        half-height-override
        (when-some [height-override (:height prop-kw->override-value)]
          (* 0.5 (double height-override)))

        ^Point3d ext-override
        (when (or radius-override half-height-override)
          (let [^double radius (or radius-override (first (:point-scale user-data)))
                ^double half-height (or half-height-override (second (:point-offset-by-w user-data)))
                ext-y (+ half-height radius)
                ext-z (if (:is-2d user-data) 0.0 radius)]
            (Point3d. radius ext-y ext-z)))

        point-scale-override
        (when ext-override
          ;; Note: X is repeated for Y in the point-scale.
          (array/of-floats (.x ext-override) (.x ext-override) (.z ext-override)))

        point-offset-by-w-override
        (when half-height-override
          (array/of-floats 0.0 half-height-override 0.0))

        visibility-aabb
        (if ext-override
          (geom/mirrored-point->aabb ext-override)
          visibility-aabb)

        user-data
        (cond-> user-data
                point-scale-override (assoc :point-scale point-scale-override)
                point-offset-by-w-override (assoc :point-offset-by-w point-offset-by-w-override))]

    (pair visibility-aabb user-data)))

(g/defnk produce-capsule-shape-scene
  [_node-id pose diameter height color node-outline-key project-physics-type]
  ;; NOTE: Capsules are currently only supported when physics type is 3D.
  (let [is-2d (= "2D" project-physics-type)

        ;; The preview-fn technically takes and returns a visibility-aabb, but
        ;; we can also use it for our local-aabb.
        [local-aabb user-data]
        (preview-capsule-shape-renderable
          geom/null-aabb
          {:is-2d is-2d}
          {:diameter diameter
           :height height})]

    {:node-id _node-id
     :node-outline-key node-outline-key
     :pose pose
     :aabb local-aabb
     :renderable {:render-fn render-triangles-uniform-scale
                  :preview-fn preview-capsule-shape-renderable
                  :tags #{:collision-shape :gizmo}
                  :passes [pass/transparent pass/selection]
                  :user-data (assoc user-data
                               :color color
                               :geometry scene-shapes/capsule-triangles)}
     :children [{:node-id _node-id
                 :aabb local-aabb
                 :renderable {:render-fn render-lines-uniform-scale
                              :preview-fn preview-capsule-shape-renderable
                              :tags #{:collision-shape :gizmo :outline}
                              :passes [pass/outline]
                              :user-data (assoc user-data
                                           :color color
                                           :geometry scene-shapes/capsule-lines)}}]}))

(g/defnode SphereShape
  (inherits Shape)

  (property diameter g/Num ; Always assigned in load-fn.
            (default 0.0) ; Used to prevent validation errors during node initialization from editor scripts
            (dynamic label (properties/label-dynamic :collision-object.shape :diameter))
            (dynamic tooltip (properties/tooltip-dynamic :collision-object.shape :diameter))
            (dynamic edit-type (g/constantly {:type g/Num :min 0.0}))

            (dynamic error (g/fnk [_node-id diameter]
                             (validation/prop-error :fatal _node-id :diameter validation/prop-zero-or-below? diameter diameter-message))))

  (display-order [Shape :diameter])

  (output scene g/Any produce-sphere-shape-scene)

  (output shape-errors g/Any (g/fnk [_node-id id id-counts diameter]
                               (g/package-errors _node-id
                                 (validate-image-id _node-id id id-counts)
                                 (validation/prop-error :fatal _node-id :diameter validation/prop-zero-or-below? diameter diameter-message))))
  (output shape-data g/Any (g/fnk [diameter]
                             [(/ (double diameter) 2.0)])))

(defmethod scene-tools/manip-scalable? ::SphereShape [_node-id] true)

(defmethod scene-tools/manip-scale ::SphereShape [node-id ^Vector3d delta manip-phase initial-evaluation-context]
  (let [old-diameter (g/node-value node-id :diameter initial-evaluation-context)
        new-diameter (properties/scale-by-absolute-value-and-round old-diameter (.getX delta))]
    (case manip-phase
      :manip-phase/commit
      {:manip/tx-data (g/set-property node-id :diameter new-diameter)}

      :manip-phase/preview
      {:manip/prop-kw->override-value {:diameter new-diameter}})))

(defmethod scene-tools/manip-scale-manips ::SphereShape [_node-id]
  [:scale-uniform])

;; NOTE: Use a custom, more specific error message to express that we're dealing with multiple properties
(defn- prop-error-box-dimensions [node-id dimensions]
  (validation/prop-error :fatal node-id :dimensions
                         (fn [dimensions]
                           (when (some #(<= ^double % 0.0) dimensions)
                             (localization/message "error.collision-object-shape-dimensions-must-be-greater-than-zero")))
                         dimensions))

(g/defnode BoxShape
  (inherits Shape)

  (property dimensions types/Vec3 ; Always assigned in load-fn.
            (default [0.0 0.0 0.0]) ; Used to prevent validation errors during node initialization from editor scripts
            (dynamic label (properties/label-dynamic :collision-object.shape :dimensions))
            (dynamic tooltip (properties/tooltip-dynamic :collision-object.shape :dimensions))
            (dynamic error (g/fnk [_node-id dimensions]
                             (prop-error-box-dimensions _node-id dimensions)))
            (dynamic edit-type (g/constantly {:type types/Vec3
                                              :labels ["W" "H" "D"]
                                              :min 0.0})))

  (display-order [Shape :dimensions])

  (output scene g/Any produce-box-shape-scene)
  (output shape-errors g/Any (g/fnk [_node-id id id-counts dimensions]
                               (g/package-errors _node-id
                                 (validate-image-id _node-id id id-counts)
                                 (prop-error-box-dimensions _node-id dimensions))))
  (output shape-data g/Any (g/fnk [dimensions]
                             (let [[^double w ^double h ^double d] dimensions]
                               [(/ w 2.0) (/ h 2.0) (/ d 2.0)]))))

(defmethod scene-tools/manip-scalable? ::BoxShape [_node-id] true)

(defmethod scene-tools/manip-scale ::BoxShape [node-id ^Vector3d delta manip-phase initial-evaluation-context]
  (let [old-dimensions (g/node-value node-id :dimensions initial-evaluation-context)
        new-dimensions (math/zip-clj-v3 old-dimensions delta properties/scale-by-absolute-value-and-round)]
    (case manip-phase
      :manip-phase/commit
      {:manip/tx-data (g/set-property node-id :dimensions new-dimensions)}

      :manip-phase/preview
      {:manip/prop-kw->override-value {:dimensions new-dimensions}})))

(g/defnode CapsuleShape
  (inherits Shape)

  (property diameter g/Num ; Always assigned in load-fn.
            (default 0.0) ; Used to prevent validation errors during node initialization from editor scripts
            (dynamic label (properties/label-dynamic :collision-object.shape :diameter))
            (dynamic tooltip (properties/tooltip-dynamic :collision-object.shape :diameter))
            (dynamic edit-type (g/constantly {:type g/Num :min 0.0}))
            (dynamic error (g/fnk [_node-id diameter]
                             (validation/prop-error :fatal _node-id :diameter validation/prop-zero-or-below? diameter diameter-message))))
  (property height g/Num ; Always assigned in load-fn.
            (default 0.0) ; Used to prevent validation errors during node initialization from editor scripts
            (dynamic label (properties/label-dynamic :collision-object.shape :height))
            (dynamic tooltip (properties/tooltip-dynamic :collision-object.shape :height))
            (dynamic edit-type (g/constantly {:type g/Num :min 0.0}))
            (dynamic error (g/fnk [_node-id height]
                             (validation/prop-error :fatal _node-id :height validation/prop-negative? height height-message))))

  (display-order [Shape :diameter :height])

  (output scene g/Any produce-capsule-shape-scene)
  (output shape-errors g/Any (g/fnk [_node-id id id-counts diameter height]
                               (g/package-errors _node-id
                                 (validate-image-id _node-id id id-counts)
                                 (validation/prop-error :fatal _node-id :diameter validation/prop-zero-or-below? diameter diameter-message)
                                 (validation/prop-error :fatal _node-id :height validation/prop-negative? height height-message))))
  (output shape-data g/Any (g/fnk [diameter height]
                             [(/ (double diameter) 2) (double height)])))

(defmethod scene-tools/manip-scalable? ::CapsuleShape [_node-id] true)

(defmethod scene-tools/manip-scale ::CapsuleShape [node-id ^Vector3d delta manip-phase initial-evaluation-context]
  (let [old-diameter (g/node-value node-id :diameter initial-evaluation-context)
        old-height (g/node-value node-id :height initial-evaluation-context)
        new-diameter (properties/scale-by-absolute-value-and-round old-diameter (.getX delta))
        new-height (properties/scale-by-absolute-value-and-round old-height (.getY delta))]
    (case manip-phase
      :manip-phase/commit
      {:manip/tx-data (g/set-properties node-id :diameter new-diameter :height new-height)}

      :manip-phase/preview
      {:manip/prop-kw->override-value {:diameter new-diameter :height new-height}})))

(defmethod scene-tools/manip-scale-manips ::CapsuleShape [_node-id]
  [:scale-x :scale-y :scale-xy :scale-uniform])

(defn- collision-mesh-primitive-scene
  [node-id color hull {:keys [aabb index-buffer position-buffer]}]
  (when (or hull index-buffer)
    {:node-id node-id
     :aabb aabb
     :renderable {:render-fn (if hull
                               render-points-uniform-scale
                               render-triangles-uniform-scale)
                  :tags (if hull
                          #{:collision-shape :gizmo :outline}
                          #{:collision-shape :gizmo})
                  :passes (if hull
                            [pass/outline]
                            [pass/transparent pass/selection])
                  :user-data (cond-> {:color color
                                      :geometry (cond-> {:primitive-type (if hull
                                                                           GL2/GL_POINTS
                                                                           GL2/GL_TRIANGLES)
                                                         :position-buffer position-buffer}
                                                  (and (not hull) index-buffer) (assoc :index-buffer index-buffer))}
                               hull (assoc :point-count (graphics.types/element-count position-buffer)
                                           :point-size 3.0)
                               (not hull) (assoc :double-sided true))}}))

(g/defnk produce-mesh-shape-scene
  [_node-id pose color node-outline-key shape-type selected-collision-mesh selected-collision-mesh-renderable]
  (when (and selected-collision-mesh
             selected-collision-mesh-renderable
             (or (= :type-hull shape-type)
                 (and (= :type-mesh shape-type)
                      (coll/every? :triangles (:primitives selected-collision-mesh)))))
    (let [hull (= :type-hull shape-type)
          children (into []
                         (keep (partial collision-mesh-primitive-scene _node-id color hull))
                         (:primitives selected-collision-mesh-renderable))]
      (when (coll/not-empty children)
        {:node-id _node-id
         :node-outline-key node-outline-key
         :pose pose
         :aabb (:aabb selected-collision-mesh-renderable)
         :children children}))))

(defn- mesh-shape-selection-error [node-id shape-type mesh-scene mesh-name mesh-index collision-meshes]
  (let [selected-collision-mesh (when-not (g/error-value? collision-meshes)
                                  (model-loader/resolve-named-mesh collision-meshes mesh-name mesh-index))]
    (cond
      (nil? mesh-scene)
      (g/->error node-id :mesh-scene :fatal mesh-scene
                 (localization/message "error.collision-object-mesh-shape-scene-required"))

      (g/error-value? collision-meshes)
      collision-meshes

      (coll/empty? (model-loader/named-meshes collision-meshes))
      (g/->error node-id :mesh-index :fatal mesh-index
                 (localization/message "error.collision-object-mesh-shape-no-named-meshes"))

      (string/blank? mesh-name)
      (g/->error node-id :mesh-index :fatal mesh-index
                 (localization/message "error.collision-object-mesh-shape-mesh-required"))

      (nil? selected-collision-mesh)
      (g/->error node-id :mesh-index :fatal mesh-index
                 (localization/message "error.collision-object-mesh-shape-mesh-missing"
                                       {"mesh" mesh-name}))

      (and (= :type-mesh shape-type)
           (not (coll/every? :triangles (:primitives selected-collision-mesh))))
      (g/->error node-id :mesh-index :fatal mesh-index
                 (localization/message "error.collision-object-mesh-shape-triangle-topology-required"))

      (and (= :type-mesh shape-type)
           (zero? (long (transduce (map :index-count) + 0 (:primitives selected-collision-mesh)))))
      (g/->error node-id :mesh-index :fatal mesh-index
                 (localization/message "error.collision-object-mesh-shape-empty"))

      (and (= :type-hull shape-type)
           (zero? (long (transduce (map :position-count) + 0 (:primitives selected-collision-mesh)))))
      (g/->error node-id :mesh-index :fatal mesh-index
                 (localization/message "error.collision-object-hull-shape-empty"))

      :else
      nil)))

(g/defnode MeshSourceShape
  (inherits Shape)

  (display-order [Shape])

  (output scene g/Any produce-mesh-shape-scene)
  (output shape-errors g/Any
          (g/fnk [_node-id id id-counts shape-type mesh-scene mesh-name mesh-index ^:try collision-meshes]
            (g/package-errors _node-id
              (validate-image-id _node-id id id-counts)
              (mesh-shape-selection-error _node-id shape-type mesh-scene mesh-name mesh-index collision-meshes))))
  (output shape-data g/Any (g/constantly [])))

(g/defnode HullShape
  (inherits MeshSourceShape)

  (property shape-type g/Any
            (default :type-hull)))

(g/defnode MeshShape
  (inherits MeshSourceShape)

  (property shape-type g/Any
            (default :type-mesh)))

(defn- resolve-shape-node-outline-key [evaluation-context parent-node shape-node]
  (let [type-label (:label (shape-type-ui (g/node-value shape-node :shape-type evaluation-context)))
        taken-keys (outline/taken-node-outline-keys parent-node evaluation-context)]
    (g/update-property shape-node :node-outline-key (fnil outline/next-node-outline-key type-label) taken-keys)))

(defn attach-shape-node
  ([parent shape-node]
   (attach-shape-node true parent shape-node))
  ([resolve-node-outline-key? parent shape-node]
   (concat
     (when resolve-node-outline-key?
       ;; resolve the node outline key before connecting the shape node so taken
       ;; node outline keys don't include the shape node key
       (g/expand-ec resolve-shape-node-outline-key parent shape-node))
     (g/connect shape-node :_node-id              parent     :nodes)
     (g/connect shape-node :node-outline          parent     :child-outlines)
     (g/connect shape-node :scene                 parent     :child-scenes)
     (g/connect shape-node :shape                 parent     :shapes)
     (g/connect shape-node :shape-errors          parent     :shape-errors)
     (g/connect shape-node :collision-mesh-set-info parent :collision-mesh-set-infos)
     (g/connect parent     :id-counts             shape-node :id-counts)
     (g/connect parent     :collision-group-color shape-node :color)
     (g/connect parent     :project-physics-type  shape-node :project-physics-type))))

(defmulti decode-shape-data
  (fn [shape _data] (:shape-type shape)))

(defmethod decode-shape-data :type-sphere
  [_shape [^double r]]
  {:diameter (* 2.0 r)})

(defmethod decode-shape-data :type-box
  [_shape [^double ext-x ^double ext-y ^double ext-z]]
  {:dimensions [(* 2.0 ext-x) (* 2.0 ext-y) (* 2.0 ext-z)]})

(defmethod decode-shape-data :type-capsule
  [_shape [^double r h]]
  {:diameter (* 2.0 r)
   :height h})

(defmethod decode-shape-data :type-mesh
  [_shape _data]
  {})

(defmethod decode-shape-data :type-hull
  [_shape _data]
  {})

(defn make-shape-node
  [parent {:keys [shape-type] :as shape}]
  (let [graph-id (g/node-id->graph-id parent)
        node-type (case shape-type
                    :type-sphere SphereShape
                    :type-box BoxShape
                    :type-capsule CapsuleShape
                    :type-hull HullShape
                    :type-mesh MeshShape)
        node-props (dissoc shape :index :count :id-hash)]
    (g/make-nodes
      graph-id
      [shape-node [node-type node-props]]
      (attach-shape-node false parent shape-node))))

(defn- decode-embedded-shape [embedded-collision-shape-data shape]
  (let [shape-data (if embedded-collision-shape-data
                     (let [^long count (:count shape)
                           ^long index (:index shape)]
                       (subvec embedded-collision-shape-data index (+ index count)))
                     protobuf/vector3-zero)
        decoded-shape-data (decode-shape-data shape shape-data)]
    (merge shape decoded-shape-data)))

(defn load-collision-object
  [project self resource collision-object-desc]
  {:pre [(map? collision-object-desc)]} ; Physics$CollisionObjectDesc in map format.
  (let [basis (g/now)
        resolve-resource #(workspace/resolve-resource basis resource %)
        resolve-shape-resources (fn [shape]
                                  (cond-> shape
                                          (:mesh-scene shape)
                                          (update :mesh-scene resolve-resource)))
        to-comma-separated-string #(some->> % (string/join ", "))]
    (concat
      (gu/set-properties-from-pb-map self Physics$CollisionObjectDesc collision-object-desc
        collision-shape (resolve-resource :collision-shape)
        type :type
        mass :mass
        friction :friction
        restitution :restitution
        group :group
        mask (to-comma-separated-string :mask)
        linear-damping :linear-damping
        angular-damping :angular-damping
        locked-rotation :locked-rotation
        bullet :bullet
        event-collision :event-collision
        event-contact :event-contact
        event-trigger :event-trigger)
      (g/connect self :collision-group-node project :collision-group-nodes)
      (g/connect project :collision-groups-data self :collision-groups-data)
      (g/connect project :settings self :project-settings)
      (when-some [{:keys [data shapes]} (:embedded-collision-shape collision-object-desc)]
        (sequence (comp (map #(assoc %1 :node-outline-key %2))
                        (map resolve-shape-resources)
                        (map #(decode-embedded-shape data %))
                        (map #(make-shape-node self %)))
                  shapes
                  (outline/gen-node-outline-keys (map (comp shape-type-label :shape-type)
                                                      shapes)))))))

(defn convex-hull-scene
  [_node-id convex-shape-data color project-physics-type]
  (when (and (= (:shape-type convex-shape-data) :type-hull)
             (not-empty (:data convex-shape-data)))
    (let [points (partition 3 (:data convex-shape-data))
          [min-coords max-coords] (reduce (fn [[min-point max-point] point]
                                            [(mapv min min-point point) (mapv max max-point point)])
                                          [(repeat Double/MAX_VALUE) (repeat Double/MIN_VALUE)]
                                          points)
          aabb (geom/coords->aabb max-coords min-coords)
          vbuf (vtx/flip! (reduce (fn [vb [x y z]] (scene-shapes/pos-vtx-put! vb x y z 0.0))
                                  (scene-shapes/->pos-vtx (count points) :static)
                                  points))]
      (if (= "2D" project-physics-type)
        {:node-id _node-id
         :node-outline-key "2D Convex Hull"
         :aabb aabb
         :renderable {:render-fn render-triangles-uniform-scale
                      :tags #{:collision-shape :gizmo}
                      :passes [pass/transparent pass/selection]
                      :user-data {:color color
                                  :double-sided true
                                  :geometry {:primitive-type GL2/GL_POLYGON
                                             :vbuf vbuf}}}
         :children [{:node-id _node-id
                     :aabb aabb
                     :renderable {:render-fn render-lines-uniform-scale
                                  :tags #{:collision-shape :gizmo :outline}
                                  :passes [pass/outline]
                                  :user-data {:color color
                                              :geometry {:primitive-type GL2/GL_LINE_LOOP
                                                         :vbuf vbuf}}}}]}
        {:node-id _node-id
         :node-outline-key "3D Convex Hull"
         :aabb aabb
         :renderable {:render-fn render-points-uniform-scale
                      :tags #{:collision-shape :gizmo :outline}
                      :passes [pass/outline]
                      :user-data {:color color
                                  :point-size 3.0
                                  :geometry {:primitive-type GL2/GL_POINTS
                                             :vbuf vbuf}}}}))))

(g/defnk produce-scene
  [_node-id child-scenes convex-shape-data collision-group-color project-physics-type]
  {:node-id _node-id
   :aabb geom/null-aabb
   :renderable {:passes [pass/selection]}
   :children (if convex-shape-data
               [(convex-hull-scene _node-id convex-shape-data collision-group-color project-physics-type)]
               child-scenes)})

(defn- make-embedded-collision-shape [shapes]
  (loop [idx 0
         [shape & rest] shapes
         ret {:shapes [] :data []}]
    (if-not shape
      ret
      (let [data (:data shape)
            data-len (count data)
            shape-msg (-> shape
                          (assoc :index (if (mesh-source-shape-type? (:shape-type shape)) 0 idx)
                                 :count data-len)
                          (dissoc :data))]
        (recur (+ idx data-len)
               rest
               (-> ret
                   (update :shapes conj shape-msg)
                   (update :data into data)))))))

(defn- strip-empty-embedded-collision-shape [collision-object-desc]
  ;; Physics$CollisionObjectDesc in map format.
  (cond-> collision-object-desc

          (empty? (:shapes (:embedded-collision-shape collision-object-desc)))
          (dissoc :embedded-collision-shape)))

(g/defnk produce-save-value
  [collision-shape-resource type mass friction restitution
   group mask angular-damping linear-damping locked-rotation bullet event-collision event-contact event-trigger
   shapes]
  (let [embedded-collision-shape (make-embedded-collision-shape shapes)
        mask (cond-> []
                     (some? mask)
                     (into (comp (map string/trim)
                                 (remove string/blank?))
                           (string/split mask #",")))]
    (-> (protobuf/make-map-without-defaults Physics$CollisionObjectDesc
          :collision-shape (resource/resource->proj-path collision-shape-resource)
          :type type
          :mass mass
          :friction friction
          :restitution restitution
          :group group
          :mask mask
          :linear-damping linear-damping
          :angular-damping angular-damping
          :locked-rotation locked-rotation
          :bullet bullet
          :event-collision event-collision
          :event-contact event-contact
          :event-trigger event-trigger
          :embedded-collision-shape embedded-collision-shape)
        (strip-empty-embedded-collision-shape))))

(defn build-collision-object
  [resource dep-resources user-data]
  (let [{:keys [collision-shape-build-resource mesh-sets pb-msg]} user-data
        shape (when collision-shape-build-resource
                (get dep-resources collision-shape-build-resource))
        pb-msg (cond-> pb-msg
                 shape (assoc :collision-shape (resource/proj-path shape)))
        collision-object (protobuf/map->pb Physics$CollisionObjectDesc pb-msg)
        collision-object-builder (.toBuilder ^Physics$CollisionObjectDesc collision-object)
        mesh-sets (into {}
                        (map (fn [[mesh-scene mesh-set]]
                               [mesh-scene (protobuf/map->pb Rig$MeshSet mesh-set)]))
                        mesh-sets)]
    (CollisionMeshCompiler/compile (.getEmbeddedCollisionShapeBuilder collision-object-builder) mesh-sets)
    {:resource resource
     :content (.toByteArray (.build collision-object-builder))}))

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

(defn- insert-id-hashes [shapes]
  (mapv (fn [shape]
          (let [shape-id (:id shape)
                shape-id-hash (if (empty? shape-id)
                                0
                                (murmur/hash64 shape-id))]
            (assoc shape :id-hash shape-id-hash)))
        shapes))

(g/defnk produce-build-targets
  [_node-id resource save-value collision-shape dep-build-targets shape-errors mass type project-physics-type shapes collision-mesh-set-infos id-counts]
  (let [dep-build-targets (flatten dep-build-targets)
        convex-shape (when (and collision-shape (= "convexshape" (resource/type-ext collision-shape)))
                       (get-in (first dep-build-targets) [:user-data :pb]))
        pb-msg (if convex-shape
                 (dissoc save-value :collision-shape) ; Convex shape will be merged into :embedded-collision-shape below.
                 save-value)
        dep-build-targets (if convex-shape [] dep-build-targets)
        deps-by-source (into {} (map #(let [res (:resource %)] [(:resource res) res]) dep-build-targets))
        collision-shape-build-resource (when-not convex-shape
                                         (get deps-by-source collision-shape))
        mesh-sets
        (into {}
              (keep (fn [{:keys [mesh-scene mesh-set]}]
                      (when mesh-scene
                        [mesh-scene mesh-set])))
              collision-mesh-set-infos)
        pb-msg (-> pb-msg
                   (update :embedded-collision-shape merge-convex-shape convex-shape)
                   (update-in [:embedded-collision-shape :shapes] insert-id-hashes))]
    (g/precluding-errors
      [shape-errors
       (validation/prop-error :fatal _node-id :collision-shape validation/prop-resource-not-exists? collision-shape collision-shape-message)
       (when (= :collision-object-type-dynamic type)
         (validation/prop-error :fatal _node-id :mass validation/prop-zero-or-below? mass mass-message))
       (when (and (empty? (:collision-shape pb-msg))
                  (empty? (:embedded-collision-shape pb-msg)))
         (g/->error _node-id :collision-shape :fatal collision-shape (localization/message "error.collision-object-has-no-shapes")))
       (validation/prop-error :fatal _node-id :collision-shape validation/prop-collision-shape-conflict? shapes collision-shape)
       (sequence (comp (map :shape-type)
                       (distinct)
                       (remove #(contains? (shape-type-physics-types %) project-physics-type))
                       (map #(localization/message "error.collision-object-shape-not-supported-in-physics"
                                                   {"shape" (shape-type-message %)
                                                    "physics" project-physics-type}))
                       (map #(g/->error _node-id :shapes :fatal shapes %)))
                 shapes)
       (sequence (comp
                   (map :id)
                   (map #(validate-image-id _node-id % id-counts)))
                 shapes)]
      [(bt/with-content-hash
         {:node-id _node-id
          :resource (workspace/make-build-resource resource)
          :build-fn build-collision-object
          :user-data {:pb-msg pb-msg
                      :collision-shape-build-resource collision-shape-build-resource
                      :mesh-sets mesh-sets}
          :deps dep-build-targets})])))

(g/defnk produce-collision-group-color
  [collision-groups-data group]
  (collision-groups/color collision-groups-data group))

(defn- tilemap-collision-shape? [collision-shape]
  (boolean
    (when collision-shape
      (contains? #{"tilemap" "tilegrid"} (resource/type-ext collision-shape)))))

(g/defnode CollisionObjectNode
  (inherits resource-node/ResourceNode)

  (input shapes g/Any :array)
  (input child-scenes g/Any :array)
  (input collision-shape-resource resource/Resource)
  (input dep-build-targets g/Any :array)
  (input collision-mesh-set-infos g/Any :array)
  (input collision-groups-data g/Any)
  (input project-settings g/Any)
  (input convex-shape-data g/Any)
  (input shape-errors g/Any :array)

  (property collision-shape resource/Resource ; Nil is valid default.
            (value (gu/passthrough collision-shape-resource))
            (set (fn [evaluation-context self old-value new-value]
                   (project/resource-setter evaluation-context self old-value new-value
                                            [:resource :collision-shape-resource]
                                            [:build-targets :dep-build-targets]
                                            [:save-value :convex-shape-data])))
            (dynamic edit-type (g/constantly {:type resource/Resource :ext #{"convexshape" "tilemap"}}))
            (dynamic error (g/fnk [_node-id collision-shape shapes]
                             (or (validation/prop-error :fatal _node-id :collision-shape validation/prop-resource-not-exists? collision-shape collision-shape-message)
                                 (validation/prop-error :fatal _node-id :collision-shape validation/prop-collision-shape-conflict? shapes collision-shape))))
            (dynamic label (properties/label-dynamic :collision-object :collision-shape))
            (dynamic tooltip (properties/tooltip-dynamic :collision-object :collision-shape)))

  (property type g/Any ; Required protobuf field.
            (dynamic edit-type (g/constantly (properties/->pb-choicebox Physics$CollisionObjectType)))
            (dynamic label (properties/label-dynamic :collision-object :type))
            (dynamic tooltip (properties/tooltip-dynamic :collision-object :type)))

  (property mass g/Num ; Required protobuf field.
            (value (g/fnk [mass type]
                     (if (= :collision-object-type-dynamic type) mass 0.0)))
            (dynamic read-only? (g/fnk [type]
                                  (not= :collision-object-type-dynamic type)))
            (dynamic error (g/fnk [_node-id mass type]
                             (when (= :collision-object-type-dynamic type)
                               (validation/prop-error :fatal _node-id :mass validation/prop-zero-or-below? mass mass-message))))
            (dynamic label (properties/label-dynamic :collision-object :mass))
            (dynamic tooltip (properties/tooltip-dynamic :collision-object :mass)))

  (property friction g/Num ; Required protobuf field.
            (dynamic label (properties/label-dynamic :collision-object :friction))
            (dynamic tooltip (properties/tooltip-dynamic :collision-object :friction)))
  (property restitution g/Num ; Required protobuf field.
            (dynamic label (properties/label-dynamic :collision-object :restitution))
            (dynamic tooltip (properties/tooltip-dynamic :collision-object :restitution)))
  (property linear-damping g/Num
            (default (protobuf/default Physics$CollisionObjectDesc :linear-damping))
            (dynamic label (properties/label-dynamic :collision-object :linear-damping))
            (dynamic tooltip (properties/tooltip-dynamic :collision-object :linear-damping)))
  (property angular-damping g/Num
            (default (protobuf/default Physics$CollisionObjectDesc :angular-damping))
            (dynamic label (properties/label-dynamic :collision-object :angular-damping))
            (dynamic tooltip (properties/tooltip-dynamic :collision-object :angular-damping)))
  (property locked-rotation g/Bool
            (default (protobuf/default Physics$CollisionObjectDesc :locked-rotation))
            (dynamic label (properties/label-dynamic :collision-object :locked-rotation))
            (dynamic tooltip (properties/tooltip-dynamic :collision-object :locked-rotation)))
  (property bullet g/Bool
            (default (protobuf/default Physics$CollisionObjectDesc :bullet))
            (dynamic label (properties/label-dynamic :collision-object :bullet))
            (dynamic tooltip (properties/tooltip-dynamic :collision-object :bullet)))
  (property event-collision g/Bool
            (default (protobuf/default Physics$CollisionObjectDesc :event-collision))
            (dynamic label (properties/label-dynamic :collision-object :event-collision))
            (dynamic tooltip (properties/tooltip-dynamic :collision-object :event-collision)))
  (property event-contact g/Bool
            (default (protobuf/default Physics$CollisionObjectDesc :event-contact))
            (dynamic label (properties/label-dynamic :collision-object :event-contact))
            (dynamic tooltip (properties/tooltip-dynamic :collision-object :event-contact)))
  (property event-trigger g/Bool
            (default (protobuf/default Physics$CollisionObjectDesc :event-trigger))
            (dynamic label (properties/label-dynamic :collision-object :event-trigger))
            (dynamic tooltip (properties/tooltip-dynamic :collision-object :event-trigger)))

  (property group g/Str ; Required protobuf field.
            (dynamic read-only? (g/fnk [collision-shape] (tilemap-collision-shape? collision-shape)))
            (dynamic label (properties/label-dynamic :collision-object :group))
            (dynamic tooltip (g/fnk [collision-shape]
                               (localization/message
                                 (if (tilemap-collision-shape? collision-shape)
                                   "property.collision-object.group.tilemap.tooltip"
                                   "property.collision-object.group.tooltip")))))
  (property mask g/Str ; Nil is valid default.
            (dynamic label (properties/label-dynamic :collision-object :mask))
            (dynamic tooltip (properties/tooltip-dynamic :collision-object :mask)))

  (output scene g/Any :cached produce-scene)
  (output project-physics-type PhysicsType (g/fnk [project-settings] (project-physics-type project-settings)))
  (output node-outline outline/OutlineData :cached (g/fnk [_node-id child-outlines]
                                                     {:node-id _node-id
                                                      :node-outline-key "Collision Object"
                                                      :label (localization/message "outline.collision-object")
                                                      :icon collision-object-icon
                                                      :children (localization/annotate-as-sorted localization/natural-sort-by-label child-outlines)
                                                      :child-reqs [{:node-type Shape
                                                                    :tx-attach-fn attach-shape-node}]}))

  (output id-counts NameCounts :cached (g/fnk [shapes] (frequencies (keep :id shapes))))
  (output save-value g/Any :cached produce-save-value)
  (output build-targets g/Any :cached produce-build-targets)
  (output collision-group-node g/Any :cached (g/fnk [_node-id group] {:node-id _node-id :collision-group group}))
  (output collision-group-color g/Any :cached produce-collision-group-color))

(node-types/register-node-type-name! SphereShape "shape-type-sphere")
(node-types/register-node-type-name! BoxShape "shape-type-box")
(node-types/register-node-type-name! CapsuleShape "shape-type-capsule")
(node-types/register-node-type-name! HullShape "shape-type-hull")
(node-types/register-node-type-name! MeshShape "shape-type-mesh")

(defn- sanitize-collision-object [collision-object-desc]
  (strip-empty-embedded-collision-shape collision-object-desc))

(defn register-resource-types [workspace]
  (concat
    (attachment/register
      workspace CollisionObjectNode :shapes
      :add {SphereShape attach-shape-node
            BoxShape attach-shape-node
            CapsuleShape attach-shape-node
            HullShape attach-shape-node
            MeshShape attach-shape-node}
      :get attachment/nodes-getter)
    (resource-node/register-ddf-resource-type workspace
      :ext "collisionobject"
      :label (localization/message "resource.type.collisionobject")
      :node-type CollisionObjectNode
      :ddf-type Physics$CollisionObjectDesc
      :load-fn load-collision-object
      :sanitize-fn sanitize-collision-object
      :icon collision-object-icon
      :icon-class :design
      :category (localization/message "resource.category.components")
      :view-types [:scene :text]
      :view-opts {:scene {:grid true}}
      :tags #{:component}
      :tag-opts {:component {:transform-properties #{}}})))

;; outline context menu

(defn- default-shape
  [shape-type]
  (merge (protobuf/make-map-without-defaults Physics$CollisionShape$Shape
           :shape-type shape-type)
         (case shape-type
           :type-sphere {:diameter 20.0}
           :type-box {:dimensions [20.0 20.0 20.0]}
           :type-capsule {:diameter 20.0 :height 40.0}
           :type-hull {}
           :type-mesh {})))

(defn- add-shape-handler
  [collision-object-node shape-type select-fn]
  (let [op-seq (gensym)
        node-outline-key (outline/next-node-outline-key (shape-type-label shape-type)
                                                        (outline/taken-node-outline-keys collision-object-node))
        shape (assoc (default-shape shape-type) :node-outline-key node-outline-key)
        shape-node (first (g/tx-nodes-added
                            (g/transact
                              (concat
                                (g/operation-label (localization/message "operation.collision-object.add-shape"))
                                (g/operation-sequence op-seq)
                                (make-shape-node collision-object-node shape)))))]
    (when (some? select-fn)
      (g/transact
        (concat
          (g/operation-sequence op-seq)
          (select-fn [shape-node]))))))

(defn- selection->collision-object [selection evaluation-context]
  (handler/adapt-single selection CollisionObjectNode evaluation-context))

(handler/defhandler :edit.add-embedded-component :workbench
  (label [user-data]
    (if-not user-data
      (localization/message "command.edit.add-embedded-component.variant.collision-object")
      (shape-type-message (:shape-type user-data))))
  (active? [selection evaluation-context] (selection->collision-object selection evaluation-context))
  (run [selection user-data app-view]
    (g/let-ec [self (selection->collision-object selection evaluation-context)]
      (add-shape-handler self (:shape-type user-data) (fn [node-ids] (app-view/select app-view node-ids)))))
  (options [selection user-data evaluation-context]
    (let [self (selection->collision-object selection evaluation-context)]
      (when-not user-data
        (->> shape-type-ui
             (reduce-kv
               (fn [acc shape-type {:keys [icon message physics-types]}]
                 (if-not (contains? physics-types (g/node-value self :project-physics-type evaluation-context))
                   acc
                   (conj! acc {:label message
                               :icon icon
                               :command :edit.add-embedded-component
                               :user-data {:_node-id self :shape-type shape-type}})))
               (transient []))
             persistent!
             (localization/annotate-as-sorted localization/natural-sort-by-label))))))

(ext-graph/register-property-getter!
  ::CollisionObjectNode
  (fn CollisionObjectNode-getter [node-id property evaluation-context]
    (case property
      "collision_type"
      (let [node (g/node-by-id (:basis evaluation-context) node-id)]
        (when-let [edit-type-id (properties/edit-type-id (g/node-property-dynamic node :type :edit-type evaluation-context))]
          (when-let [converter (-> edit-type-id ext-graph/edit-type-id->value-converter :to)]
            #(converter (g/node-value node-id :type evaluation-context)))))

      nil))
  (fn CollisionObjectNode-lister [node-id evaluation-context]
    (let [node (g/node-by-id (:basis evaluation-context) node-id)]
      (when-let [edit-type-id (properties/edit-type-id (g/node-property-dynamic node :type :edit-type evaluation-context))]
        (when-let [_converter (-> edit-type-id ext-graph/edit-type-id->value-converter :to)]
          ["collision_type"])))))

(ext-graph/register-property-setter!
  ::CollisionObjectNode
  (fn CollisionObjectNode-setter [node-id property rt project evaluation-context]
    (case property
      "collision_type"
      (let [node (g/node-by-id (:basis evaluation-context) node-id)]
        (when-not (g/node-property-dynamic node :type :read-only? false evaluation-context)
          (let [edit-type (g/node-property-dynamic node :type :edit-type evaluation-context)]
            (when-let [converter (-> edit-type properties/edit-type-id ext-graph/edit-type-id->value-converter :from)]
              #(g/set-property node-id :type (converter % rt edit-type project evaluation-context))))))

      nil)))
