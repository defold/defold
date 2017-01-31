(ns editor.game-project
  (:require [clojure.string :as string]
            [clojure.java.io :as io]
            [dynamo.graph :as g]
            [util.murmur :as murmur]
            [editor.graph-util :as gu]
            [editor.game-project-core :as gpcore]
            [editor.defold-project :as project]
            [camel-snake-kebab :as camel]
            [editor.workspace :as workspace]
            [editor.resource :as resource]
            [service.log :as log])
  (:import [java.io File]
           [org.apache.commons.io IOUtils]))

(set! *warn-on-reflection* true)

(def game-project-icon "icons/32/Icons_04-Project-file.png")

(defn- label [key]
  (-> key
      name
      camel/->Camel_Snake_Case_String
      (string/replace "_" " ")))

(defn- make-form-field [setting]
  (assoc setting
         :label (label (second (:path setting)))
         :optional true))

(defn- make-form-section [category-name category-info settings]
  {:title category-name
   :help (:help category-info)
   :fields (map make-form-field settings)})

(defn- make-form-values-map [settings]
  (into {} (map (juxt :path :value) settings)))

(defn- make-form-data [form-ops meta-info settings]
  (let [meta-settings (:settings meta-info)
        meta-category-map (:categories meta-info)
        categories (distinct (map gpcore/setting-category meta-settings))
        category-settings (group-by gpcore/setting-category meta-settings)
        sections (map #(make-form-section % (get-in meta-info [:categories %]) (category-settings %)) categories)
        values (make-form-values-map settings)]
    {:form-ops form-ops :sections sections :values values}))

(g/defnk produce-save-data [_node-id resource merged-raw-settings]
  (let [content (gpcore/settings->str (gpcore/settings-with-value merged-raw-settings))]
    {:resource resource
     :content content}))

(defn- build-game-project [self basis resource dep-resources user-data]
  (let [{:keys [raw-settings path->built-resource-settings]} user-data
        settings (mapv (fn [{:keys [path value] :as setting}]
                         (if-let [resource-value (path->built-resource-settings path)]
                           (let [new-val (resource/proj-path (dep-resources resource-value))]
                             (assoc setting :value new-val))
                           setting))
                      (gpcore/settings-with-value raw-settings))
        ^String user-data-content (gpcore/settings->str settings)]
    {:resource resource :content (.getBytes user-data-content)}))

(defn- resource-content [resource]
  (with-open [s (io/input-stream resource)]
    (IOUtils/toByteArray s)))

(defn- build-custom-resource [self basis resource dep-resources user-data]
  {:resource resource :content (resource-content (:resource resource))})

(defrecord CustomResource [resource]
  ;; Only purpose is to provide resource-type with :build-ext = :ext
  resource/Resource
  (children [this] (resource/children resource))
  (ext [this] (resource/ext resource))
  (resource-type [this]
    (let [ext (resource/ext this)]
      {:ext ext
       :label "Custom Resource"
       :build-ext ext}))
  (source-type [this] (resource/source-type resource))
  (exists? [this] (resource/exists? resource))
  (read-only? [this] (resource/read-only? resource))
  (path [this] (resource/path resource))
  (abs-path [this] (resource/abs-path resource))
  (proj-path [this] (resource/proj-path resource))
  (url [this] (resource/url resource))
  (resource-name [this] (resource/resource-name resource))
  (workspace [this] (resource/workspace resource))
  (resource-hash [this] (resource/resource-hash resource))

  io/IOFactory
  (io/make-input-stream  [this opts] (io/input-stream resource))
  (io/make-reader        [this opts] (io/reader resource))
  (io/make-output-stream [this opts] (io/output-stream resource))
  (io/make-writer        [this opts] (io/writer resource)))

(defn- make-custom-build-target [node-id resource]
  {:node-id node-id
   :resource (workspace/make-build-resource (CustomResource. resource))
   :build-fn build-custom-resource
   ;; NOTE! Break build cache when resource content changes.
   :user-data {:hash (murmur/hash64-bytes (resource-content resource))}})

(defn- with-leading-slash [path]
  (if (string/starts-with? path "/")
    path
    (str "/" path)))

(defn- strip-trailing-slash [path]
  (string/replace path #"/*$" ""))

(defn- file-resource? [resource]
  (= (resource/source-type resource) :file))

(defn- find-custom-resources [resource-map custom-paths]
  (->> (flatten (keep (fn [custom-path]
                      (when-let [base-resource (resource-map custom-path)]
                        (resource/resource-seq base-resource)))
                    custom-paths))
       (distinct)
       (filter file-resource?)))

(defn- parse-custom-resource-paths [cr-setting]
  (let [paths (remove string/blank? (map string/trim (string/split (or cr-setting "")  #",")))]
    (map (comp strip-trailing-slash with-leading-slash) paths)))

(defn- set-raw-setting [settings {:keys [path] :as meta-setting} value]
  (gpcore/set-setting settings path (gpcore/render-raw-setting-value meta-setting value)))

(defn- set-form-op [{:keys [node-id resource-setting-nodes meta-settings]} path value]
  (let [meta-setting (gpcore/get-meta-setting meta-settings path)]
    (if-let [resource-setting-node-id (resource-setting-nodes path)]
      (do
        (g/set-property! resource-setting-node-id :value value)
        (g/update-property! node-id :raw-settings set-raw-setting meta-setting (resource/resource->proj-path value)))
      (g/update-property! node-id :raw-settings set-raw-setting meta-setting value))))

(defn- clear-form-op [{:keys [node-id resource-setting-nodes meta-settings]} path]
  (when-let [resource-setting-node-id (resource-setting-nodes path)]
    (let [default-resource (gpcore/get-default-setting meta-settings path)]
      (g/set-property! resource-setting-node-id :value default-resource)))
  (g/update-property! node-id :raw-settings gpcore/clear-setting path))

(defn- make-form-ops [node-id resource-setting-nodes meta-settings]
  {:user-data {:node-id node-id :resource-setting-nodes resource-setting-nodes :meta-settings meta-settings}
   :set set-form-op
   :clear clear-form-op})

(g/defnk produce-settings-map [meta-info raw-settings resource-settings]
  (let [meta-settings (:settings meta-info)
        sanitized-settings (gpcore/sanitize-settings meta-settings (gpcore/settings-with-value raw-settings))
        all-settings (concat (gpcore/make-default-settings meta-settings) sanitized-settings resource-settings)
        settings-map (gpcore/make-settings-map all-settings)]
    settings-map))

(g/defnk produce-form-data [_node-id meta-info raw-settings resource-setting-nodes resource-settings]
  (let [meta-settings (:settings meta-info)
        sanitized-settings (gpcore/sanitize-settings meta-settings (gpcore/settings-with-value raw-settings))
        non-defaulted-setting-paths (into #{} (map :path (filter :value sanitized-settings)))
        non-default-resource-settings (filter (comp non-defaulted-setting-paths :path) resource-settings)
        all-settings (concat sanitized-settings non-default-resource-settings)]
    (make-form-data (make-form-ops _node-id resource-setting-nodes meta-settings) meta-info all-settings)))

(def ^:private resource-setting-connections
  {["display" "display_profiles"] [[:build-targets :dep-build-targets]
                                   [:profile-data :display-profiles-data]]
   ["bootstrap" "main_collection"] [[:build-targets :dep-build-targets]]
   ["bootstrap" "render"] [[:build-targets :dep-build-targets]]
   ["graphics" "texture_profiles"] [[:build-targets :dep-build-targets]]
   ["input" "gamepads"] [[:build-targets :dep-build-targets]]
   ["input" "game_binding"] [[:build-targets :dep-build-targets]]})

(g/defnode ResourceSettingNode
  (property path g/Any)
  (property value resource/Resource
            (dynamic visible (g/constantly false))
            (value (gu/passthrough resource))
            (set (fn [basis self old-value new-value]
                   (concat
                     ;; connect resource node to this
                     (project/resource-setter basis self old-value new-value
                                              [:resource :resource])
                     (let [game-project-id (ffirst (g/targets-of basis self :_node-id))
                           path (g/node-value self :path {:basis basis})]
                       ;; connect extra resource node outputs directly to GameProjectNode
                       (apply project/resource-setter basis game-project-id old-value new-value
                              (resource-setting-connections path)))))))
  (input resource resource/Resource)
  (output resource-setting-reference g/Any :cached (g/fnk [_node-id path value] {:path path :node-id _node-id :value value})))

(g/defnode GameProjectNode
  (inherits project/ResourceNode)

  (property raw-settings g/Any (dynamic visible (g/constantly false)))
  (property meta-info g/Any (dynamic visible (g/constantly false)))

  (output settings-map g/Any :cached produce-settings-map)
  (output form-data g/Any :cached produce-form-data)

  (input display-profiles-data g/Any)
  (output display-profiles-data g/Any (gu/passthrough display-profiles-data))

  (input resource-setting-references g/Any :array)
  (output resource-setting-nodes g/Any :cached (g/fnk [resource-setting-references] (into {} (map (juxt :path :node-id) resource-setting-references))))
  (output resource-settings g/Any :cached (g/fnk [resource-setting-references] (map #(select-keys % [:path :value]) resource-setting-references)))

  (output merged-raw-settings g/Any :cached (g/fnk [raw-settings meta-info resource-settings]
                                                   (let [meta-settings-map (gpcore/make-meta-settings-map (:settings meta-info))
                                                         raw-resource-settings (map (fn [setting]
                                                                                      (update setting :value
                                                                                              #(when %
                                                                                                 (gpcore/render-raw-setting-value (meta-settings-map (:path setting))
                                                                                                                                  (resource/resource->proj-path %)))))
                                                                                    resource-settings)
                                                         meta-settings (:meta-settings meta-info)]
                                                     (reduce (fn [raw {:keys [path value]}]
                                                               (if (gpcore/get-setting raw path)
                                                                 (gpcore/set-setting raw path value)
                                                                 raw))
                                                             raw-settings
                                                             raw-resource-settings))))

  (input resource-map g/Any)
  (input dep-build-targets g/Any :array)

  (output custom-build-targets g/Any :cached (g/fnk [_node-id resource-map settings-map]
                                                    (let [custom-paths (parse-custom-resource-paths (get settings-map ["project" "custom_resources"]))]
                                                      (map (partial make-custom-build-target _node-id)
                                                           (find-custom-resources resource-map custom-paths)))))

  (output outline g/Any :cached (g/fnk [_node-id] {:node-id _node-id :label "Game Project" :icon game-project-icon}))
  (output save-data g/Any :cached produce-save-data)
  (output build-targets g/Any :cached (g/fnk [_node-id resource raw-settings custom-build-targets resource-settings dep-build-targets]
                                             (let [dep-build-targets (vec (into (flatten dep-build-targets) custom-build-targets))
                                                   deps-by-source (into {} (map
                                                                             (fn [build-target]
                                                                               (let [build-resource (:resource build-target)
                                                                                     source-resource (:resource build-resource)]
                                                                                 [source-resource build-resource]))
                                                                             dep-build-targets))
                                                   path->built-resource-settings (into {} (keep (fn [resource-setting]
                                                                                                  (when (resource-setting-connections (:path resource-setting))
                                                                                                    [(:path resource-setting) (deps-by-source (:value resource-setting))]))
                                                                                                resource-settings))]
                                               [{:node-id _node-id
                                                 :resource (workspace/make-build-resource resource)
                                                 :build-fn build-game-project
                                                 :user-data {:raw-settings raw-settings
                                                             :path->built-resource-settings path->built-resource-settings}
                                                 :deps dep-build-targets}]))))

;;; loading node

(defn- resolve-resource-settings [settings base-resource value-field]
  (mapv (fn [setting]
          (if (= :resource (:type setting))
            (update setting value-field #(workspace/resolve-resource base-resource %))
            setting))
        settings))

(defn- type-annotate-settings [settings meta-settings]
  (let [meta-settings-map (gpcore/make-meta-settings-map meta-settings)]
    (map #(assoc % :type (:type (meta-settings-map (:path %)))) settings)))

(defn- load-game-project [project self input]
  (let [raw-settings (gpcore/parse-settings (gpcore/string-reader (slurp input)))
        game-project-resource (g/node-value self :resource)
        meta-info (-> (gpcore/add-meta-info-for-unknown-settings gpcore/basic-meta-info raw-settings)
                      (update :settings resolve-resource-settings game-project-resource :default))
        meta-settings (:settings meta-info)
        settings (-> (gpcore/sanitize-settings meta-settings raw-settings) ; this provokes parse errors if any
                     (type-annotate-settings meta-settings)
                     (resolve-resource-settings game-project-resource :value))
        resource-setting-paths (set (map :path (filter #(= :resource (:type %)) meta-settings)))
        graph-id (g/node-id->graph-id self)]

    (concat
      ;; We retain the actual raw string settings and update these when/if the user changes a setting,
      ;; rather than parse to proper typed/sanitized values and rendering these on save - again only to
      ;; reduce diffs.
      ;; Resource settings are also stored in separate ResourceSettingNodes, and this is the canonical place to look
      ;; for the settings current value. The corresponding setting in raw-settings may be stale and is only used to track
      ;; whether the value is/was set or cleared.
      (g/set-property self :raw-settings raw-settings :meta-info meta-info)
      (g/connect project :resource-map self :resource-map)
      (for [resource-setting-path resource-setting-paths]
        (let [resource (gpcore/get-setting-or-default meta-settings settings resource-setting-path)]
          (g/make-nodes graph-id [resource-setting-node ResourceSettingNode]
                        (g/connect resource-setting-node :_node-id self :nodes)
                        (g/set-property resource-setting-node :path resource-setting-path)
                        (when resource
                          (g/set-property resource-setting-node :value resource))
                        (g/connect resource-setting-node :resource-setting-reference self :resource-setting-references)))))))

(defn register-resource-types [workspace]
  (workspace/register-resource-type workspace
                                    :textual? true
                                    :ext "project"
                                    :label "Project"
                                    :node-type GameProjectNode
                                    :load-fn load-game-project
                                    :icon game-project-icon
                                    :view-types [:form-view :text]))
