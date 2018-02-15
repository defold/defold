(ns editor.game-object
  (:require [clojure.set :as set]
            [clojure.string :as str]
            [editor.protobuf :as protobuf]
            [dynamo.graph :as g]
            [editor.app-view :as app-view]
            [editor.graph-util :as gu]
            [editor.dialogs :as dialogs]
            [editor.geom :as geom]
            [editor.handler :as handler]
            [editor.defold-project :as project]
            [editor.scene :as scene]
            [editor.scene-tools :as scene-tools]
            [editor.sound :as sound]
            [editor.script :as script]
            [editor.resource :as resource]
            [editor.resource-node :as resource-node]
            [editor.workspace :as workspace]
            [editor.properties :as properties]
            [editor.validation :as validation]
            [editor.outline :as outline]
            [service.log :as log])
  (:import [com.dynamo.gameobject.proto GameObject$PrototypeDesc]
           [com.dynamo.sound.proto Sound$SoundDesc]
           [java.io StringReader]
           [javax.vecmath Matrix4d Vector3d]))

(set! *warn-on-reflection* true)

(def game-object-icon "icons/32/Icons_06-Game-object.png")
(def unknown-icon "icons/32/Icons_29-AT-Unknown.png")

(defn- gen-ref-ddf [id position rotation path ddf-properties]
  {:id id
   :position position
   :rotation rotation
   :component (resource/resource->proj-path path)
   :properties ddf-properties})

(defn- gen-embed-ddf [id position rotation save-data]
  {:id id
   :type (or (and (:resource save-data) (:ext (resource/resource-type (:resource save-data))))
             "unknown")
   :position position
   :rotation rotation
   :data (or (:content save-data) "")})

(defn- wrap-if-raw-sound [target _node-id]
  (let [resource (:resource (:resource target))
        source-path (resource/proj-path resource)
        ext (resource/ext resource)]
    (if (sound/supported-audio-formats ext)
      (let [workspace (project/workspace (project/get-project _node-id))
            res-type  (workspace/get-resource-type workspace "sound")
            pb        {:sound source-path}
            target    {:node-id  _node-id
                       :resource (workspace/make-build-resource (resource/make-memory-resource workspace res-type (protobuf/map->str Sound$SoundDesc pb)))
                       :build-fn (fn [resource dep-resources user-data]
                                   (let [pb (:pb user-data)
                                         pb (assoc pb :sound (resource/proj-path (second (first dep-resources))))]
                                     {:resource resource :content (protobuf/map->bytes Sound$SoundDesc pb)}))
                       :deps     [target]}]
        target)
      target)))

(defn- source-outline-subst [err]
  (if-let [resource (get-in err [:user-data :resource])]
    (let [rt (resource/resource-type resource)]
      {:node-id (:node-id err)
       :label (or (:label rt) (:ext rt) "unknown")
       :icon (or (:icon rt) unknown-icon)})
    {:node-id -1
     :icon ""
     :label ""}))

