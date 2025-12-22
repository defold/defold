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

(ns editor.game-object
  (:require [clojure.set :as set]
            [dynamo.graph :as g]
            [editor.app-view :as app-view]
            [editor.attachment :as attachment]
            [editor.build-target :as bt]
            [editor.collection-string-data :as collection-string-data]
            [editor.defold-project :as project]
            [editor.editor-extensions.coerce :as coerce]
            [editor.editor-extensions.graph :as ext-graph]
            [editor.editor-extensions.runtime :as rt]
            [editor.game-object-common :as game-object-common]
            [editor.graph-util :as gu]
            [editor.handler :as handler]
            [editor.id :as id]
            [editor.localization :as localization]
            [editor.outline :as outline]
            [editor.properties :as properties]
            [editor.protobuf :as protobuf]
            [editor.resource :as resource]
            [editor.resource-dialog :as resource-dialog]
            [editor.resource-node :as resource-node]
            [editor.scene :as scene]
            [editor.scene-tools :as scene-tools]
            [editor.sound :as sound]
            [editor.types :as types]
            [editor.util :as eutil]
            [editor.validation :as validation]
            [editor.workspace :as workspace]
            [internal.util :as util]
            [util.eduction :as e])
  (:import [com.dynamo.gameobject.proto GameObject$ComponentDesc GameObject$EmbeddedComponentDesc GameObject$PrototypeDesc]
           [com.dynamo.gamesys.proto Sound$SoundDesc]
           [javax.vecmath Vector3d]
           [org.apache.commons.io FilenameUtils]
           [org.luaj.vm2 LuaError]))

(set! *warn-on-reflection* true)

(def unknown-icon "icons/32/Icons_29-AT-Unknown.png")

(defn- gen-ref-ddf [id position rotation scale source-resource ddf-properties]
  (-> (protobuf/make-map-without-defaults GameObject$ComponentDesc
        :id id
        :position position
        :rotation rotation
        :scale scale
        :component (resource/resource->proj-path source-resource)
        :properties ddf-properties)
      (game-object-common/strip-default-scale-from-component-desc)))

(defn- gen-embed-ddf [id position rotation scale source-resource source-save-value]
  (-> (protobuf/make-map-without-defaults GameObject$EmbeddedComponentDesc
        :id id
        :type (or (some-> source-resource resource/resource-type :ext) "unknown")
        :position position
        :rotation rotation
        :scale scale
        :data source-save-value)
      (game-object-common/strip-default-scale-from-component-desc)))

(defn- wrap-if-raw-sound [component-source-build-target _node-id]
  (let [build-resource (:resource component-source-build-target)
        source-resource (:resource build-resource)
        ext (resource/type-ext source-resource)]
    (if-not (sound/supported-audio-formats ext)
      component-source-build-target ; Not a raw sound. Return unaltered.
      (let [workspace (resource/workspace source-resource)
            dep-build-targets [component-source-build-target] ; The build-target of the referenced .wav file.
            sound-desc (protobuf/make-map-without-defaults Sound$SoundDesc
                         :sound (resource/proj-path source-resource))
            sound-desc-resource (workspace/make-placeholder-resource workspace "sound")]
        (sound/make-sound-desc-build-target _node-id sound-desc-resource sound-desc dep-build-targets)))))

(defn- source-outline-subst [err]
  (if-let [resource (get-in err [:user-data :resource])]
    (let [rt (resource/resource-type resource)]
      {:node-id (:node-id err)
       :node-outline-key (or (:ext rt) "unknown")
       :label (or (:label rt) (:ext rt) (localization/message "outline.unknown"))
       :icon (or (:icon rt) unknown-icon)})
    {:node-id -1
     :node-outline-key ""
     :icon ""
     :label ""}))

(defn- supported-transform-properties [component-resource-type]
  (assert (some? component-resource-type))
  (if-not (contains? (:tags component-resource-type) :component)
    #{}
    (let [supported-properties (-> component-resource-type :tag-opts :component :transform-properties)]
      (assert (set? supported-properties))
      (assert (every? (partial contains? scene/identity-transform-properties) supported-properties))
      supported-properties)))

