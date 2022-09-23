(ns editor.game-object-data
  (:require [dynamo.graph :as g]
            [editor.build-target :as bt]
            [editor.collection-string-data :as collection-string-data]
            [editor.defold-project :as project]
            [editor.game-object-common :as game-object-common]
            [editor.geom :as geom]
            [editor.graph-util :as gu]
            [editor.math :as math]
            [editor.properties :as properties]
            [editor.resource-node :as resource-node]
            [editor.workspace :as workspace]
            [internal.graph :as ig]
            [internal.graph.types :as gt]
            [util.coll :refer [pair]])
  (:import [com.dynamo.gameobject.proto GameObject$PrototypeDesc]
           [javax.vecmath Matrix4d]))

(def ^:private embedded-component-resource-connections
  [[:build-targets :embedded-component-build-targets]])

(defn- add-embedded-component-resource-node [host-node-id embedded-component-resource-data ext->resource-type project]
  (let [embedded-component-pb-string (collection-string-data/string-encoded-data ext->resource-type embedded-component-resource-data)
        embedded-resource (project/make-embedded-resource project (:type embedded-component-resource-data) embedded-component-pb-string)
        node-type (project/resource-node-type embedded-resource)
        graph (g/node-id->graph-id host-node-id)]
    (g/make-nodes graph [embedded-resource-node-id [node-type :resource embedded-resource]]
      (project/load-node project embedded-resource-node-id node-type embedded-resource)
      (project/connect-if-output node-type embedded-resource-node-id host-node-id embedded-component-resource-connections))))