(def ^:private identity-transform-properties
  {:position [0.0 0.0 0.0]
   :rotation [0.0 0.0 0.0 1.0]
   :scale [1.0 1.0 1.0]})

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
             :label id
             :icon (or (not-empty (:icon source-outline)) unknown-icon)
             :outline-overridden? overridden?
             :children (:children source-outline)}
          (cond->
            (resource/openable-resource? source-resource) (assoc :link source-resource :outline-reference? true)
            source-id (assoc :alt-outline source-outline))))))
  (output ddf-message g/Any :abstract)
  (output scene g/Any :cached (g/fnk [_node-id transform scene]
                                (let [transform (if-let [local-transform (:transform scene)]
                                                  (doto (Matrix4d. ^Matrix4d transform)
                                                    (.mul ^Matrix4d local-transform))
                                                  transform)
                                      updatable (some-> (:updatable scene)
                                                  (assoc :node-id _node-id))]
                                  (cond-> scene
                                    true (scene/claim-scene _node-id)
                                    true (assoc :transform transform
                                           :aabb (geom/aabb-transform (geom/aabb-incorporate (get scene :aabb (geom/null-aabb)) 0 0 0) transform))
                                    updatable ((partial scene/map-scene #(assoc % :updatable updatable)))))))
  (output build-resource resource/Resource :abstract)
  (output build-targets g/Any :cached (g/fnk [_node-id source-build-targets resource-property-build-targets build-resource ddf-message transform]
                                        (if-let [source-build-target (first source-build-targets)]
                                          (let [[go-props go-prop-dep-build-targets] (properties/build-target-go-props resource-property-build-targets (:properties ddf-message))
                                                build-target (-> source-build-target
                                                                 (assoc :resource build-resource)
                                                                 (wrap-if-raw-sound _node-id)
                                                                 (assoc :instance-data {:resource (:resource source-build-target)
                                                                                        :resource-property-build-targets go-prop-dep-build-targets
                                                                                        :transform transform
                                                                                        :instance-msg (if (seq go-props)
                                                                                                        (assoc ddf-message :properties go-props)
                                                                                                        ddf-message)}))]
                                            [build-target])
                                          [])))
  (output resource-property-build-targets g/Any (gu/passthrough resource-property-build-targets))
  (output _properties g/Properties :cached produce-component-properties))

(g/defnode EmbeddedComponent
  (inherits ComponentNode)

  (input embedded-resource-id g/NodeID)
  (input save-data g/Any :cascade-delete)
  (output ddf-message g/Any :cached (g/fnk [id position rotation save-data]
                                      (gen-embed-ddf id position rotation save-data)))
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

(defn load-property-overrides
  "Takes a sequence of GameObject.PropertyDesc in map format and returns a map of [prop-kw, [go-prop-type, clj-value]]."
  [base-resource property-descs]
  (into {}
        (map (partial properties/property-desc->property-override base-resource))
        property-descs))

(defn property-overrides
  "Takes a sequence of go-props and returns a map of [prop-kw, [go-prop-type, clj-value]]."
  [go-props]
  (into {}
        (map properties/go-prop->property-override)
        go-props))

(g/defnode ReferencedComponent
  (inherits ComponentNode)

  (property path g/Any
            (dynamic edit-type (g/fnk [source-resource]
                                      {:type resource/Resource
                                       :ext (some-> source-resource resource/resource-type :ext)
                                       :to-type (fn [v] (:resource v))
                                       :from-type (fn [r] {:resource r :overrides {}})}))
            (value (g/fnk [source-resource ddf-properties]
                          {:resource source-resource
                           :overrides (property-overrides ddf-properties)}))
            (set (fn [evaluation-context self old-value new-value]
                   (concat
                     (if-let [old-source (g/node-value self :source-id evaluation-context)]
                       (g/delete-node old-source)
                       [])
                     (let [new-resource (:resource new-value)
                           resource-type (and new-resource (resource/resource-type new-resource))
                           override? (contains? (:tags resource-type) :overridable-properties)]
                       (if override?
                         (let [project (project/get-project self)]
                           (project/connect-resource-node project new-resource self []
                                                          (fn [comp-node]
                                                            (let [override (g/override (:basis evaluation-context) comp-node {:traverse? (constantly true)})
                                                                  id-mapping (:id-mapping override)
                                                                  or-node (get id-mapping comp-node)
                                                                  comp-props (:properties (g/node-value comp-node :_properties evaluation-context))]
                                                              (concat
                                                                (:tx-data override)
                                                                (let [outputs (g/output-labels (:node-type (resource/resource-type new-resource)))]
                                                                  (for [[from to] [[:_node-id :source-id]
                                                                                   [:resource :source-resource]
                                                                                   [:node-outline :source-outline]
                                                                                   [:_properties :source-properties]
                                                                                   [:scene :scene]
                                                                                   [:build-targets :source-build-targets]
                                                                                   [:resource-property-build-targets :resource-property-build-targets]]
                                                                        :when (contains? outputs from)]
                                                                    (g/connect or-node from self to)))
                                                                (for [[label [type value]] (:overrides new-value)]
                                                                  (let [original-type (get-in comp-props [label :type])
                                                                        override-type (properties/go-prop-type->property-type type)]
                                                                    (when (= original-type override-type)
                                                                      (if (not= :property-type-resource type)
                                                                        (g/set-property or-node label value)
                                                                        (concat
                                                                          (g/set-property or-node label value)
                                                                          (g/update-property or-node :property-resources assoc label value)))))))))))
                         (project/resource-setter self (:resource old-value) (:resource new-value)
                                                  [:resource :source-resource]
                                                  [:node-outline :source-outline]
                                                  [:user-properties :user-properties]
                                                  [:scene :scene]
                                                  [:build-targets :source-build-targets]))))))
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
  (output ddf-message g/Any :cached (g/fnk [id position rotation source-resource ddf-properties]
                                      (gen-ref-ddf id position rotation source-resource ddf-properties))))

(g/defnk produce-proto-msg [ref-ddf embed-ddf]
  {:components ref-ddf
   :embedded-components embed-ddf})

(defn- externalize [inst-data resources]
  (map (fn [{:keys [resource instance-msg]}]
         (let [resource (get resources resource)
               go-props (properties/build-go-props resources (:properties instance-msg))]
           (cond-> (-> instance-msg
                       (dissoc :data :type)
                       (assoc :component (resource/proj-path resource)))

                   (seq go-props)
                   (assoc :properties go-props
                          :property-decls (properties/go-props->decls go-props)))))
       inst-data))

(defn- build-game-object [resource dep-resources user-data]
  (let [instance-msgs (externalize (:instance-data user-data) dep-resources)
        property-resource-paths (properties/go-prop-resource-paths (mapcat :properties instance-msgs))
        msg {:components instance-msgs
             :property-resources property-resource-paths}]
    {:resource resource :content (protobuf/map->bytes GameObject$PrototypeDesc msg)}))

(g/defnk produce-build-targets [_node-id resource proto-msg dep-build-targets id-counts]
  (or (let [dup-ids (keep (fn [[id count]] (when (> count 1) id)) id-counts)]
        (when (not-empty dup-ids)
          (g/->error _node-id :build-targets :fatal nil (format "the following ids are not unique: %s" (str/join ", " dup-ids)))))
      (let [instance-data (map :instance-data (flatten dep-build-targets))]
        [{:node-id _node-id
          :resource (workspace/make-build-resource resource)
          :build-fn build-game-object
          :user-data {:proto-msg proto-msg :instance-data instance-data}
          :deps (into (vec (flatten dep-build-targets))
                      (comp (mapcat :resource-property-build-targets)
                            (distinct))
                      instance-data)}])))

(g/defnk produce-scene [_node-id child-scenes]
  {:node-id _node-id
   :aabb (reduce geom/aabb-union (geom/null-aabb) (filter #(not (nil? %)) (map :aabb child-scenes)))
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
  (output proto-msg g/Any :cached produce-proto-msg)
  (output save-value g/Any (gu/passthrough proto-msg))
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
                  [comp-node [ReferencedComponent :id id :position position :rotation rotation :scale scale :path path]]
                  (attach-ref-component self comp-node)
                  (when select-fn
                    (select-fn [comp-node])))))

(defn add-component-file [go-id resource select-fn]
  (let [id (gen-component-id go-id (resource/base-name resource))]
    (g/transact
      (concat
        (g/operation-label "Add Component")
        (add-component go-id resource id identity-transform-properties {} select-fn)))))

(defn add-component-handler [workspace project go-id select-fn]
  (let [component-exts (map :ext (concat (workspace/get-resource-types workspace :component)
                                         (workspace/get-resource-types workspace :embeddable)))]
    (when-let [resource (first (dialogs/make-resource-dialog workspace project {:ext component-exts :title "Select Component File"}))]
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
        resource (project/make-embedded-resource project type data)]
    (g/make-nodes graph [comp-node [EmbeddedComponent :id id :position position :rotation rotation :scale scale]]
      (g/connect comp-node :_node-id self :nodes)
      (if select-fn
        (select-fn [comp-node])
        [])
      (let [tx-data (project/make-resource-node graph project resource true {comp-node [[:_node-id :embedded-resource-id]
                                                                                        [:resource :source-resource]
                                                                                        [:_properties :source-properties]
                                                                                        [:node-outline :source-outline]
                                                                                        [:save-data :save-data]
                                                                                        [:scene :scene]
                                                                                        [:build-targets :source-build-targets]]
                                                                             self [[:_node-id :nodes]]})]
        (concat
          tx-data
          (if (empty? tx-data)
            []
            (attach-embedded-component self comp-node)))))))

(defn add-embedded-component-handler [user-data select-fn]
  (let [self (:_node-id user-data)
        project (project/get-project self)
        component-type (:resource-type user-data)
        template (workspace/template component-type)
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
                      (workspace/has-template? resource-type))))))

(defn add-embedded-component-options [self workspace user-data]
  (when (not user-data)
    (->> (embeddable-component-resource-types workspace)
         (map (fn [res-type] {:label (or (:label res-type) (:ext res-type))
                              :icon (:icon res-type)
                              :command :add
                              :user-data {:_node-id self :resource-type res-type}}))
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
                properties (load-property-overrides resource (:properties component))]]
      (add-component self source-resource (:id component) transform-properties properties nil))
    (for [embedded (:embedded-components prototype)
          :let [resource-type (get (g/node-value project :resource-types) (:type embedded))
                transform-properties (select-transform-properties resource-type embedded)]]
      (add-embedded-component self project (:type embedded) (:data embedded) (:id embedded) transform-properties false))))

(defn- sanitize-component [c]
  (dissoc c :property-decls))

(defn- sanitize-game-object [go]
  (update go :components (partial mapv sanitize-component)))

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
    :sanitize-fn sanitize-game-object
    :icon game-object-icon
    :view-types [:scene :text]
    :view-opts {:scene {:grid true}}))