(defn- select-transform-properties
  "Used to strip unsupported transform properties when loading a component.
  Unsupported properties will be removed. If the resource type is unknown to us,
  keep all transform properties."
  [component-resource-type component]
  (select-keys
    component
    (if (some? component-resource-type)
      (supported-transform-properties component-resource-type)
      game-object-common/component-transform-property-keys)))

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
                            (let [stripped-transform-properties (remove transform-properties (keys scene/identity-transform-properties))
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

(defn- resource-path-error [_node-id source-resource]
    (or (validation/prop-error :fatal _node-id :path validation/prop-nil? source-resource "Path")
        (validation/prop-error :fatal _node-id :path validation/prop-resource-not-exists? source-resource "Path")
        (validation/prop-error :fatal _node-id :script validation/prop-resource-not-component? source-resource "Path")))

(g/defnk produce-referenced-component-build-targets [_node-id source-resource ddf-message pose resource-property-build-targets source-build-targets]
  ;; Create a build-target for the referenced component. Also tag on
  ;; :component-instance-data with the overrides for this instance. This will
  ;; later be extracted and compiled into the GameObject - the overrides do not
  ;; end up in the resulting component binary.
  (or (resource-path-error _node-id source-resource)
      (if-some [errors (not-empty (keep :error (:properties ddf-message)))]
        (g/error-aggregate errors :_node-id _node-id :_label :build-targets)
        (when-some [source-build-target (first source-build-targets)]
          (let [build-target (wrap-if-raw-sound source-build-target _node-id)
                build-resource (:resource build-target) ; The wrap-if-raw-sound call might have changed this.
                proj-path->resource-property-build-target (bt/make-proj-path->build-target resource-property-build-targets)
                component-instance-data (game-object-common/referenced-component-instance-data build-resource ddf-message pose proj-path->resource-property-build-target)]
            [(assoc build-target
               :component-instance-data component-instance-data)])))))

(g/defnk produce-embedded-component-build-targets [_node-id ddf-message pose source-build-targets]
  ;; Create a build-target for the embedded component. Also tag on
  ;; :component-instance-data with the overrides for this instance. This will
  ;; later be extracted and compiled into the GameObject - the overrides do not
  ;; end up in the resulting component binary.
  (if-some [errors (not-empty (keep :error (:properties ddf-message)))]
    (g/error-aggregate errors :_node-id _node-id :_label :build-targets)
    (when-some [source-build-target (first source-build-targets)]
      (let [build-resource (:resource source-build-target)
            component-instance-data (game-object-common/embedded-component-instance-data build-resource ddf-message pose)]
        [(assoc source-build-target
           :component-instance-data component-instance-data)]))))

(g/defnode ComponentNode
  (inherits scene/SceneNode)
  (inherits outline/OutlineNode)

  (property id g/Str ; Required protobuf field.
            (dynamic error (g/fnk [_node-id id id-counts]
                                  (or (validation/prop-error :fatal _node-id :id validation/prop-empty? id "Id")
                                      (validation/prop-error :fatal _node-id :id (partial validation/prop-id-duplicate? id-counts) id)
                                      (validation/prop-error :warning _node-id :id validation/prop-contains-prohibited-characters? id "Id"))))
            (dynamic read-only? (g/fnk [_node-id]
                                  (g/override? _node-id))))
  (property url g/Str ; Just for presentation.
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
            (some-> source-resource resource/proj-path) (assoc :link source-resource :outline-reference? true)
            source-id (assoc :alt-outline source-outline))))))
  (output ddf-message g/Any :abstract)
  (output scene g/Any :cached (g/fnk [_node-id id transform scene]
                                (game-object-common/component-scene _node-id id transform scene)))
  (output build-targets g/Any :abstract)
  (output resource-property-build-targets g/Any (gu/passthrough resource-property-build-targets))
  (output _properties g/Properties :cached produce-component-properties))

