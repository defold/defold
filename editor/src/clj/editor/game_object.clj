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

(ns editor.game-object
  (:require [clojure.set :as set]
            [clojure.string :as str]
            [dynamo.graph :as g]
            [editor.app-view :as app-view]
            [editor.build-target :as bt]
            [editor.defold-project :as project]
            [editor.geom :as geom]
            [editor.gl.pass :as pass]
            [editor.graph-util :as gu]
            [editor.handler :as handler]
            [editor.outline :as outline]
            [editor.properties :as properties]
            [editor.protobuf :as protobuf]
            [editor.resource :as resource]
            [editor.resource-dialog :as resource-dialog]
            [editor.resource-node :as resource-node]
            [editor.scene :as scene]
            [editor.scene-tools :as scene-tools]
            [editor.sound :as sound]
            [editor.validation :as validation]
            [editor.workspace :as workspace]
            [internal.util :as util]
            [service.log :as log])
  (:import [com.dynamo.gameobject.proto GameObject$PrototypeDesc]
           [com.dynamo.gamesys.proto Sound$SoundDesc]
           [java.io StringReader]
           [javax.vecmath Matrix4d Vector3d]
           [org.apache.commons.io FilenameUtils]))

(set! *warn-on-reflection* true)

(def game-object-icon "icons/32/Icons_06-Game-object.png")
(def unknown-icon "icons/32/Icons_29-AT-Unknown.png")

(defn- gen-ref-ddf [id position rotation scale source-resource ddf-properties]
  {:id id
   :position position
   :rotation rotation
   :scale scale
   :component (resource/resource->proj-path source-resource)
   :properties ddf-properties})

(defn- gen-embed-ddf [id position rotation scale save-data]
  {:id id
   :type (or (and (:resource save-data) (:ext (resource/resource-type (:resource save-data))))
             "unknown")
   :position position
   :rotation rotation
   :scale scale
   :data (or (:content save-data) "")})

(defn- build-raw-sound [resource dep-resources user-data]
  (let [pb (:pb user-data)
        pb (assoc pb :sound (resource/proj-path (second (first dep-resources))))]
    {:resource resource :content (protobuf/map->bytes Sound$SoundDesc pb)}))

(defn- wrap-if-raw-sound [target _node-id]
  (let [resource (:resource (:resource target))
        source-path (resource/proj-path resource)
        ext (resource/type-ext resource)]
    (if (sound/supported-audio-formats ext)
      (let [workspace (project/workspace (project/get-project _node-id))
            res-type  (workspace/get-resource-type workspace "sound")
            pb        {:sound source-path}
            target    (bt/with-content-hash
                        {:node-id _node-id
                         :resource (workspace/make-build-resource (resource/make-memory-resource workspace res-type (protobuf/map->str Sound$SoundDesc pb)))
                         :build-fn build-raw-sound
                         :deps [target]})]
        target)
      target)))

(defn- source-outline-subst [err]
  (if-let [resource (get-in err [:user-data :resource])]
    (let [rt (resource/resource-type resource)
          label (or (:label rt) (:ext rt) "unknown")
          icon (or (:icon rt) unknown-icon)]
      {:node-id (:node-id err)
       :node-outline-key label
       :label label
       :icon icon})
    {:node-id -1
     :node-outline-key ""
     :icon ""
     :label ""}))

(def ^:private identity-transform-properties
  {:position [(float 0.0) (float 0.0) (float 0.0)]
   :rotation [(float 0.0) (float 0.0) (float 0.0) (float 1.0)]
   :scale [(float 1.0) (float 1.0) (float 1.0)]})

(defn- supported-transform-properties [component-resource-type]
  (assert (some? component-resource-type))
  (if-not (contains? (:tags component-resource-type) :component)
    #{}
    (let [supported-properties (-> component-resource-type :tag-opts :component :transform-properties)]
      (assert (set? supported-properties))
      (assert (every? (partial contains? identity-transform-properties) supported-properties))
      supported-properties)))

