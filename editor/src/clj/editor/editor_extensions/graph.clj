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

(ns editor.editor-extensions.graph
  "Code for interacting with graph from editor scripts, i.e. get/set values"
  (:require [clojure.string :as string]
            [dynamo.graph :as g]
            [editor.attachment :as attachment]
            [editor.code.util :as code.util]
            [editor.collision-groups :as collision-groups]
            [editor.defold-project :as project]
            [editor.editor-extensions.coerce :as coerce]
            [editor.editor-extensions.node-types :as node-types]
            [editor.editor-extensions.runtime :as rt]
            [editor.editor-extensions.tile-map :as tile-map]
            [editor.form :as form]
            [editor.game-project :as game-project]
            [editor.gui-attachment :as gui-attachment]
            [editor.id :as id]
            [editor.outline :as outline]
            [editor.properties :as properties]
            [editor.protobuf :as protobuf]
            [editor.resource :as resource]
            [editor.settings-core :as settings-core]
            [editor.types :as types]
            [editor.util :as util]
            [editor.workspace :as workspace]
            [util.coll :as coll]
            [util.fn :as fn]
            [util.id-vec :as iv])
  (:import [com.dynamo.particle.proto Particle$ModifierType]
           [editor.editor_extensions.tile_map Tiles]
           [editor.properties Curve CurveSpread]
           [java.net URI URISyntaxException]
           [java.util ArrayDeque HashSet]
           [javax.vecmath Vector3d]
           [org.apache.commons.io FilenameUtils]
           [org.luaj.vm2 LuaError]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(def resource-path-coercer
  (coerce/wrap-with-pred coerce/string #(and (string? %) (string/starts-with? % "/")) "is not a resource path"))

(def node-id-or-path-coercer
  (coerce/one-of
    (coerce/wrap-with-pred coerce/userdata g/node-id? "is not a node id")
    resource-path-coercer))

(defn resolve-node-id-or-path
  "Resolve node id or proj-path to either a node id or a folder resource

  If a path points to a file resource, it will be resolved to node-id"
  [node-id-or-path project evaluation-context]
  (if (string? node-id-or-path)
    (let [resource (workspace/find-resource (project/workspace project evaluation-context) node-id-or-path evaluation-context)]
      (when-not resource
        (throw (LuaError. (str node-id-or-path " not found"))))
      (or (project/get-resource-node project resource evaluation-context)
          resource))
    node-id-or-path))

(defn node-id-or-path->node-id
  "Coerce a value that may be node id or proj-path to existing node id"
  [node-id-or-path project evaluation-context]
  (let [node-id-or-resource (resolve-node-id-or-path node-id-or-path project evaluation-context)]
    (when (resource/resource? node-id-or-resource)
      (throw (LuaError. (str (resource/proj-path node-id-or-resource) " is not a file resource"))))
    node-id-or-resource))

(defn node-id->type-keyword [node-id evaluation-context]
  (g/node-type-kw (:basis evaluation-context) node-id))

;; region get

(defn- coercing-converter [coercer]
  (fn [lua-value rt _edit-type _project _evaluation-context]
    (rt/->clj rt coercer lua-value)))

(def node-id-or-path-or-empty-string-coercer
  (coerce/one-of
    node-id-or-path-coercer
    (coerce/enum "")))

(defn- resource-converter [lua-value rt edit-type project evaluation-context]
  (let [node-id-or-path (rt/->clj rt node-id-or-path-or-empty-string-coercer lua-value)]
    (when-not (= node-id-or-path "")
      (let [resource (if (string? node-id-or-path)
                       (-> project
                           (project/workspace evaluation-context)
                           (workspace/resolve-workspace-resource node-id-or-path evaluation-context))
                       (g/node-value node-id-or-path :resource evaluation-context))
            ext (:ext edit-type)
            ext (if (string? ext) [ext] ext)]
        (when (and (seq ext)
                   (not ((set ext) (resource/type-ext resource))))
          (throw (LuaError. (str "resource extension should be "
                                 (->> ext sort (util/join-words ", " " or "))))))
        resource))))

(defn- choicebox-converter [lua-value rt edit-type _project _evaluation-context]
  (let [opts (->> edit-type :options (mapv first))]
    (rt/->clj rt (apply coerce/enum opts) lua-value)))

(defn- slider-converter [lua-value rt edit-type _project _evaluation-context]
  (let [{:keys [min max] :or {min 0.0 max 1.0}} edit-type
        coercer (coerce/wrap-with-pred coerce/number #(<= min % max) (str "should be between " min " and " max))]
    (double (rt/->clj rt coercer lua-value))))

(defn- curve-points->lua [points]
  (->> points
       (iv/iv-vals)
       (sort-by #(% 0))
       (mapv #(array-map :x (% 0) :y (% 1) :tx (% 2) :ty (% 3)))))

(defn- Curve->lua [^Curve curve]
  {:points (curve-points->lua (.-points curve))})

(defn- CurveSpread->lua [^CurveSpread curve-spread]
  {:points (curve-points->lua (.-points curve-spread))
   :spread (.-spread curve-spread)})

(def ^:private curve-points-coercer
  (-> (coerce/hash-map
        :req {:x (-> coerce/number
                     (coerce/wrap-with-pred #(<= 0.0 (double %) 1.0) "is not between 0 and 1")
                     (coerce/wrap-transform float))
              :y (coerce/wrap-transform coerce/number float)
              :tx (-> coerce/number
                      (coerce/wrap-with-pred #(<= 0.0 (double %) 1.0) "is not between 0 and 1")
                      (coerce/wrap-transform #(Math/max (double 0.001) (double %))))
              :ty (-> coerce/number
                      (coerce/wrap-with-pred #(<= -1.0 (double %) 1.0) "is not between -1 and 1")
                      (coerce/wrap-transform double))}
        :extra-keys false)
      (coerce/vector-of :min-count 1)
      (coerce/wrap-with-pred
        (fn validate-curve-points [points]
          (and (zero? (double (:x (points 0))))
               (apply < (mapv :x points))
               (or (= 1 (count points))
                   (= 1.0 (:x (peek points))))))
        "does not increase xs monotonically from 0 to 1")
      (coerce/wrap-transform
        (fn transform-curve-points [points]
          (mapv
            (fn transform-point [{:keys [x y tx ty]}]
              (let [tangent (doto (Vector3d. tx ty 0) (.normalize))]
                [x y (float (.-x tangent)) (float (.-y tangent))]))
            points)))))

(def ^:private number-curve-coercer
  (coerce/wrap-transform
    coerce/number
    (fn transform-number->curve [n]
      {:points [[protobuf/float-zero (float n) protobuf/float-one protobuf/float-zero]]})))

(def ^:private Curve-coercer
  (coerce/wrap-transform
    (coerce/one-of
      number-curve-coercer
      (coerce/hash-map :opt {:points curve-points-coercer}
                       :extra-keys false))
    #(properties/->curve (:points %))))

(def ^:private CurveSpread-coercer
  (coerce/wrap-transform
    (coerce/one-of
      number-curve-coercer
      (coerce/hash-map :opt {:points curve-points-coercer
                             :spread (coerce/wrap-transform coerce/number float)}
                       :extra-keys false))
    #(properties/->curve-spread (:points %) (:spread %))))

(def edit-type-id->value-converter
  {g/Str {:to identity :from (coercing-converter coerce/string)}
   g/Bool {:to identity :from (coercing-converter coerce/boolean)}
   g/Num {:to identity :from (coercing-converter coerce/number)}
   g/Int {:to identity :from (coercing-converter coerce/integer)}
   types/Vec2 {:to identity :from (coercing-converter (coerce/tuple coerce/number coerce/number))}
   types/Vec3 {:to identity :from (coercing-converter (coerce/tuple coerce/number coerce/number coerce/number))}
   types/Vec4 {:to identity :from (coercing-converter (coerce/tuple coerce/number coerce/number coerce/number coerce/number))}
   'editor.resource.Resource {:to resource/proj-path :from resource-converter}
   types/Color {:to identity :from (coercing-converter (coerce/tuple coerce/number coerce/number coerce/number coerce/number))}
   :multi-line-text {:to identity :from (coercing-converter coerce/string)}
   :choicebox {:to identity :from choicebox-converter}
   :slider {:to identity :from slider-converter}
   Curve {:to Curve->lua :from (coercing-converter Curve-coercer)}
   CurveSpread {:to CurveSpread->lua :from (coercing-converter CurveSpread-coercer)}})

(defn- property->prop-kw [property]
  (if (string/starts-with? property "__")
    (keyword property)
    (keyword (string/replace property "_" "-"))))

(defn- outline-property [node-id property evaluation-context]
  (let [prop-kw (property->prop-kw property)
        outline-property (-> node-id
                             (g/node-value :_properties evaluation-context)
                             (get-in [:properties prop-kw]))]
    (when (and outline-property
               (properties/visible? outline-property)
               (edit-type-id->value-converter (properties/property-edit-type-id outline-property)))
      (cond-> outline-property
              (not (contains? outline-property :prop-kw))
              (assoc :prop-kw prop-kw)))))

(defn- make-property-fn-builder [{:keys [parents]} node-type-kw->f]
  (fn/memoize
    (fn build-property-fn [node-type-kw]
      (let [seen (HashSet.)
            queue (doto (ArrayDeque.) (.add node-type-kw))
            fs (loop [fs (transient [])]
                 (if-let [node-type-kw (.pollFirst queue)]
                   (if (.contains seen node-type-kw)
                     (recur fs)
                     (do
                       (.add seen node-type-kw)
                       (some->> (parents node-type-kw) (run! #(.add queue %)))
                       (recur (if-let [f (node-type-kw->f node-type-kw)]
                                (conj! fs f)
                                fs))))
                   (persistent! fs)))]
        (case (count fs)
          0 fn/constantly-nil
          1 (fs 0)
          (fn
            ([] (coll/some #(%) fs))
            ([a] (coll/some #(% a) fs))
            ([a b] (coll/some #(% a b) fs))
            ([a b c] (coll/some #(% a b c) fs))
            ([a b c d] (coll/some #(% a b c d) fs))
            ([a b c d e] (coll/some #(% a b c d e) fs))))))))

(defn make-inheritance-chain
  "Create an inheritance chain

  The inheritance chain is somewhat similar to multimethods. It uses hierarchy,
  though it dispatches only on a single value. Register methods by calling the
  inheritance chain instance with 2 args: dispatch value and a function.
  Get a method chain by calling the inheritance chain instance with a dispatch
  value. If there are multiple methods that satisfy the dispatch value, they
  will be tried in order, from most specific to least specific, until a method
  returns a non-nil value."
  ([]
   (make-inheritance-chain #'clojure.core/global-hierarchy))
  ([hierarchy-ref]
   (let [state-atom (atom {::builder nil
                           ::hierarchy @hierarchy-ref
                           ::node-type-kw->f {}})]
     (fn inheritance-chain
       ([node-type-kw]
        (let [h @hierarchy-ref
              {::keys [builder hierarchy] :as current-state} @state-atom
              builder (if (and builder (identical? hierarchy h))
                        builder
                        (let [builder (make-property-fn-builder h (::node-type-kw->f current-state))]
                          (swap! state-atom assoc ::hierarchy h ::builder builder)
                          builder))]
          (builder node-type-kw)))
       ([node-type-kw f]
        (swap! state-atom #(-> % (update ::node-type-kw->f assoc node-type-kw f) (assoc ::builder nil)))
        nil)))))

(defonce ^:private ext-property-getter
  (make-inheritance-chain))

(defn register-property-getter!
  "Register a getter associated with a node type

  Args:
    node-type-kw    keyword of a graph node type
    f               a function of 3 args:
                      node-id               the node id to resolve the value
                      property              string property name
                      evaluation-context    the evaluation context"
  [node-type-kw f]
  {:pre [(qualified-keyword? node-type-kw) (ifn? f)]}
  (ext-property-getter node-type-kw f))

(register-property-getter!
  :editor.code.resource/CodeEditorResourceNode
  (fn CodeEditorResourceNode-getter [node-id property evaluation-context]
    (case property
      "text" #(coll/join-to-string \newline (g/node-value node-id :lines evaluation-context))
      nil)))

(register-property-getter!
  :editor.resource/ResourceNode
  (fn ResourceNode-getter [node-id property evaluation-context]
    (case property
      "path" #(resource/resource->proj-path (g/node-value node-id :resource evaluation-context))
      nil)))

(register-property-getter!
  :editor.tile-source/TileSourceNode
  (fn TileSourceNode-getter [node-id property evaluation-context]
    (case property
      "tile_collision_groups"
      (fn get-tile-collision-groups []
        (coll/pair-map-by
          (comp inc key) ;; 0-indexed to 1-indexed
          #(g/node-value (val %) :id evaluation-context)
          (g/node-value node-id :tile->collision-group-node evaluation-context)))
      nil)))

(register-property-getter!
  :editor.tile-map/LayerNode
  (fn LayerNode-getter [node-id property evaluation-context]
    (case property
      "tiles" #(tile-map/make (g/node-value node-id :cell-map evaluation-context))
      nil)))

(register-property-getter!
  :editor.particlefx/ModifierNode
  (fn ModifierNode-getter [node-id property evaluation-context]
    (case property
      "type" #(g/node-value node-id :type evaluation-context)
      nil)))

(def ^:private game-project-setting-path-regex #"^[a-zA-Z_][a-zA-Z0-9_]*\.[a-zA-Z_][a-zA-Z0-9_]*$")

(register-property-getter!
  :editor.game-project/GameProjectNode
  (fn GameProjectNode-getter [node-id property evaluation-context]
    (when (re-matches game-project-setting-path-regex property)
      (let [path (string/split property #"\." 2)]
        #(game-project/get-setting node-id path evaluation-context)))))

(register-property-getter!
  ::outline/OutlineNode
  (fn OutlineNode-getter [node-id property evaluation-context]
    (when-let [outline-property (outline-property node-id property evaluation-context)]
      (when-let [to (-> outline-property properties/property-edit-type-id edit-type-id->value-converter :to)]
        #(some-> (properties/value outline-property) to)))))

(defn ext-value-getter
  "Create 0-arg fn that produces node property value

  Returns nil if there is no getter for node-id-or-resource+property

  Args:
    node-id-or-resource    resolved node id or folder resource
    property               string property name
    project                the project node id
    evaluation-context     used evaluation context"
  [node-id-or-resource property project evaluation-context]
  (if (resource/resource? node-id-or-resource)
    (case property
      "path" #(resource/proj-path node-id-or-resource)
      "children" #(mapv resource/proj-path (resource/children node-id-or-resource))
      nil)
    (let [node-id node-id-or-resource
          {:keys [basis]} evaluation-context
          node-type (g/node-type* basis node-id)
          workspace (project/workspace project evaluation-context)]
      (or (attachment/find-alternative workspace node-id #((ext-property-getter (:k (g/node-type* basis %))) % property evaluation-context) evaluation-context)
          (case property
            "type" (when-let [type-name (node-types/->name node-type)]
                     (constantly type-name))
            (let [list-kw (property->prop-kw property)]
              (when (attachment/defined? workspace node-id list-kw evaluation-context)
                #(mapv rt/wrap-userdata (attachment/get workspace node-id list-kw evaluation-context)))))))))

;; endregion

;; region set

(defonce ^:private ext-property-setter
  (make-inheritance-chain))

(defn register-property-setter!
  "Register a setter associated with a node type

  Args:
    node-type-kw    keyword of a graph node type
    f               a function of 5 args:
                      node-id               the node id to resolve the value
                      property              string property name
                      rt                    editor script runtime
                      project               the project node id
                      evaluation-context    the evaluation context
                    the function should return either a 1-arg function from lua
                    value to transaction steps or nil"
  [node-type-kw f]
  {:pre [(qualified-keyword? node-type-kw) (ifn? f)]}
  (ext-property-setter node-type-kw f))

(register-property-setter!
  :editor.code.resource/CodeEditorResourceNode
  (fn CodeEditorResourceNode-setter [node-id property rt _ _]
    (case property
      "text" (fn set-text [lua-value]
               [(g/set-property node-id :modified-lines (code.util/split-lines (rt/->clj rt coerce/string lua-value)))
                (g/update-property node-id :invalidated-rows conj 0)
                (g/set-property node-id :cursor-ranges [#code/range[[0 0] [0 0]]])
                (g/set-property node-id :regions [])])
      nil)))

(def ^:private tile-collision-group-coercer
  (coerce/map-of coerce/integer coerce/string))

(register-property-setter!
  :editor.tile-source/TileSourceNode
  (fn TileSourceNode-setter [node-id property rt project _]
    (case property
      "tile_collision_groups"
      (fn set-tile-collision-groups [lua-value]
        (let [one-indexed-tile-index->collision-group-id (rt/->clj rt tile-collision-group-coercer lua-value)]
          (g/expand-ec
            (fn [evaluation-context]
              (let [workspace (project/workspace project evaluation-context)
                    collision-id->node-id
                    (coll/pair-map-by
                      #(g/node-value % :id evaluation-context)
                      (attachment/get workspace node-id :collision-groups evaluation-context))
                    new-tile->collision-group-node
                    (coll/pair-map-by
                      (comp dec key) ;; 1-indexed to 0-indexed
                      (fn [e]
                        (let [id (val e)]
                          (or (collision-id->node-id id)
                              (throw (LuaError. (format "Collision group \"%s\" is not defined in the tilesource" id))))))
                      one-indexed-tile-index->collision-group-id)]
                (g/set-property node-id :tile->collision-group-node new-tile->collision-group-node))))))
      nil)))

(def ^:private tiles-coercer
  (coerce/wrap-with-pred coerce/userdata #(instance? Tiles %) "is not a tiles datatype"))

(register-property-setter!
  :editor.tile-map/LayerNode
  (fn LayerNode-setter [node-id property rt _ _]
    (case property
      "tiles" (fn set-tiles [lua-value]
                (g/set-property node-id :cell-map ((rt/->clj rt tiles-coercer lua-value))))
      nil)))

(register-property-setter!
  :editor.particlefx/EmitterNode
  (fn EmitterNode-setter [node-id property rt _ _]
    (case property
      "animation"
      ;; set property directly instead of going through outline so that there is no
      ;; order dependency between setting animation and material
      (fn set-emitter-animation [lua-value]
        (g/set-property node-id :animation (rt/->clj rt coerce/string lua-value)))

      nil)))

(def ^:private modifier-type-coercer
  (apply coerce/enum (mapv first (protobuf/enum-values Particle$ModifierType))))

(register-property-setter!
  :editor.particlefx/ModifierNode
  (fn ModifierNode-setter [node-id property rt _ _]
    (case property
      "type"
      ;; hidden in the outline, but we want to set it from editor scripts
      (fn set-modifier-type [lua-value]
        (g/set-property node-id :type (rt/->clj rt modifier-type-coercer lua-value)))

      nil)))

(defn- coercing-meta-setting-converter [coercer]
  (fn converter [lua-value rt _project _evaluation-context]
    (rt/->clj rt coercer lua-value)))

(defn meta-setting-converter [meta-setting]
  (if (not meta-setting)
    nil
    (if-let [options (:options meta-setting)]
      (coercing-meta-setting-converter (apply coerce/enum (mapv first options)))
      (case (:type meta-setting)
        :string
        (coercing-meta-setting-converter coerce/string)

        :number
        (coercing-meta-setting-converter (coerce/wrap-transform coerce/number double))

        :integer
        (coercing-meta-setting-converter coerce/integer)

        :boolean
        (coercing-meta-setting-converter coerce/boolean)

        :url
        (coercing-meta-setting-converter
          (-> coerce/string
              (coerce/wrap-transform #(try (URI. %) (catch URISyntaxException _)))
              (coerce/wrap-with-pred some? "is not a valid URL")))

        :resource
        (fn [lua-value rt project evaluation-context]
          (resource-converter lua-value rt {:ext (:filter meta-setting)} project evaluation-context))

        (:list :comma-separated-list)
        (when-let [element-converter (meta-setting-converter (:element meta-setting))]
          (fn list-converter [lua-value rt project evaluation-context]
            (rt/->clj
              rt
              (coerce/vector-of
                (-> coerce/untouched
                    (coerce/wrap-transform
                      (fn [lua-element]
                        (element-converter lua-element rt project evaluation-context)))))
              lua-value)))

        nil))))

(register-property-setter!
  :editor.game-project/GameProjectNode
  (fn GameProjectNode-setter [node-id property rt project evaluation-context]
    (when (re-matches game-project-setting-path-regex property)
      (when-let [path (string/split property #"\." 2)]
        (let [{:keys [meta-settings form-ops]} (g/node-value node-id :form-data evaluation-context)]
          (when-let [converter (meta-setting-converter (settings-core/get-meta-setting meta-settings path))]
            #(form/set-value form-ops path (converter % rt project evaluation-context))))))))

(register-property-setter!
  ::outline/OutlineNode
  (fn OutlineNode-setter [node-id property rt project evaluation-context]
    (when-let [outline-property (outline-property node-id property evaluation-context)]
      (when-not (properties/read-only? outline-property)
        (let [edit-type (properties/property-edit-type outline-property)]
          (when-let [from (-> edit-type
                              properties/edit-type-id
                              edit-type-id->value-converter
                              :from)]
            #(properties/set-value evaluation-context
                                   outline-property
                                   (from % rt edit-type project evaluation-context))))))))

(defn ext-lua-value-setter
  "Create 1-arg fn from new lua value to transaction steps setting the property

  Returns nil if there is no setter for the node-id+property pair"
  [node-id property rt project evaluation-context]
  (attachment/find-alternative
    (project/workspace project evaluation-context)
    node-id
    #((ext-property-setter (node-id->type-keyword % evaluation-context)) % property rt project evaluation-context)
    evaluation-context))

;; endregion

;; region reset

(defonce ^:private ext-property-resetter
  (make-inheritance-chain))

(defn register-property-resetter!
  "Register a setter associated with a node type

  Args:
    node-type-kw    keyword of a graph node type
    f               a function of 3 args:
                      node-id               the node id to resolve the value
                      property              string property name
                      evaluation-context    the evaluation context
                    the function should return either a 0-arg function that
                    creates transaction steps or nil"
  [node-type-kw f]
  {:pre [(qualified-keyword? node-type-kw) (ifn? f)]}
  (ext-property-resetter node-type-kw f))

(register-property-resetter!
  ::outline/OutlineNode
  (fn OutlineNode-resetter [node-id property evaluation-context]
    (when-let [property (outline-property node-id property evaluation-context)]
      (when (and (not (properties/read-only? property))
                 (properties/overridden-uncoalesced? property))
        #(properties/clear-override-uncoalesced property)))))

(def ^:private can-reset-args-coercer
  (coerce/regex :node node-id-or-path-coercer :property coerce/string))

(defn make-ext-can-reset-fn [project]
  (rt/varargs-lua-fn ext-can-reset [{:keys [rt evaluation-context]} varargs]
    (let [{:keys [node property]} (rt/->clj rt can-reset-args-coercer varargs)
          node-id-or-resource (resolve-node-id-or-path node project evaluation-context)]
      (and (not (resource/resource? node-id-or-resource))
           (let [node-id node-id-or-resource]
             (some? ((ext-property-resetter (node-id->type-keyword node-id evaluation-context)) node-id property evaluation-context)))))))

(def ^:private reset-args-coercer
  (coerce/regex :node node-id-or-path-coercer :property coerce/string))

(defn make-ext-reset-fn [project]
  (rt/varargs-lua-fn ext-reset [{:keys [rt evaluation-context]} varargs]
    (let [{:keys [node property]} (rt/->clj rt reset-args-coercer varargs)
          node-id (node-id-or-path->node-id node project evaluation-context)]
      (if-let [resetter ((ext-property-resetter (node-id->type-keyword node-id evaluation-context)) node-id property evaluation-context)]
        (-> (resetter)
            (with-meta {:type :transaction-step})
            (rt/wrap-userdata "editor.tx.reset(...)"))
        (throw (LuaError. (format "Can't reset property \"%s\" of %s"
                                  property
                                  (name (node-id->type-keyword node-id evaluation-context)))))))))

;; endregion

;; region attach

;; SDK api
(defn attachment->set-tx-steps [attachment child-node-id rt project evaluation-context]
  (coll/mapcat
    (fn [[property lua-value]]
      (if-let [setter (ext-lua-value-setter child-node-id property rt project evaluation-context)]
        (setter lua-value)
        (throw (LuaError. (format "Can't set property \"%s\" of %s"
                                  property
                                  (name (node-id->type-keyword child-node-id evaluation-context)))))))
    attachment))

;; SDK api
(defmulti init-attachment
  (fn init-attachment-dispatch-fn [_evaluation-context _rt _project _parent-node-id child-node-type _child-node-id _attachment]
    (:k child-node-type)))

(defmethod init-attachment :default [evaluation-context rt project _ _ child-node-id attachment]
  (attachment->set-tx-steps attachment child-node-id rt project evaluation-context))

(def ^:private default-new-animation-name-lua-value (rt/->lua "New Animation"))
(defmethod init-attachment :editor.atlas/AtlasAnimation [evaluation-context rt project _ _ child-node-id attachment]
  (-> attachment
      (util/provide-defaults
        "id" default-new-animation-name-lua-value)
      (attachment->set-tx-steps child-node-id rt project evaluation-context)))

(def ^:private default-start-tile-lua-value (rt/->lua 1))
(def ^:private default-end-tile-lua-value (rt/->lua 1))
(defmethod init-attachment :editor.tile-source/TileAnimationNode [evaluation-context rt project _ _ child-node-id attachment]
  (-> attachment
      (util/provide-defaults
        "id" default-new-animation-name-lua-value
        "start_tile" default-start-tile-lua-value
        "end_tile" default-end-tile-lua-value)
      (attachment->set-tx-steps child-node-id rt project evaluation-context)))

(defmethod init-attachment :editor.tile-source/CollisionGroupNode [evaluation-context rt project _ _ child-node-id attachment]
  (-> attachment
      (util/provide-defaults
        "id" (rt/->lua
               (id/gen "collision_group"
                       (collision-groups/collision-groups
                         (g/node-value project :collision-groups-data evaluation-context)))))
      (attachment->set-tx-steps child-node-id rt project evaluation-context)))

(defmethod init-attachment :editor.tile-map/LayerNode [evaluation-context rt project parent-node-id _ child-node-id attachment]
  (-> attachment
      (util/provide-defaults
        "id" (rt/->lua (id/gen "layer" (g/node-value parent-node-id :layer-ids evaluation-context))))
      (attachment->set-tx-steps child-node-id rt project evaluation-context)))

(def ^:private default-emitter-mode (rt/->lua :play-mode-loop))
(def ^:private default-emitter-space (rt/->lua :emission-space-world))
(def ^:private default-emitter-type (rt/->lua :emitter-type-2dcone))
(def ^:private default-emitter-max-particle-count (rt/->lua 128))
(def ^:private default-emitter-tile-source (rt/->lua "/builtins/graphics/particle_blob.tilesource"))
(def ^:private default-emitter-animation (rt/->lua "anim"))
(def ^:private default-emitter-material (rt/->lua "/builtins/materials/particlefx.material"))
(def ^:private default-emitter-curve-1 (rt/->lua {:points [{:x 0 :y 1 :tx 1 :ty 0}]}))
(def ^:private default-emitter-curve-10 (rt/->lua {:points [{:x 0 :y 10 :tx 1 :ty 0}]}))

(defmethod init-attachment :editor.particlefx/EmitterNode [evaluation-context rt project parent-node-id _ child-node-id attachment]
  (-> attachment
      (util/provide-defaults
        "id" (rt/->lua (id/gen "emitter" (g/node-value parent-node-id :ids evaluation-context)))
        "mode" default-emitter-mode
        "space" default-emitter-space
        "type" default-emitter-type
        "max_particle_count" default-emitter-max-particle-count
        "material" default-emitter-material
        "tile_source" default-emitter-tile-source
        "animation" default-emitter-animation

        "emitter_key_particle_red" default-emitter-curve-1
        "emitter_key_particle_green" default-emitter-curve-1
        "emitter_key_particle_blue" default-emitter-curve-1
        "emitter_key_particle_alpha" default-emitter-curve-1

        "emitter_key_particle_life_time" default-emitter-curve-1
        "emitter_key_particle_size" default-emitter-curve-1
        "emitter_key_particle_speed" default-emitter-curve-1
        "emitter_key_spawn_rate" default-emitter-curve-10

        "particle_key_red" default-emitter-curve-1
        "particle_key_green" default-emitter-curve-1
        "particle_key_blue" default-emitter-curve-1
        "particle_key_alpha" default-emitter-curve-1

        "particle_key_scale" default-emitter-curve-1)
      (attachment->set-tx-steps child-node-id rt project evaluation-context)))

(def ^:private default-modifier-type (rt/->lua :modifier-type-acceleration))
(defmethod init-attachment :editor.particlefx/ModifierNode [evaluation-context rt project _ _ child-node-id attachment]
  (-> attachment
      (util/provide-defaults
        "type" default-modifier-type)
      (attachment->set-tx-steps child-node-id rt project evaluation-context)))

(def ^:private default-sphere-diameter (rt/->lua 20.0))
(defmethod init-attachment :editor.collision-object/SphereShape [evaluation-context rt project _ _ child-node-id attachment]
  (concat
    (g/set-property child-node-id :shape-type :type-sphere)
    (-> attachment
        (util/provide-defaults
          "diameter" default-sphere-diameter)
        (attachment->set-tx-steps child-node-id rt project evaluation-context))))

(def ^:private default-box-dimensions (rt/->lua [20.0 20.0 20.0]))
(defmethod init-attachment :editor.collision-object/BoxShape [evaluation-context rt project _ _ child-node-id attachment]
  (concat
    (g/set-property child-node-id :shape-type :type-box)
    (-> attachment
        (util/provide-defaults
          "dimensions" default-box-dimensions)
        (attachment->set-tx-steps child-node-id rt project evaluation-context))))

(def ^:private default-capsule-diameter (rt/->lua 20.0))
(def ^:private default-capsule-height (rt/->lua 40.0))
(defmethod init-attachment :editor.collision-object/CapsuleShape [evaluation-context rt project _ _ child-node-id attachment]
  (concat
    (g/set-property child-node-id :shape-type :type-capsule)
    (-> attachment
        (util/provide-defaults
          "diameter" default-capsule-diameter
          "height" default-capsule-height)
        (attachment->set-tx-steps child-node-id rt project evaluation-context))))

(defmethod init-attachment :editor.gui/LayerNode [evaluation-context rt project parent-node-id _ child-node-id attachment]
  (let [layers-node (gui-attachment/scene-node->layers-node (:basis evaluation-context) parent-node-id)]
    (concat
      (g/set-property child-node-id :child-index (gui-attachment/next-child-index layers-node evaluation-context))
      (-> attachment
          (util/provide-defaults
            "name" (rt/->lua (id/gen "layer" (g/node-value layers-node :name-counts evaluation-context))))
          (attachment->set-tx-steps child-node-id rt project evaluation-context)))))

;; SDK api
(defn gen-gui-component-name [attachment resource-key container-node-fn rt parent-node-id evaluation-context]
  (rt/->lua
    (id/gen
      (if-let [lua-material-str (get attachment resource-key)]
        (FilenameUtils/getBaseName (rt/->clj rt coerce/string lua-material-str))
        resource-key)
      (-> evaluation-context
          :basis
          (container-node-fn parent-node-id)
          (g/node-value :name-counts evaluation-context)))))

(defmethod init-attachment :editor.gui/MaterialNode [evaluation-context rt project parent-node-id _ child-node-id attachment]
  (-> attachment
      (util/provide-defaults
        "name" (gen-gui-component-name attachment "material" gui-attachment/scene-node->materials-node rt parent-node-id evaluation-context))
      (attachment->set-tx-steps child-node-id rt project evaluation-context)))

(defmethod init-attachment :editor.gui/ParticleFXResource [evaluation-context rt project parent-node-id _ child-node-id attachment]
  (-> attachment
      (util/provide-defaults
        "name" (gen-gui-component-name attachment "particlefx" gui-attachment/scene-node->particlefx-resources-node rt parent-node-id evaluation-context))
      (attachment->set-tx-steps child-node-id rt project evaluation-context)))

(defmethod init-attachment :editor.gui/TextureNode [evaluation-context rt project parent-node-id _ child-node-id attachment]
  (-> attachment
      (util/provide-defaults
        "name" (gen-gui-component-name attachment "texture" gui-attachment/scene-node->textures-node rt parent-node-id evaluation-context))
      (attachment->set-tx-steps child-node-id rt project evaluation-context)))

(defmethod init-attachment :editor.gui/LayoutNode [evaluation-context rt project parent-node-id _ child-node-id attachment]
  (if-let [lua-name (get attachment "name")]
    (concat
      (g/set-property child-node-id :name (rt/->clj rt (apply coerce/enum (g/node-value parent-node-id :unused-display-profiles evaluation-context)) lua-name))
      (-> attachment
          (dissoc "name")
          (attachment->set-tx-steps child-node-id rt project evaluation-context)))
    (throw (LuaError. "layout name is required"))))

(defmethod init-attachment :editor.gui/FontNode [evaluation-context rt project parent-node-id _ child-node-id attachment]
  (-> attachment
      (util/provide-defaults
        "name" (gen-gui-component-name attachment "font" gui-attachment/scene-node->fonts-node rt parent-node-id evaluation-context))
      (attachment->set-tx-steps child-node-id rt project evaluation-context)))

(def ^:private attachment-coercer (coerce/map-of coerce/string coerce/untouched))
(def ^:private attachments-coercer (coerce/vector-of attachment-coercer))

;; Attachment -> node tree creation order:
;; 1. extract-node-type — figure out the node type of node to create
;; 2. create-extra-nodes — create extra nodes that should be a part of the node
;;    tree and influence node initialization. For example, if a node type is
;;    configured to define an alternative (attachment/define-alternative), this
;;    is the place to create the alternative node, since its existence defines
;;    the existence of list properties
;; 3. init-attachment — strip the attachment of list properties, and apply
;;    the remaining entries as property setters
;; 4. add-child-attachments — take all list properties from the attachment, and
;;    recursively attach the list item attachments to the created node.

(defmulti extract-node-type
  "Given an attachment, return a tuple of attachment+node-type

  The returned attachment might be modified, typically stripped of the type key"
  (fn extract-node-type-dispatch-fn [_rt _attachment workspace node-id list-kw evaluation-context]
    (let [node-id (attachment/list-node-id workspace node-id list-kw evaluation-context)]
      [(g/node-type-kw (:basis evaluation-context) node-id) list-kw])))

(defmethod extract-node-type :default [rt attachment workspace node-id list-kw evaluation-context]
  (let [possible-node-types (attachment/child-node-types workspace node-id list-kw evaluation-context)
        n (count possible-node-types)]
    (assert (pos? n) "Check for attachment/editable? before extracting node type")
    (if (= 1 n)
      (coll/pair attachment (reduce-kv (fn [_ k _] (reduced k)) nil possible-node-types))
      (if-let [lua-type (attachment "type")]
        (let [node-type-name (rt/->clj rt coerce/string lua-type)
              node-type (node-types/->type node-type-name)]
          (if (and node-type (contains? possible-node-types node-type))
            (coll/pair (dissoc attachment "type") node-type)
            (throw (LuaError. (str node-type-name " is not " (->> possible-node-types keys (keep node-types/->name) sort (util/join-words ", " " or ")))))))
        (throw (LuaError. "type is required"))))))

(defn- init-attachment-without-list-properties [evaluation-context rt project workspace attachment parent-node-id child-node-id]
  (init-attachment
    evaluation-context
    rt
    project
    parent-node-id
    (g/node-type* (:basis evaluation-context) child-node-id)
    child-node-id
    (reduce-kv
      (fn [acc property _]
        (let [list-kw (property->prop-kw property)]
          (if (attachment/defined? workspace child-node-id list-kw evaluation-context)
            (dissoc acc property)
            acc)))
      attachment
      attachment)))

(defmulti create-extra-nodes
  (fn create-extra-nodes-dispatch-fn [evaluation-context _rt _project _workspace _attachment node-id]
    (g/node-type-kw (:basis evaluation-context) node-id)))

(defmethod create-extra-nodes :default [_evaluation-context _rt _project _workspace _attachment _node-id])

(defn- construct-and-init-attachment [rt project workspace attachment parent-node-id child-node-id]
  (concat
    (g/expand-ec create-extra-nodes rt project workspace attachment child-node-id)
    (g/expand-ec init-attachment-without-list-properties rt project workspace attachment parent-node-id child-node-id)))

(defn- add-child-attachments [evaluation-context rt project workspace attachment node-id]
  (persistent!
    (reduce-kv
      (fn [acc property lua-value]
        (let [list-kw (property->prop-kw property)]
          (if (attachment/defined? workspace node-id list-kw evaluation-context)
            (if (attachment/editable? workspace node-id list-kw evaluation-context)
              (reduce
                (fn [acc attachment]
                  (let [[attachment node-type] (extract-node-type rt attachment workspace node-id list-kw evaluation-context)]
                    (conj! acc (attachment/add
                                 workspace node-id list-kw node-type
                                 (partial construct-and-init-attachment rt project workspace attachment)
                                 (partial g/expand-ec add-child-attachments rt project workspace attachment)))))
                acc
                (rt/->clj rt attachments-coercer lua-value))
              (throw (LuaError. (format "\"%s\" is read-only" property))))
            acc)))
      (transient [])
      attachment)))

(def ^:private add-args-coercer
  (coerce/regex :node node-id-or-path-coercer
                :property coerce/string
                :attachment attachment-coercer))

(defn make-ext-add-fn [project]
  (rt/varargs-lua-fn ext-add [{:keys [rt evaluation-context]} varargs]
    (let [{:keys [node property attachment]} (rt/->clj rt add-args-coercer varargs)
          workspace (project/workspace project evaluation-context)
          node-id (node-id-or-path->node-id node project evaluation-context)
          list-kw (property->prop-kw property)]
      (when-not (attachment/defined? workspace node-id list-kw evaluation-context)
        (throw (LuaError. (format "\"%s\" is undefined" property))))
      (when-not (attachment/editable? workspace node-id list-kw evaluation-context)
        (throw (LuaError. (format "\"%s\" is read-only" property))))
      (let [[attachment node-type] (extract-node-type rt attachment workspace node-id list-kw evaluation-context)]
        (-> (attachment/add
              workspace node-id list-kw node-type
              (partial construct-and-init-attachment rt project workspace attachment)
              (partial g/expand-ec add-child-attachments rt project workspace attachment))
            (with-meta {:type :transaction-step})
            (rt/wrap-userdata "editor.tx.add(...)"))))))

(def ^:private can-add-args-coercer
  (coerce/regex :node node-id-or-path-coercer :property coerce/string))

(defn make-ext-can-add-fn [project]
  (rt/varargs-lua-fn ext-can-add [{:keys [rt evaluation-context]} varargs]
    (let [{:keys [node property]} (rt/->clj rt can-add-args-coercer varargs)
          node-id-or-resource (resolve-node-id-or-path node project evaluation-context)
          list-kw (property->prop-kw property)
          workspace (project/workspace project evaluation-context)]
      (and (not (resource/resource? node-id-or-resource))
           (attachment/editable? workspace node-id-or-resource list-kw evaluation-context)))))

(def ^:private clear-args-coercer
  (coerce/regex :node node-id-or-path-coercer :property coerce/string))

(defn make-ext-clear-fn [project]
  (rt/varargs-lua-fn ext-clear [{:keys [rt evaluation-context]} varargs]
    (let [{:keys [node property]} (rt/->clj rt clear-args-coercer varargs)
          workspace (project/workspace project evaluation-context)
          node-id (node-id-or-path->node-id node project evaluation-context)
          list-kw (property->prop-kw property)]
      (when-not (attachment/defined? workspace node-id list-kw evaluation-context)
        (throw (LuaError. (format "\"%s\" is undefined" property))))
      (when-not (attachment/editable? workspace node-id list-kw evaluation-context)
        (throw (LuaError. (format "\"%s\" is read-only" property))))
      (-> (attachment/clear workspace node-id list-kw)
          (with-meta {:type :transaction-step})
          (rt/wrap-userdata "editor.tx.clear(...)")))))

(def ^:private remove-args-coercer
  (coerce/regex :node node-id-or-path-coercer :property coerce/string :child node-id-or-path-coercer))

(defn make-ext-remove-fn [project]
  (rt/varargs-lua-fn ext-remove [{:keys [rt evaluation-context]} varargs]
    (let [{:keys [node property child]} (rt/->clj rt remove-args-coercer varargs)
          workspace (project/workspace project evaluation-context)
          node-id (node-id-or-path->node-id node project evaluation-context)
          child-node-id (node-id-or-path->node-id child project evaluation-context)
          list-kw (property->prop-kw property)]
      (when-not (attachment/defined? workspace node-id list-kw evaluation-context)
        (throw (LuaError. (format "\"%s\" is undefined" property))))
      (when-not (attachment/editable? workspace node-id list-kw evaluation-context)
        (throw (LuaError. (format "\"%s\" is read-only" property))))
      (when-not (some #(= child-node-id %) (attachment/get workspace node-id list-kw evaluation-context))
        (throw (LuaError. (format "%s is not in the \"%s\" list of %s" child property node))))
      (-> (attachment/remove workspace node-id list-kw child-node-id)
          (with-meta {:type :transaction-step})
          (rt/wrap-userdata "editor.tx.remove(...)")))))

(def ^:private can-reorder-args-coercer
  (coerce/regex :node node-id-or-path-coercer :property coerce/string))

(defn make-ext-can-reorder-fn [project]
  (rt/varargs-lua-fn ext-can-reorder [{:keys [rt evaluation-context]} varargs]
    (let [{:keys [node property]} (rt/->clj rt can-reorder-args-coercer varargs)
          node-id-or-resource (resolve-node-id-or-path node project evaluation-context)
          workspace (project/workspace project evaluation-context)
          list-kw (property->prop-kw property)]
      (and (not (resource/resource? node-id-or-resource))
           (attachment/editable? workspace node-id-or-resource list-kw evaluation-context)
           (attachment/reorderable? workspace node-id-or-resource list-kw evaluation-context)))))

(def ^:private reorder-args-coercer
  (coerce/regex :node node-id-or-path-coercer
                :property coerce/string
                :children (coerce/vector-of node-id-or-path-coercer)))

(defn make-ext-reorder-fn [project]
  (rt/varargs-lua-fn ext-reorder [{:keys [rt evaluation-context]} varargs]
    (let [{:keys [node property children]} (rt/->clj rt reorder-args-coercer varargs)
          workspace (project/workspace project evaluation-context)
          node-id (node-id-or-path->node-id node project evaluation-context)
          list-kw (property->prop-kw property)
          reordered-child-node-ids (mapv #(node-id-or-path->node-id % project evaluation-context) children)]
      (when-not (attachment/defined? workspace node-id list-kw evaluation-context)
        (throw (LuaError. (format "\"%s\" is undefined" property))))
      (when-not (attachment/editable? workspace node-id list-kw evaluation-context)
        (throw (LuaError. (format "\"%s\" is read-only" property))))
      (when-not (attachment/reorderable? workspace node-id list-kw evaluation-context)
        (throw (LuaError. (format "\"%s\" is not reorderable" property))))
      (let [current-child-node-set (set (attachment/get workspace node-id list-kw evaluation-context))]
        (when (or (not (every? current-child-node-set reordered-child-node-ids))
                  (not (= (count current-child-node-set) (count reordered-child-node-ids))))
          (throw (LuaError. "Reordered child nodes are not the same as current child nodes"))))
      (-> (attachment/reorder workspace node-id list-kw reordered-child-node-ids)
          (with-meta {:type :transaction-step})
          (rt/wrap-userdata "editor.tx.reorder(...)")))))

;; endregion