(g/defnode EmbeddedComponent
  (inherits ComponentNode)

  (input embedded-resource-id g/NodeID :cascade-delete)
  (input source-save-value g/Any)
  (output ddf-message g/Any (g/fnk [id position rotation scale source-resource source-save-value]
                              (gen-embed-ddf id position rotation scale source-resource source-save-value)))
  (output build-targets g/Any produce-embedded-component-build-targets))

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
      (scene/manip-scale-scene-node evaluation-context node-id delta)

      :else
      nil)))

;; -----------------------------------------------------------------------------

(defn- get-all-comp-exts [workspace]
  (keep (fn [[ext {:keys [tags :as _resource-type]}]]
          (when (or (contains? tags :component)
                    (contains? tags :embeddable))
            ext))
        (workspace/get-resource-type-map workspace)))

(g/defnode ReferencedComponent
  (inherits ComponentNode)

  (property path g/Any ; Required protobuf field.
            (dynamic edit-type (g/fnk [source-resource _node-id]
                                 (let [resource-type (some-> source-resource resource/resource-type)
                                       tags (:tags resource-type)]
                                   {:type resource/Resource
                                    :ext (or (and (or (contains? tags :component) (contains? tags :embeddable))
                                                  (some-> resource-type :ext))
                                             (get-all-comp-exts (project/workspace (project/get-project _node-id))))
                                    :to-type (fn [v] (:resource v))
                                    :from-type (fn [r] {:resource r :overrides []})})))
            (value (g/fnk [source-resource ddf-properties]
                     {:resource source-resource
                      :overrides ddf-properties}))
            (set (fn [evaluation-context self old-value new-value]
                   (concat
                     (when-some [old-source (g/node-value self :source-id evaluation-context)]
                       (g/delete-node old-source))
                     (let [new-resource (:resource new-value)
                           resource-type (some-> new-resource resource/resource-type)
                           project (project/get-project self)

                           [comp-node tx-data]
                           (if (resource/overridable-resource-type? resource-type)
                             (let [workspace (project/workspace project)]
                               (when-some [{connect-tx-data :tx-data
                                            comp-node :node-id
                                            created-in-tx :created-in-tx} (project/connect-resource-node evaluation-context project new-resource self [])]
                                 [comp-node
                                  (concat
                                    connect-tx-data
                                    (g/override comp-node {:traverse-fn g/always-override-traverse-fn}
                                                (fn [evaluation-context id-mapping]
                                                  (let [or-comp-node (get id-mapping comp-node)
                                                        comp-props (if created-in-tx
                                                                     {}
                                                                     (:properties (g/node-value comp-node :_properties evaluation-context)))]
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
                             (resource-path-error _node-id source-resource))))

  (input source-id g/NodeID :cascade-delete)
  (output build-targets g/Any produce-referenced-component-build-targets)
  (output ddf-properties g/Any :cached
          (g/fnk [source-properties]
                 (let [prop-order (into {} (map-indexed (fn [i k] [k i])) (:display-order source-properties))]
                   (->> source-properties
                        :properties
                        (filter (fn [[_ p]] (contains? p :original-value)))
                        (sort-by (comp prop-order first))
                        (into [] (keep properties/property-entry->go-prop))
                        (not-empty)))))
  (output ddf-message g/Any (g/fnk [id position rotation scale source-resource ddf-properties]
                              (gen-ref-ddf id position rotation scale source-resource ddf-properties))))

(g/defnk produce-proto-msg [ref-ddf embed-ddf]
  (protobuf/make-map-without-defaults GameObject$PrototypeDesc
    :components ref-ddf
    :embedded-components embed-ddf))

(defn component-property-desc-save-value [component-property-desc]
  ;; GameObject$ComponentPropertyDesc or GameObject$ComponentDesc in map format.
  (-> component-property-desc
      (protobuf/sanitize-repeated :properties properties/go-prop->property-desc)))

(defn- component-desc-save-value [component-desc]
  ;; GameObject$ComponentDesc in map format.
  (-> component-desc
      (component-property-desc-save-value)))

(defn prototype-desc-save-value [prototype-desc]
  ;; GameObject$PrototypeDesc in map format.
  (-> prototype-desc
      (protobuf/sanitize-repeated :components component-desc-save-value)))

(g/defnk produce-build-targets [_node-id resource dep-build-targets id-counts]
  (or (let [dup-ids (keep (fn [[id count]] (when (> count 1) id)) id-counts)]
        (game-object-common/maybe-duplicate-id-error _node-id dup-ids))
      (let [component-instance-build-targets (flatten dep-build-targets)
            component-instance-datas (mapv :component-instance-data component-instance-build-targets)
            component-build-targets (into []
                                          (comp (map #(dissoc % :component-instance-data))
                                                (util/distinct-by (comp resource/proj-path :resource)))
                                          component-instance-build-targets)]
        [(game-object-common/game-object-build-target resource _node-id component-instance-datas component-build-targets)])))

(g/defnk produce-scene [_node-id child-scenes]
  (game-object-common/game-object-scene _node-id child-scenes))

(defn- attach-component [self-id comp-id ddf-input]
  ;; self-id: GameObjectNode
  ;; comp-id: EmbeddedComponent, ReferencedComponent
  (concat
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

(defn- attach-referenced-component [self-id comp-id]
  ;; self-id: GameObjectNode
  ;; comp-id: ReferencedComponent
  (attach-component self-id comp-id :ref-ddf))

(defn- attach-embedded-component [self-id comp-id]
  ;; self-id: GameObjectNode
  ;; comp-id: EmbeddedComponent
  (attach-component self-id comp-id :embed-ddf))

(defn- resolve-id [self-id comp-id]
  (g/update-property comp-id :id id/resolve (g/node-value self-id :component-ids)))

(defn- outline-attach-ref-component [self-id comp-id]
  ;; self-id: GameObjectNode
  ;; comp-id: ReferencedComponent
  (concat
    (resolve-id self-id comp-id)
    (attach-component self-id comp-id :ref-ddf)))

(defn- outline-attach-embedded-component [self-id comp-id]
  ;; self-id: GameObjectNode
  ;; comp-id: EmbeddedComponent
  (concat
    (resolve-id self-id comp-id)
    (attach-component self-id comp-id :embed-ddf)))

(g/defnk produce-go-outline [_node-id child-outlines]
  {:node-id _node-id
   :node-outline-key "Game Object"
   :label (localization/message "outline.game-object")
   :icon game-object-common/game-object-icon
   :children (localization/annotate-as-sorted localization/natural-sort-by-label child-outlines)
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
  (output save-value g/Any :cached (g/fnk [proto-msg] (prototype-desc-save-value proto-msg)))
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
  (id/gen base (e/map first (g/node-value go-node :component-ids))))

(defn- add-component [self source-resource id transform-properties properties select-fn]
  (let [path {:resource source-resource
              :overrides properties}]
    (g/make-nodes (g/node-id->graph-id self)
      [comp-node [ReferencedComponent :id id]]
      (gu/set-properties-from-pb-map comp-node GameObject$ComponentDesc transform-properties
        position :position
        rotation :rotation
        scale :scale)
      (g/set-property comp-node :path path) ; Set last so the :alter-referenced-component-fn can alter component properties.
      (attach-referenced-component self comp-node)
      (when select-fn
        (select-fn [comp-node])))))

(defn add-referenced-component! [go-id resource select-fn]
  (let [id (gen-component-id go-id (resource/base-name resource))]
    (g/transact
      (concat
        (g/operation-label (localization/message "operation.game-object.add-component"))
        (add-component go-id resource id nil nil select-fn)))))

(defn add-component-handler [workspace project go-id select-fn]
  (when-let [resources (resource-dialog/make
                         workspace project
                         {:ext (get-all-comp-exts workspace)
                          :title (localization/message "dialog.select-component-file.title")
                          :selection :multiple})]
    (doseq [resource resources]
      (add-referenced-component! go-id resource select-fn))))

(defn- selection->game-object [selection]
  (g/override-root (handler/adapt-single selection GameObjectNode)))

(handler/defhandler :edit.add-referenced-component :workbench
  :label (localization/message "command.edit.add-referenced-component.variant.game-object")
  (active? [selection] (selection->game-object selection))
  (run [workspace project selection app-view]
       (add-component-handler workspace project (selection->game-object selection) (fn [node-ids] (app-view/select app-view node-ids)))))

(defn- connect-embedded-resource [node-type resource-node comp-node]
  (gu/connect-existing-outputs node-type resource-node comp-node
    [[:_node-id :embedded-resource-id]
     [:resource :source-resource]
     [:_properties :source-properties]
     [:node-outline :source-outline]
     [:save-value :source-save-value]
     [:scene :scene]
     [:build-targets :source-build-targets]]))

(defn- add-embedded-component [self project type pb-map id transform-properties select-fn]
  {:pre [(map? pb-map)]}
  (let [graph (g/node-id->graph-id self)
        resource (project/make-embedded-resource project :editable type pb-map)
        node-type (project/resource-node-type resource)]
    (g/make-nodes graph [comp-node [EmbeddedComponent :id id]
                         resource-node [node-type :resource resource]]
      (gu/set-properties-from-pb-map comp-node GameObject$EmbeddedComponentDesc transform-properties
        position :position
        rotation :rotation
        scale :scale)
      (project/load-embedded-resource-node project resource-node resource pb-map)
      (connect-embedded-resource node-type resource-node comp-node)
      (attach-embedded-component self comp-node)
      (when select-fn
        (select-fn [comp-node])))))

(defn add-embedded-component! [go-id resource-type select-fn]
  (let [project (project/get-project go-id)
        workspace (project/workspace project)
        pb-map (game-object-common/template-pb-map workspace resource-type)
        id (gen-component-id go-id (:ext resource-type))]
    (g/transact
      (concat
        (g/operation-label (localization/message "operation.game-object.add-component"))
        (add-embedded-component go-id project (:ext resource-type) pb-map id nil select-fn)))))

(defn- add-embedded-component-handler [user-data select-fn]
  (let [go-id (:_node-id user-data)
        resource-type (:resource-type user-data)]
    (add-embedded-component! go-id resource-type select-fn)))

(defn- embeddable-component-resource-type? [resource-type workspace evaluation-context]
  (let [{:keys [tags] :as resource-type} resource-type]
    (and (contains? tags :component)
         (not (contains? tags :non-embeddable))
         (workspace/has-template? workspace resource-type evaluation-context))))

(defn embeddable-component-resource-types
  ([workspace]
   (g/with-auto-evaluation-context evaluation-context
     (embeddable-component-resource-types workspace evaluation-context)))
  ([workspace evaluation-context]
   (keep (fn [[_ext resource-type]]
           (when (embeddable-component-resource-type? resource-type workspace evaluation-context)
             resource-type))
         (resource/resource-types-by-type-ext (:basis evaluation-context) workspace :editable))))

(defn- add-embedded-component-options [self workspace user-data evaluation-context]
  (when (not user-data)
    (->> (embeddable-component-resource-types workspace evaluation-context)
         (mapv (fn [res-type]
                 {:label (or (:label res-type) (:ext res-type))
                  :icon (:icon res-type)
                  :command :edit.add-embedded-component
                  :user-data {:_node-id self :resource-type res-type :workspace workspace}}))
         (localization/annotate-as-sorted localization/natural-sort-by-label))))

(handler/defhandler :edit.add-embedded-component :workbench
  (label [user-data]
    (if-not user-data
      (localization/message "command.edit.add-embedded-component.variant.game-object")
      (let [rt (:resource-type user-data)]
        (or (:label rt) (:ext rt)))))
  (active? [selection] (selection->game-object selection))
  (run [user-data app-view] (add-embedded-component-handler user-data (fn [node-ids] (app-view/select app-view node-ids))))
  (options [selection user-data evaluation-context]
    (let [self (selection->game-object selection)
          workspace (:workspace (g/node-value self :resource evaluation-context))]
      (add-embedded-component-options self workspace user-data evaluation-context))))

(defn load-game-object [project self resource prototype-desc]
  {:pre [(map? prototype-desc)]} ; GameObject$PrototypeDesc in map format.
  (let [workspace (project/workspace project)
        ext->embedded-component-resource-type (workspace/get-resource-type-map workspace)]
    (concat
      (for [component (:components prototype-desc)
            :let [source-path (:component component)
                  source-resource (workspace/resolve-resource resource source-path)
                  resource-type (some-> source-resource resource/resource-type)
                  transform-properties (select-transform-properties resource-type component)
                  properties (:properties component)]]
        (add-component self source-resource (:id component) transform-properties properties nil))
      (for [{:keys [id type data] :as embedded-component-desc} (:embedded-components prototype-desc)]
        (let [resource-type (ext->embedded-component-resource-type type)
              transform-properties (select-transform-properties resource-type embedded-component-desc)]
          (collection-string-data/verify-string-decoded-embedded-component-desc! embedded-component-desc resource)
          (add-embedded-component self project type data id transform-properties false))))))

(defn- sanitize-game-object [workspace prototype-desc]
  ;; GameObject$PrototypeDesc in map format.
  (let [ext->embedded-component-resource-type (workspace/get-resource-type-map workspace)]
    (game-object-common/sanitize-prototype-desc prototype-desc ext->embedded-component-resource-type)))

(defn- string-encode-game-object [workspace prototype-desc]
  ;; GameObject$PrototypeDesc in map format.
  (let [ext->embedded-component-resource-type (workspace/get-resource-type-map workspace)]
    (collection-string-data/string-encode-prototype-desc ext->embedded-component-resource-type prototype-desc)))

(defn- handle-drop
  [root-id _selection workspace world-pos resources]
  (let [transform-props {:position (types/Point3d->Vec3 world-pos)}
        taken-ids (map first (g/node-value root-id :component-ids))
        supported-exts (get-all-comp-exts workspace)]
    (->> resources
         (e/filter #(some #{(resource/type-ext %)} supported-exts))
         (outline/name-resource-pairs taken-ids)
         (mapv (fn [[id resource]]
                 (add-component root-id resource id transform-props nil nil))))))

(def ^:private ext-referenced-component-type "component-reference")
(def ^:private constantly-ext-referenced-component-type (constantly ext-referenced-component-type))

(ext-graph/register-property-getter!
  ::ReferencedComponent
  (fn ReferencedComponent-getter [_node-id property _evaluation-context]
    (case property
      "type" constantly-ext-referenced-component-type
      nil)))

(ext-graph/register-property-getter!
  ::EmbeddedComponent
  (fn EmbeddedComponent-getter [node-id property evaluation-context]
    (case property
      "type" #(resource/ext (g/node-value node-id :source-resource evaluation-context))
      nil)))

(defmethod ext-graph/extract-node-type [::GameObjectNode :components]
  [rt attachment workspace _node-id _list-kw evaluation-context]
  (if-let [lua-type (attachment "type")]
    (let [type-name (rt/->clj rt coerce/string lua-type)]
      (if (= ext-referenced-component-type type-name)
        [(dissoc attachment "type") ReferencedComponent]
        (let [resource-types (resource/resource-types-by-type-ext (:basis evaluation-context) workspace :editable)
              resource-type (resource-types type-name)]
          (if (and resource-type (embeddable-component-resource-type? resource-type workspace evaluation-context))
            [attachment EmbeddedComponent]
            (throw (LuaError. (str "type is not "
                                   (->> (embeddable-component-resource-types workspace evaluation-context)
                                        (map :ext)
                                        (cons ext-referenced-component-type)
                                        (eutil/join-words ", " " or ")))))))))
    (throw (LuaError. "type is required"))))

(defmethod ext-graph/create-extra-nodes ::EmbeddedComponent [evaluation-context rt project workspace attachment node-id]
  (let [component-ext (rt/->clj rt coerce/string (attachment "type"))
        resource-types (resource/resource-types-by-type-ext (:basis evaluation-context) workspace :editable)
        resource-type (resource-types component-ext)]
    (assert resource-type)
    (assert (embeddable-component-resource-type? resource-type workspace evaluation-context))
    (let [graph (g/node-id->graph-id node-id)
          pb-map (game-object-common/template-pb-map workspace resource-type evaluation-context)
          resource (resource/make-memory-resource workspace resource-type pb-map)
          node-type (:node-type resource-type)]
      (g/make-nodes graph [resource-node [node-type :resource resource]]
        (project/load-embedded-resource-node project resource-node resource pb-map)
        (connect-embedded-resource node-type resource-node node-id)))))

(defmethod ext-graph/init-attachment ::EmbeddedComponent
  [evaluation-context rt project parent-node-id _child-node-type child-node-id attachment]
  (let [component-ext (rt/->clj rt coerce/string (attachment "type"))]
    (-> attachment
        (dissoc "type")
        (eutil/provide-defaults
          "id" (rt/->lua (id/gen component-ext (g/node-value parent-node-id :component-ids evaluation-context))))
        (ext-graph/attachment->set-tx-steps child-node-id rt project evaluation-context))))

(defn- set-referenced-attachment-properties [evaluation-context rt project parent-node-id child-node-id attachment]
  (-> attachment
      ;; Ignore the path since it's applied first in init-attachment
      (dissoc "path")
      (eutil/provide-defaults
        "id" (rt/->lua
               (id/gen
                 (if-let [lua-path (attachment "path")]
                   (FilenameUtils/getBaseName (rt/->clj rt coerce/string lua-path))
                   "component")
                 (g/node-value parent-node-id :component-ids evaluation-context))))
      (ext-graph/attachment->set-tx-steps child-node-id rt project evaluation-context)))

(defmethod ext-graph/init-attachment ::ReferencedComponent
  [evaluation-context rt project parent-node-id _child-node-type child-node-id attachment]
  (if-let [lua-resource-path (attachment "path")]
    (concat
      (ext-graph/attachment->set-tx-steps {"path" lua-resource-path} child-node-id rt project evaluation-context)
      (g/expand-ec set-referenced-attachment-properties rt project parent-node-id child-node-id attachment))
    (set-referenced-attachment-properties evaluation-context rt project parent-node-id child-node-id attachment)))

(defn- embedded-component-attachment-alternative [comp-id evaluation-context]
  (g/node-value comp-id :embedded-resource-id evaluation-context))

(defn register-resource-types [workspace]
  (concat
    (attachment/register
      workspace GameObjectNode :components
      :add {EmbeddedComponent attach-embedded-component
            ReferencedComponent attach-referenced-component}
      :get attachment/nodes-getter
      :read-only? #(g/override? (:basis %2) %1))
    (attachment/define-alternative workspace EmbeddedComponent embedded-component-attachment-alternative)
    (resource-node/register-ddf-resource-type workspace
      :ext "go"
      :label (localization/message "resource.type.go")
      :node-type GameObjectNode
      :ddf-type GameObject$PrototypeDesc
      :load-fn load-game-object
      :allow-unloaded-use true
      :dependencies-fn (game-object-common/make-game-object-dependencies-fn #(workspace/get-resource-type-map workspace))
      :sanitize-fn (partial sanitize-game-object workspace)
      :string-encode-fn (partial string-encode-game-object workspace)
      :icon game-object-common/game-object-icon
      :icon-class :design
      :view-types [:scene :text]
      :view-opts {:scene {:grid true
                          :drop-fn handle-drop}})))