(defn- select-transform-properties
  "Used to strip unsupported transform properties when loading a component.
  Unsupported properties will be initialized to the identity transform.
  If the resource type is unknown to us, keep all transform properties."
  [component-resource-type component]
  (merge identity-transform-properties
         (select-keys component
                      (if (some? component-resource-type)
                        (supported-transform-properties component-resource-type)
                        (keys identity-transform-properties)))))

(g/defnk produce-component-transform-properties
  "Determines which transform properties we allow the user to edit using the
  Property Editor. This also affects which manipulators work with the component.
  If the resource type is unknown to us, show no transform properties."
  [source-resource]
  (if-let [component-resource-type (some-> source-resource resource/resource-type)]
    (supported-transform-properties component-resource-type)
    #{}))

(g/defnk produce-component-properties
  [_declared-properties source-properties transform-properties]
  (assert (set? transform-properties))
  (-> _declared-properties
      (update :properties (fn [properties]
                            (let [stripped-transform-properties (remove transform-properties (keys identity-transform-properties))
                                  component-node-properties (apply dissoc properties stripped-transform-properties)
                                  source-resource-properties (:properties source-properties)]
                              (merge-with (fn [_ _]
                                            (let [duplicate-keys (set/intersection (set (keys component-node-properties))
                                                                                   (set (keys source-resource-properties)))]
                                              (throw (ex-info (str "Conflicting properties in source resource: " duplicate-keys)
                                                              {:duplicate-keys duplicate-keys}))))
                                          component-node-properties
                                          source-resource-properties))))
      (update :display-order (fn [display-order]
                               (vec (distinct (concat display-order (:display-order source-properties))))))))

(g/defnk produce-component-build-targets [_node-id build-resource ddf-message transform resource-property-build-targets source-build-targets]
  ;; Create a build-target for the referenced or embedded component. Also tag on
  ;; :instance-data with the overrides for this instance. This will later be
  ;; extracted and compiled into the GameObject - the overrides do not end up in
  ;; the resulting component binary.
  (when-some [source-build-target (first source-build-targets)]
    (let [go-props-with-source-resources (:properties ddf-message)]
      (if-some [errors (not-empty (keep :error go-props-with-source-resources))]
        (g/error-aggregate errors :_node-id _node-id :_label :build-targets)
        (let [[go-props go-prop-dep-build-targets] (properties/build-target-go-props resource-property-build-targets go-props-with-source-resources)
              build-target (-> source-build-target
                               (assoc :resource build-resource)
                               (wrap-if-raw-sound _node-id))
              build-target (assoc build-target
                             :instance-data {:resource (:resource build-target)
                                             :transform transform
                                             :property-deps go-prop-dep-build-targets
                                             :instance-msg (if (seq go-props)
                                                             (assoc ddf-message :properties go-props)
                                                             ddf-message)})]
          [(bt/with-content-hash build-target)])))))

(g/defnode ComponentNode
  (inherits scene/SceneNode)
  (inherits outline/OutlineNode)

  (property id g/Str
            (dynamic error (g/fnk [_node-id id id-counts]
                                  (or (validation/prop-error :fatal _node-id :id validation/prop-empty? id "Id")
                                      (validation/prop-error :fatal _node-id :id (partial validation/prop-id-duplicate? id-counts) id))))
            (dynamic read-only? (g/fnk [_node-id]
                                  (g/override? _node-id))))
  (property url g/Str
            (value (g/fnk [base-url id] (format "%s#%s" (or base-url "") id)))
            (dynamic read-only? (g/constantly true)))

  (display-order [:id :url :path scene/SceneNode])

  (input source-resource resource/Resource)
  (input source-properties g/Properties :substitute {:properties {}})
  (input scene g/Any)
  (input source-build-targets g/Any)
  (input resource-property-build-targets g/Any)
  (input base-url g/Str)
  (input id-counts g/Any)

  (input source-outline outline/OutlineData :substitute source-outline-subst)

  (output transform-properties g/Any produce-component-transform-properties)
  (output component-id g/IdPair (g/fnk [_node-id id] [id _node-id]))
  (output node-outline outline/OutlineData :cached
    (g/fnk [_node-id id source-outline source-properties source-resource]
      (let [source-outline (or source-outline {:icon unknown-icon})
            source-id (when-let [source-id (:node-id source-outline)]
                        (and (not= source-id -1) source-id))
            overridden? (boolean (some (fn [[_ p]] (contains? p :original-value)) (:properties source-properties)))]
        (-> {:node-id _node-id
             :node-outline-key id
             :label id
             :icon (or (not-empty (:icon source-outline)) unknown-icon)
             :outline-overridden? overridden?
             :children (:children source-outline)}
          (cond->
            (resource/openable-resource? source-resource) (assoc :link source-resource :outline-reference? true)
            source-id (assoc :alt-outline source-outline))))))
  (output ddf-message g/Any :abstract)
  (output scene g/Any :cached (g/fnk [_node-id id transform scene]
                                (if (some? scene)
                                  (let [transform (if-let [local-transform (:transform scene)]
                                                    (doto (Matrix4d. ^Matrix4d transform)
                                                      (.mul ^Matrix4d local-transform))
                                                    transform)
                                        updatable (some-> (:updatable scene)
                                                          (assoc :node-id _node-id))]
                                    ;; label has scale and thus transform, others have identity transform
                                    (cond-> (assoc (scene/claim-scene scene _node-id id)
                                                   :transform transform)
                                            updatable ((partial scene/map-scene #(assoc % :updatable updatable)))))
                                  ;; This handles the case of no scene
                                  ;; from actual component - typically
                                  ;; bad data. Covered by for instance
                                  ;; unknown_components.go in the test
                                  ;; project.
                                  {:node-id _node-id
                                   :transform transform
                                   :aabb geom/empty-bounding-box
                                   :renderable {:passes [pass/selection]}})))
  (output build-resource resource/Resource :abstract)
  (output build-targets g/Any :cached produce-component-build-targets)
  (output resource-property-build-targets g/Any (gu/passthrough resource-property-build-targets))
  (output _properties g/Properties :cached produce-component-properties))

(g/defnode EmbeddedComponent
  (inherits ComponentNode)

  (input embedded-resource-id g/NodeID)
  (input save-data g/Any :cascade-delete)
  (output ddf-message g/Any (g/fnk [id position rotation scale save-data]
                              (gen-embed-ddf id position rotation scale save-data)))
  (output build-resource resource/Resource (g/fnk [source-resource save-data]
                                                  (some-> source-resource
                                                     (assoc :data (:content save-data))
                                                     workspace/make-build-resource))))

;; -----------------------------------------------------------------------------
;; Currently some source resources have scale properties. This was done so
;; that particular component types such as the Label component could support
;; scaling. This is not ideal, since the scaling cannot differ between
;; instances of the component. We probably want to remove this and move the
;; scale attribute to the Component instance in the future.
;;
;; Here we delegate scaling to the embedded resource node. To support scaling,
;; the ResourceNode needs to implement both manip-scalable? and manip-scale.

(defmethod scene-tools/manip-scalable? ::EmbeddedComponent [node-id]
  (or (some-> (g/node-value node-id :embedded-resource-id) scene-tools/manip-scalable?)
      (contains? (g/node-value node-id :transform-properties) :scale)))

(defmethod scene-tools/manip-scale ::EmbeddedComponent [evaluation-context node-id ^Vector3d delta]
  (let [embedded-resource-id (g/node-value node-id :embedded-resource-id evaluation-context)]
    (cond
      (some-> embedded-resource-id scene-tools/manip-scalable?)
      (scene-tools/manip-scale evaluation-context embedded-resource-id delta)

      (contains? (g/node-value node-id :transform-properties evaluation-context) :scale)
      (let [[sx sy sz] (g/node-value node-id :scale evaluation-context)
            new-scale [(* sx (.x delta)) (* sy (.y delta)) (* sz (.z delta))]]
        (g/set-property node-id :scale (properties/round-vec new-scale)))

      :else
      nil)))

;; -----------------------------------------------------------------------------

(g/defnode ReferencedComponent
  (inherits ComponentNode)

  (property path g/Any
            (dynamic edit-type (g/fnk [source-resource]
                                      {:type resource/Resource
                                       :ext (some-> source-resource resource/resource-type :ext)
                                       :to-type (fn [v] (:resource v))
                                       :from-type (fn [r] {:resource r :overrides []})}))
            (value (g/fnk [source-resource ddf-properties]
                          {:resource source-resource
                           :overrides ddf-properties}))
            (set (fn [evaluation-context self old-value new-value]
                   (concat
                     (when-some [old-source (g/node-value self :source-id evaluation-context)]
                       (g/delete-node old-source))
                     (let [new-resource (:resource new-value)
                           resource-type (and new-resource (resource/resource-type new-resource))
                           project (project/get-project self)
                           override? (contains? (:tags resource-type) :overridable-properties)

                           [comp-node tx-data]
                           (if override?
                             (let [workspace (project/workspace project)]
                               (when-some [{connect-tx-data :tx-data comp-node :node-id} (project/connect-resource-node evaluation-context project new-resource self [])]
                                 [comp-node
                                  (concat
                                    connect-tx-data
                                    (g/override comp-node {:traverse-fn g/always-override-traverse-fn}
                                                (fn [evaluation-context id-mapping]
                                                  (let [or-comp-node (get id-mapping comp-node)
                                                        comp-props (:properties (g/node-value comp-node :_properties evaluation-context))]
                                                    (concat
                                                      (let [outputs (g/output-labels (:node-type (resource/resource-type new-resource)))]
                                                        (for [[from to] [[:_node-id :source-id]
                                                                         [:resource :source-resource]
                                                                         [:node-outline :source-outline]
                                                                         [:_properties :source-properties]
                                                                         [:scene :scene]
                                                                         [:build-targets :source-build-targets]
                                                                         [:resource-property-build-targets :resource-property-build-targets]]
                                                              :when (contains? outputs from)]
                                                          (g/connect or-comp-node from self to)))
                                                      (properties/apply-property-overrides workspace id-mapping comp-props (:overrides new-value)))))))]))
                             (let [old-resource (:resource old-value)
                                   new-resource (:resource new-value)
                                   connections [[:resource :source-resource]
                                                [:node-outline :source-outline]
                                                [:scene :scene]
                                                [:build-targets :source-build-targets]]
                                   disconnect-tx-data (when old-resource
                                                        (project/disconnect-resource-node evaluation-context project old-resource self connections))
                                   {connect-tx-data :tx-data comp-node :node-id} (when new-resource
                                                                                   (project/connect-resource-node evaluation-context project new-resource self connections))]
                               [comp-node
                                (concat disconnect-tx-data
                                        connect-tx-data)]))]
                       (concat
                         tx-data
                         (when-some [alter-referenced-component-fn (some-> resource-type :tag-opts :component :alter-referenced-component-fn)]
                           (alter-referenced-component-fn self comp-node)))))))
            (dynamic error (g/fnk [_node-id source-resource]
                                  (or (validation/prop-error :info _node-id :path validation/prop-nil? source-resource "Path")
                                      (validation/prop-error :fatal _node-id :path validation/prop-resource-not-exists? source-resource "Path")))))

  (input source-id g/NodeID :cascade-delete)
  (output build-resource resource/Resource (g/fnk [source-build-targets] (:resource (first source-build-targets))))
  (output ddf-properties g/Any :cached
          (g/fnk [source-properties]
                 (let [prop-order (into {} (map-indexed (fn [i k] [k i])) (:display-order source-properties))]
                   (->> source-properties
                     :properties
                     (filter (fn [[_ p]] (contains? p :original-value)))
                     (sort-by (comp prop-order first))
                     (into [] (keep properties/property-entry->go-prop))))))
  (output ddf-message g/Any (g/fnk [id position rotation scale source-resource ddf-properties]
                              (gen-ref-ddf id position rotation scale source-resource ddf-properties))))

(g/defnk produce-proto-msg [ref-ddf embed-ddf]
  {:components ref-ddf
   :embedded-components embed-ddf})

(def ^:private default-scale-value (:scale identity-transform-properties))

(defn- strip-default-scale-from-component-desc [component-desc]
  ;; GameObject$ComponentDesc or GameObject$EmbeddedComponentDesc in map format.
  (let [scale (:scale component-desc)]
    (if (or (= default-scale-value scale)
            (protobuf/default-read-scale-value? scale))
      (dissoc component-desc :scale)
      component-desc)))

(defn- strip-default-scale-from-component-descs [component-descs]
  (mapv strip-default-scale-from-component-desc component-descs))

(defn strip-default-scale-from-components-in-prototype-desc [prototype-desc]
  ;; GameObject$PrototypeDesc in map format.
  (-> prototype-desc
      (update :components strip-default-scale-from-component-descs)
      (update :embedded-components strip-default-scale-from-component-descs)))

(g/defnk produce-save-value [proto-msg]
  (strip-default-scale-from-components-in-prototype-desc proto-msg))

(defn- build-game-object [resource dep-resources user-data]
  ;; Please refer to `/engine/gameobject/proto/gameobject/gameobject_ddf.proto`
  ;; when reading this. It will clear up how the output binaries are structured.
  ;; Be aware that these structures are also used to store the saved project
  ;; data. Sometimes a field will only be used by the editor *or* the runtime.
  ;; At this point, all referenced and embedded components will have emitted
  ;; BuildResources. The engine does not have a concept of an EmbeddedComponent.
  ;; They are written as separate binaries and referenced just like any other
  ;; ReferencedComponent. However, embedded components from different sources
  ;; might have been fused into one BuildResource if they had the same contents.
  ;; We must update any references to these BuildResources to instead point to
  ;; the resulting fused BuildResource. We also extract :instance-data from the
  ;; component build targets and embed these as ComponentDesc instances in the
  ;; PrototypeDesc that represents the game object.
  (let [build-go-props (partial properties/build-go-props dep-resources)
        instance-data (:instance-data user-data)
        component-msgs (map :instance-msg instance-data)
        component-go-props (map (comp build-go-props :properties) component-msgs)
        component-build-resource-paths (map (comp resource/proj-path dep-resources :resource) instance-data)
        component-descs (map (fn [component-msg fused-build-resource-path go-props]
                               (-> component-msg
                                   (dissoc :data :properties :type) ; Runtime uses :property-decls, not :properties
                                   (assoc :component fused-build-resource-path)
                                   (cond-> (seq go-props)
                                           (assoc :property-decls (properties/go-props->decls go-props false)))))
                             component-msgs
                             component-build-resource-paths
                             component-go-props)
        property-resource-paths (into (sorted-set)
                                      (comp cat (keep properties/try-get-go-prop-proj-path))
                                      component-go-props)
        msg {:components component-descs
             :property-resources property-resource-paths}]
    {:resource resource :content (protobuf/map->bytes GameObject$PrototypeDesc msg)}))

(g/defnk produce-build-targets [_node-id resource proto-msg dep-build-targets id-counts]
  (or (let [dup-ids (keep (fn [[id count]] (when (> count 1) id)) id-counts)]
        (when (not-empty dup-ids)
          (g/->error _node-id :build-targets :fatal nil (format "the following ids are not unique: %s" (str/join ", " dup-ids)))))
      ;; Extract the :instance-data from the component build targets so that
      ;; overrides can be embedded in the resulting game object binary. We also
      ;; establish dependencies to build-targets from any resources referenced
      ;; by script property overrides.
      (let [flat-dep-build-targets (flatten dep-build-targets)
            instance-data (mapv :instance-data flat-dep-build-targets)]
        [(bt/with-content-hash
           {:node-id _node-id
            :resource (workspace/make-build-resource resource)
            :build-fn build-game-object
            :user-data {:proto-msg proto-msg :instance-data instance-data}
            :deps (into (vec flat-dep-build-targets)
                        (comp (mapcat :property-deps)
                              (util/distinct-by (comp resource/proj-path :resource)))
                        instance-data)})])))

(g/defnk produce-scene [_node-id child-scenes]
  {:node-id _node-id
   :aabb geom/null-aabb
   :children child-scenes})

(defn- attach-component [self-id comp-id ddf-input resolve-id?]
  (concat
    (when resolve-id?
      (->> (g/node-value self-id :component-ids)
           keys
           (g/update-property comp-id :id outline/resolve-id)))
    (for [[from to] [[:node-outline :child-outlines]
                     [:_node-id :nodes]
                     [:build-targets :dep-build-targets]
                     [:resource-property-build-targets :resource-property-build-targets]
                     [:ddf-message ddf-input]
                     [:component-id :component-id-pairs]
                     [:scene :child-scenes]]]
      (g/connect comp-id from self-id to))
    (for [[from to] [[:base-url :base-url]
                     [:id-counts :id-counts]]]
      (g/connect self-id from comp-id to))))

(defn- attach-ref-component [self-id comp-id]
  (attach-component self-id comp-id :ref-ddf false))

(defn- attach-embedded-component [self-id comp-id]
  (attach-component self-id comp-id :embed-ddf false))

(defn- outline-attach-ref-component [self-id comp-id]
  (attach-component self-id comp-id :ref-ddf true))

(defn- outline-attach-embedded-component [self-id comp-id]
  (attach-component self-id comp-id :embed-ddf true))

(g/defnk produce-go-outline [_node-id child-outlines]
  {:node-id _node-id
   :node-outline-key "Game Object"
   :label "Game Object"
   :icon game-object-icon
   :children (outline/natural-sort child-outlines)
   :child-reqs [{:node-type ReferencedComponent
                 :tx-attach-fn outline-attach-ref-component}
                {:node-type EmbeddedComponent
                 :tx-attach-fn outline-attach-embedded-component}]})

(g/defnode GameObjectNode
  (inherits resource-node/ResourceNode)

  (input ref-ddf g/Any :array)
  (input embed-ddf g/Any :array)
  (input child-scenes g/Any :array)
  (input component-id-pairs g/IdPair :array)
  (input dep-build-targets g/Any :array)
  (input resource-property-build-targets g/Any :array)
  (input base-url g/Str)

  (output base-url g/Str (gu/passthrough base-url))
  (output node-outline outline/OutlineData :cached produce-go-outline)
  (output proto-msg g/Any produce-proto-msg)
  (output save-value g/Any produce-save-value)
  (output build-targets g/Any :cached produce-build-targets)
  (output resource-property-build-targets g/Any (gu/passthrough resource-property-build-targets))
  (output scene g/Any :cached produce-scene)
  (output component-ids g/Dict :cached (g/fnk [component-id-pairs] (reduce conj {} component-id-pairs)))
  (output ddf-component-properties g/Any :cached
          (g/fnk [ref-ddf]
            (into []
                  (keep (fn [component-ddf]
                          (when-some [properties (not-empty (:properties component-ddf))]
                            {:id (:id component-ddf)
                             :properties properties})))
                  ref-ddf)))
  (output id-counts g/Any :cached (g/fnk [component-id-pairs]
                                         (reduce (fn [res id]
                                                   (update res id (fn [id] (inc (or id 0)))))
                                                 {} (map first component-id-pairs)))))

(defn- gen-component-id [go-node base]
  (let [ids (map first (g/node-value go-node :component-ids))]
    (loop [postfix 0]
      (let [id (if (= postfix 0) base (str base postfix))]
        (if (empty? (filter #(= id %) ids))
          id
          (recur (inc postfix)))))))

(defn- add-component [self source-resource id {:keys [position rotation scale]} properties select-fn]
  (let [path {:resource source-resource
              :overrides properties}]
    (g/make-nodes (g/node-id->graph-id self)
                  [comp-node [ReferencedComponent :id id :position position :rotation rotation :scale scale]]
                  (g/set-property comp-node :path path)
                  (attach-ref-component self comp-node)
                  (when select-fn
                    (select-fn [comp-node])))))

(defn add-component-file [go-id resource select-fn]
  (let [id (gen-component-id go-id (resource/base-name resource))]
    (g/transact
      (concat
        (g/operation-label "Add Component")
        (add-component go-id resource id identity-transform-properties [] select-fn)))))

(defn add-component-handler [workspace project go-id select-fn]
  (let [component-exts (map :ext (concat (workspace/get-resource-types workspace :component)
                                         (workspace/get-resource-types workspace :embeddable)))]
    (when-let [resource (first (resource-dialog/make workspace project {:ext component-exts :title "Select Component File"}))]
      (add-component-file go-id resource select-fn))))

(defn- selection->game-object [selection]
  (g/override-root (handler/adapt-single selection GameObjectNode)))

(handler/defhandler :add-from-file :workbench
  (active? [selection] (selection->game-object selection))
  (label [] "Add Component File")
  (run [workspace project selection app-view]
       (add-component-handler workspace project (selection->game-object selection) (fn [node-ids] (app-view/select app-view node-ids)))))

(defn- add-embedded-component [self project type data id {:keys [position rotation scale]} select-fn]
  (let [graph (g/node-id->graph-id self)
        resource (project/make-embedded-resource project type data)
        node-type (project/resource-node-type resource)]
    (g/make-nodes graph [comp-node [EmbeddedComponent :id id :position position :rotation rotation :scale scale]
                         resource-node [node-type :resource resource]]
                  (g/connect resource-node :_node-id self :nodes)
                  (g/connect comp-node :_node-id self :nodes)
                  (project/load-node project resource-node node-type resource)
                  (project/connect-if-output node-type resource-node comp-node
                                             [[:_node-id :embedded-resource-id]
                                              [:resource :source-resource]
                                              [:_properties :source-properties]
                                              [:node-outline :source-outline]
                                              [:undecorated-save-data :save-data]
                                              [:scene :scene]
                                              [:build-targets :source-build-targets]])
                  (attach-embedded-component self comp-node)
                  (when select-fn
                    (select-fn [comp-node])))))

(defn add-embedded-component-handler [user-data select-fn]
  (let [self (:_node-id user-data)
        project (project/get-project self)
        component-type (:resource-type user-data)
        workspace (:workspace user-data)
        template (workspace/template workspace component-type)
        id (gen-component-id self (:ext component-type))]
    (g/transact
     (concat
      (g/operation-label "Add Component")
      (add-embedded-component self project (:ext component-type) template id identity-transform-properties select-fn)))))

(defn add-embedded-component-label [user-data]
  (if-not user-data
    "Add Component"
    (let [rt (:resource-type user-data)]
      (or (:label rt) (:ext rt)))))

(defn embeddable-component-resource-types [workspace]
  (->> (workspace/get-resource-types workspace :component)
       (filter (fn [resource-type]
                 (and (not (contains? (:tags resource-type) :non-embeddable))
                      (workspace/has-template? workspace resource-type))))))

(defn add-embedded-component-options [self workspace user-data]
  (when (not user-data)
    (->> (embeddable-component-resource-types workspace)
         (map (fn [res-type] {:label (or (:label res-type) (:ext res-type))
                              :icon (:icon res-type)
                              :command :add
                              :user-data {:_node-id self :resource-type res-type :workspace workspace}}))
         (sort-by :label)
         vec)))

(handler/defhandler :add :workbench
  (label [user-data] (add-embedded-component-label user-data))
  (active? [selection] (selection->game-object selection))
  (run [user-data app-view] (add-embedded-component-handler user-data (fn [node-ids] (app-view/select app-view node-ids))))
  (options [selection user-data]
           (let [self (selection->game-object selection)
                 workspace (:workspace (g/node-value self :resource))]
             (add-embedded-component-options self workspace user-data))))

(defn load-game-object [project self resource prototype]
  (concat
    (for [component (:components prototype)
          :let [source-path (:component component)
                source-resource (workspace/resolve-resource resource source-path)
                resource-type (some-> source-resource resource/resource-type)
                transform-properties (select-transform-properties resource-type component)
                properties (:properties component)]]
      (add-component self source-resource (:id component) transform-properties properties nil))
    (for [embedded (:embedded-components prototype)
          :let [resource-type (get (g/node-value project :resource-types) (:type embedded))
                transform-properties (select-transform-properties resource-type embedded)]]
      (add-embedded-component self project (:type embedded) (:data embedded) (:id embedded) transform-properties false))))

(defn sanitize-property-descs-at-path [desc property-descs-path]
  ;; GameObject$ComponentDesc, GameObject$InstanceDesc, GameObject$EmbeddedInstanceDesc, or GameObject$CollectionInstanceDesc in map format.
  ;; The supplied path should lead up to a seq of GameObject$PropertyDescs in map format.
  (if-some [property-descs (not-empty (get-in desc property-descs-path))]
    (assoc-in desc property-descs-path (mapv properties/sanitize-property-desc property-descs))
    desc))

(defn- sanitize-referenced-component [component-desc]
  ;; GameObject$ComponentDesc in map format.
  (-> component-desc
      (sanitize-property-descs-at-path [:properties])
      (dissoc :property-decls) ; Only used in built data by the runtime.
      (strip-default-scale-from-component-desc)))

(defn- sanitize-embedded-component-data [embedded-component-desc resource-type-map]
  ;; GameObject$EmbeddedComponentDesc in map format.
  (let [component-ext (:type embedded-component-desc)
        resource-type (resource-type-map component-ext)
        tag-opts (:tag-opts resource-type)
        sanitize-fn (:sanitize-fn resource-type)
        sanitize-embedded-component-fn (:sanitize-embedded-component-fn (:component tag-opts))]
    (try
      (let [unsanitized-data (when (or sanitize-fn sanitize-embedded-component-fn)
                               (let [read-raw-fn (:read-raw-fn resource-type)
                                     unsanitized-data-string (:data embedded-component-desc)]
                                 (with-open [reader (StringReader. unsanitized-data-string)]
                                   (read-raw-fn reader))))
            sanitized-data (if sanitize-fn
                             (sanitize-fn unsanitized-data)
                             unsanitized-data)
            [embedded-component-desc sanitized-data] (if sanitize-embedded-component-fn
                                                       (sanitize-embedded-component-fn embedded-component-desc sanitized-data)
                                                       [embedded-component-desc sanitized-data])]
        (if (= unsanitized-data sanitized-data)
          embedded-component-desc
          (let [write-fn (:write-fn resource-type)
                sanitized-data-string (write-fn sanitized-data)]
            (assoc embedded-component-desc :data sanitized-data-string))))
      (catch Exception e
        ;; Leave unsanitized.
        (log/warn :msg (str "Failed to sanitize embedded component of type: " (or component-ext "nil")) :exception e)
        embedded-component-desc))))

(defn- sanitize-embedded-component [resource-type-map embedded-component-desc]
  ;; GameObject$EmbeddedComponentDesc in map format.
  (-> embedded-component-desc
      (sanitize-embedded-component-data resource-type-map)
      (strip-default-scale-from-component-desc)))

(defn sanitize-game-object [workspace prototype-desc]
  ;; GameObject$PrototypeDesc in map format.
  (let [resource-type-map (workspace/get-resource-type-map workspace)]
    (-> prototype-desc
        (update :components (partial mapv sanitize-referenced-component))
        (update :embedded-components (partial mapv (partial sanitize-embedded-component resource-type-map))))))

(defn- parse-embedded-dependencies [resource-types {:keys [id type data]}]
  (when-let [resource-type (resource-types type)]
    (let [read-component (:read-fn resource-type)
          component-dependencies (:dependencies-fn resource-type)]
      (try
        (component-dependencies (read-component (StringReader. data)))
        (catch Exception e
          (log/warn :msg (format "Couldn't determine dependencies for embedded component %s." id) :exception e)
          [])))))

(defn- make-dependencies-fn [workspace]
  (let [default-dependencies-fn (resource-node/make-ddf-dependencies-fn GameObject$PrototypeDesc)]
    (fn [source-value]
      (let [embedded-components (:embedded-components source-value)
            resource-types (workspace/get-resource-type-map workspace)]
        (into (default-dependencies-fn source-value)
              (mapcat (partial parse-embedded-dependencies resource-types))
              embedded-components)))))

(defn register-resource-types [workspace]
  (resource-node/register-ddf-resource-type workspace
    :ext "go"
    :label "Game Object"
    :node-type GameObjectNode
    :ddf-type GameObject$PrototypeDesc
    :load-fn load-game-object
    :dependencies-fn (make-dependencies-fn workspace)
    :sanitize-fn (partial sanitize-game-object workspace)
    :icon game-object-icon
    :view-types [:scene :text]
    :view-opts {:scene {:grid true}}))
