(ns editor.game-project
  (:require [clojure.string :as string]
            [clojure.java.io :as io]
            [dynamo.graph :as g]
            [util.murmur :as murmur]
            [editor.settings :as settings]
            [editor.settings-core :as settings-core]
            [editor.fs :as fs]
            [editor.graph-util :as gu]
            [editor.game-project-core :as gpcore]
            [editor.defold-project :as project]
            [camel-snake-kebab :as camel]
            [editor.workspace :as workspace]
            [editor.resource :as resource]
            [editor.resource-node :as resource-node]
            [service.log :as log])
  (:import [java.io File]
           [org.apache.commons.io IOUtils]))

(set! *warn-on-reflection* true)

(def game-project-icon "icons/32/Icons_04-Project-file.png")

(defn- ignored-setting?
  [{:keys [path]}]
  (= path ["project" "dependencies"]))

(defn- build-game-project [resource dep-resources user-data]
  (let [{:keys [raw-settings path->built-resource-settings]} user-data
        settings (into []
                       (comp
                         (remove ignored-setting?)
                         (map (fn [{:keys [path value] :as setting}]
                                    (if-let [resource-value (path->built-resource-settings path)]
                                      (let [new-val (resource/proj-path (dep-resources resource-value))]
                                        (assoc setting :value new-val))
                                      setting))))
                       (settings-core/settings-with-value raw-settings))
        ^String user-data-content (settings-core/settings->str settings)]
    {:resource resource :content (.getBytes user-data-content)}))

(defn- resource-content [resource]
  (with-open [s (io/input-stream resource)]
    (IOUtils/toByteArray s)))

(defn- build-custom-resource [resource dep-resources user-data]
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
  (resource-name [this] (resource/resource-name resource))
  (workspace [this] (resource/workspace resource))
  (resource-hash [this] (resource/resource-hash resource))
  (openable? [this] (resource/openable? resource))

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
    (map (comp strip-trailing-slash fs/with-leading-slash) paths)))

(def ^:private resource-setting-connections-template
  {["display" "display_profiles"] [[:build-targets :dep-build-targets]
                                   [:profile-data :display-profiles-data]]
   ["bootstrap" "debug_init_script"] [[:build-targets :dep-build-targets]]
   ["bootstrap" "main_collection"] [[:build-targets :dep-build-targets]]
   ["bootstrap" "render"] [[:build-targets :dep-build-targets]]
   ["graphics" "texture_profiles"] [[:build-targets :dep-build-targets]
                                    [:pb :texture-profiles-data]]
   ["input" "gamepads"] [[:build-targets :dep-build-targets]]
   ["input" "game_binding"] [[:build-targets :dep-build-targets]]})

(g/defnk produce-build-targets [_node-id resource raw-settings custom-build-targets resource-settings dep-build-targets]
  (let [dep-build-targets (vec (into (flatten dep-build-targets) custom-build-targets))
        deps-by-source (into {} (map
                                 (fn [build-target]
                                   (let [build-resource (:resource build-target)
                                         source-resource (:resource build-resource)]
                                     [source-resource build-resource]))
                                 dep-build-targets))
        path->built-resource-settings (into {} (keep (fn [resource-setting]
                                                       (when (resource-setting-connections-template (:path resource-setting))
                                                         [(:path resource-setting) (deps-by-source (:value resource-setting))]))
                                                     resource-settings))]
    [{:node-id _node-id
      :resource (workspace/make-build-resource resource)
      :build-fn build-game-project
      :user-data {:raw-settings raw-settings
                  :path->built-resource-settings path->built-resource-settings}
      :deps dep-build-targets}]))  

(g/defnode GameProjectNode
  (inherits resource-node/ResourceNode)

  (input display-profiles-data g/Any)
  (output display-profiles-data g/Any (gu/passthrough display-profiles-data))

  (input texture-profiles-data g/Any)
  (output texture-profiles-data g/Any (gu/passthrough texture-profiles-data))
  
  (input settings-map g/Any)
  (output settings-map g/Any :cached (gu/passthrough settings-map))

  (input form-data g/Any)
  (output form-data g/Any :cached (gu/passthrough form-data))

  (input raw-settings g/Any)
  (input resource-settings g/Any)

  (input resource-map g/Any)
  (input dep-build-targets g/Any :array)

  (output custom-build-targets g/Any :cached
          (g/fnk [_node-id resource-map settings-map]
                 (let [custom-paths (parse-custom-resource-paths (get settings-map ["project" "custom_resources"]))]
                   (map (partial make-custom-build-target _node-id)
                        (find-custom-resources resource-map custom-paths)))))

  (output outline g/Any :cached
          (g/fnk [_node-id] {:node-id _node-id :label "Game Project" :icon game-project-icon}))

  (input save-value g/Any)
  (output save-value g/Any (gu/passthrough save-value))

  (output build-targets g/Any :cached produce-build-targets))

;;; loading node

(defn- load-game-project [project self resource source-value]
  (let [graph-id (g/node-id->graph-id self)
        resource-setting-connections (reduce-kv (fn [m k v] (assoc m k [self v])) {} resource-setting-connections-template)]
    (concat
      (g/make-nodes graph-id [settings-node settings/SettingsNode]
                    (g/connect settings-node :_node-id self :nodes)
                    (g/connect settings-node :settings-map self :settings-map)
                    (g/connect settings-node :save-value self :save-value)
                    (g/connect settings-node :form-data self :form-data)
                    (g/connect settings-node :raw-settings self :raw-settings)
                    (g/connect settings-node :resource-settings self :resource-settings)
                    (settings/load-settings-node settings-node resource source-value gpcore/basic-meta-info resource-setting-connections))
      (g/connect project :resource-map self :resource-map))))

;; Test support

(defn set-setting!
  "Exposed for tests"
  [game-project path value]
  (let [form-data (g/node-value game-project :form-data)]
    (let [{:keys [user-data set]} (:form-ops form-data)]
      (set user-data path value))))

(defn get-setting
  [game-project path]
  ((g/node-value game-project :settings-map) path))

(defn register-resource-types [workspace]
  (resource-node/register-settings-resource-type workspace
    :ext "project"
    :label "Project"
    :node-type GameProjectNode
    :load-fn load-game-project
    :icon game-project-icon
    :view-types [:form-view :text]))