(g/defnode EmbeddedComponentHostResourceNode
  (inherits resource-node/ImmutableResourceNode)

  (property embedded-component-resource-data->index g/Any
            (dynamic visible (g/constantly false))
            (set (fn [evaluation-context self _old-value new-value]
                   (let [basis (:basis evaluation-context)
                         project (project/get-project basis self)
                         workspace (project/workspace project)
                         ext->resource-type (workspace/get-resource-type-map workspace)]
                     (-> []
                         (into (comp (map gt/source-id)
                                     (distinct)
                                     (mapcat g/delete-node))
                               (ig/explicit-inputs basis self :embedded-component-build-targets))
                         (into (mapcat #(add-embedded-component-resource-node self (key %) ext->resource-type project))
                               (sort-by val new-value)))))))

  (input embedded-component-build-targets g/Any :array :cascade-delete))

;; -----------------------------------------------------------------------------

(defn- any-component-desc->transform-matrix
  ^Matrix4d [{:keys [position rotation scale]}]
  ;; GameObject$ComponentDesc, or GameObject$EmbeddedInstanceDesc in map format.
  (math/clj->mat4 position rotation (or scale 1.0)))

(defn prototype-desc->referenced-component-proj-paths [prototype-desc]
  (eduction
    (keep (comp not-empty :component))
    (distinct)
    (:components prototype-desc)))

(defn embedded-component-desc->embedded-component-resource-data [embedded-component-desc]
  (select-keys embedded-component-desc [:type :data]))

(defn prototype-desc->embedded-component-resource-datas [prototype-desc]
  (eduction
    (map embedded-component-desc->embedded-component-resource-data)
    (distinct)
    (:embedded-components prototype-desc)))

(defn- prototype-desc->embedded-component-resource-data->index [prototype-desc]
  (into {}
        (map-indexed (fn [index embedded-component-resource-data]
                       (pair embedded-component-resource-data index)))
        (prototype-desc->embedded-component-resource-datas prototype-desc)))

(defn prototype-desc->component-property-descs [prototype-desc]
  (into []
        (keep (fn [component-desc]
                (let [component-id (:id component-desc)
                      property-descs (:properties component-desc)]
                  (when (seq property-descs)
                    {:id component-id
                     :properties property-descs}))))
        (:components prototype-desc)))

(defn component-property-descs->resources [component-property-descs proj-path->resource]
  (eduction
    (mapcat :properties)
    (map #(dissoc % :id))
    (distinct)
    (keep #(properties/property-desc->resource % proj-path->resource))
    component-property-descs))

(defn- prototype-desc->referenced-property-resources [prototype-desc proj-path->resource]
  (-> prototype-desc
      prototype-desc->component-property-descs
      (component-property-descs->resources proj-path->resource)))

(defn component-desc->component-instance-data [component-desc proj-path->build-target]
  (let [build-resource (-> component-desc :component proj-path->build-target :resource)
        transform-matrix (any-component-desc->transform-matrix component-desc)
        proj-path->source-resource (comp proj-path->build-target :resource :resource)
        go-props-with-source-resources (mapv #(properties/property-desc->go-prop % proj-path->source-resource) (:properties component-desc))
        component-desc (assoc component-desc :properties go-props-with-source-resources)]
    (game-object-common/referenced-component-instance-data build-resource component-desc transform-matrix proj-path->build-target)))

(defn embedded-component-desc->component-instance-data [embedded-component-desc embedded-component-desc->build-resource]
  (let [build-resource (embedded-component-desc->build-resource embedded-component-desc)
        transform-matrix (any-component-desc->transform-matrix embedded-component-desc)]
    (game-object-common/embedded-component-instance-data build-resource embedded-component-desc transform-matrix)))

(defn prototype-desc->component-instance-datas [prototype-desc embedded-component-desc->build-resource proj-path->build-target]
  (-> []
      (into (map #(component-desc->component-instance-data % proj-path->build-target))
            (:components prototype-desc))
      (into (map #(embedded-component-desc->component-instance-data % embedded-component-desc->build-resource))
            (:embedded-components prototype-desc))))

(defn make-embedded-component-desc->build-resource [embedded-component-build-targets embedded-component-resource-data->index]
  {:pre [(vector? embedded-component-build-targets)
         (ifn? embedded-component-resource-data->index)]}
  (comp :resource
        first
        embedded-component-build-targets
        embedded-component-resource-data->index
        embedded-component-desc->embedded-component-resource-data))

;; -----------------------------------------------------------------------------

(g/defnk produce-proj-path->build-target [referenced-component-build-targets resource-property-build-targets]
  (bt/make-proj-path->build-target
    (concat referenced-component-build-targets
            resource-property-build-targets)))

(g/defnk produce-build-targets [_node-id resource prototype-desc embedded-component-resource-data->index embedded-component-build-targets referenced-component-build-targets proj-path->build-target]
  (let [component-descs (:components prototype-desc)
        embedded-component-descs (:embedded-components prototype-desc)]
    (or (let [dup-ids (game-object-common/any-descs->duplicate-ids
                        (concat component-descs
                                embedded-component-descs))]
          (game-object-common/maybe-duplicate-id-error _node-id dup-ids))
        (let [embedded-component-desc->build-resource
              (make-embedded-component-desc->build-resource embedded-component-build-targets embedded-component-resource-data->index)

              component-instance-datas
              (prototype-desc->component-instance-datas prototype-desc embedded-component-desc->build-resource proj-path->build-target)

              component-build-targets
              (into embedded-component-build-targets
                    referenced-component-build-targets)

              build-resource (workspace/make-build-resource resource)]
          [(game-object-common/game-object-build-target build-resource _node-id component-instance-datas component-build-targets)]))))

(g/defnk produce-ddf-component-properties [prototype-desc]
  (prototype-desc->component-property-descs prototype-desc))

(g/defnk produce-node-outline [_node-id]
  {:node-id _node-id
   :node-outline-key "Game Object Data"
   :label "Game Object Data"
   :icon game-object-common/game-object-icon})

(g/defnk produce-scene [_node-id]
  {:node-id _node-id
   :aabb geom/null-aabb})

(def ^:private referenced-component-connections
  [[:build-targets :referenced-component-build-targets]
   [:resource-property-build-targets :resource-property-build-targets]])

(def ^:private resource-property-connections
  [[:build-targets :resource-property-build-targets]])

(g/defnode GameObjectDataNode
  (inherits EmbeddedComponentHostResourceNode)

  (property prototype-desc g/Any
            (dynamic visible (g/constantly false))
            (set (fn [evaluation-context self old-value new-value]
                   ;; We use default evaluation-context in queries to ensure the
                   ;; results are cached. See comment in connect-resource-node.
                   (let [basis (:basis evaluation-context)
                         project (project/get-project basis self)
                         workspace (project/workspace project)
                         proj-path->resource (g/node-value workspace :resource-map)]
                     (letfn [(disconnect-resource [proj-path-or-resource connections]
                               (project/disconnect-resource-node evaluation-context project proj-path-or-resource self connections))
                             (connect-resource [proj-path-or-resource connections]
                               (:tx-data (project/connect-resource-node evaluation-context project proj-path-or-resource self connections)))]
                       (-> (g/set-property self :embedded-component-resource-data->index
                             (prototype-desc->embedded-component-resource-data->index new-value))
                           (into (mapcat #(disconnect-resource % referenced-component-connections))
                                 (prototype-desc->referenced-component-proj-paths old-value))
                           (into (mapcat #(disconnect-resource % resource-property-connections))
                                 (prototype-desc->referenced-property-resources old-value proj-path->resource))
                           (into (mapcat #(connect-resource % referenced-component-connections))
                                 (prototype-desc->referenced-component-proj-paths new-value))
                           (into (mapcat #(connect-resource % resource-property-connections))
                                 (prototype-desc->referenced-property-resources new-value proj-path->resource))))))))

  (input referenced-component-build-targets g/Any :array)
  (input resource-property-build-targets g/Any :array)

  ;; Internal outputs.
  (output proj-path->build-target g/Any :cached produce-proj-path->build-target)

  ;; GameObject interface.
  (output build-targets g/Any :cached produce-build-targets)
  (output ddf-component-properties g/Any :cached produce-ddf-component-properties)
  (output node-outline g/Any produce-node-outline)
  (output resource-property-build-targets g/Any (gu/passthrough resource-property-build-targets))
  (output scene g/Any produce-scene))

(defn- sanitize-game-object-data [workspace prototype-desc]
  (let [ext->resource-type (workspace/get-resource-type-map workspace)]
    (game-object-common/sanitize-prototype-desc prototype-desc ext->resource-type :embed-data-as-maps)))

(defn- load-game-object-data [_project self _resource prototype-desc]
  (g/set-property self :prototype-desc prototype-desc))

(defn register-resource-types [workspace]
  (resource-node/register-ddf-resource-type workspace
    :ext "go"
    :label "Game Object Data"
    :node-type GameObjectDataNode
    :ddf-type GameObject$PrototypeDesc
    :dependencies-fn (game-object-common/make-game-object-dependencies-fn workspace)
    :sanitize-fn (partial sanitize-game-object-data workspace)
    :load-fn load-game-object-data
    :auto-connect-save-data? false
    :icon game-object-common/game-object-icon
    :view-types [:scene :text]
    :view-opts {:scene {:grid true}}))

;; -----------------------------------------------------------------------------

{'PrototypeDesc :-
 :components [{'ComponentDesc :-
               :component "/path/to/any-component.ext"
               :properties [{'PropertyDesc :-
                             :value "/path/to/any-resource.ext"}]}]
 :embedded-components [{'EmbeddedComponentDesc :-
                        :data {'AnyComponent :-
                               :UNKNOWN "HERE BE DRAGONS"}}]}

{'CollectionDesc :-
 :instances [{'InstanceDesc :-
              :prototype "/path/to/game-object.go"
              :component-properties [{'ComponentPropertyDesc :-
                                      :properties [{'PropertyDesc :-
                                                    :value "/path/to/any-resource.ext"}]}]}]
 :embedded-instances [{'EmbeddedInstanceDesc :-
                       :data {'PrototypeDesc :-
                              :components [{'ComponentDesc :-
                                            :component "/path/to/any-component.ext"
                                            :properties [{'PropertyDesc :-
                                                          :value "/path/to/any-resource.ext"}]}]
                              :embedded-components [{'EmbeddedComponentDesc :-
                                                     :data {'AnyComponent :-
                                                            :UNKNOWN "HERE BE DRAGONS"}}]}
                       :component-properties [{'ComponentPropertyDesc :-
                                               :properties [{'PropertyDesc :-
                                                             :value "/path/to/any-resource.ext"}]}]}]
 :collection-instances [{'CollectionInstanceDesc :-
                         :collection "/path/to/collection.collection"
                         :instance-properties [{'InstancePropertyDesc :-
                                                :properties [{'ComponentPropertyDesc :-
                                                              :properties [{'PropertyDesc :-
                                                                            :value "/path/to/any-resource.ext"}]}]}]}]}
