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

(ns editor.game-project
  (:require [clojure.java.io :as io]
            [clojure.string :as string]
            [dynamo.graph :as g]
            [editor.build-target :as bt]
            [editor.code.lang.ini :as ini]
            [editor.form :as form]
            [editor.fs :as fs]
            [editor.game-project-core :as gpcore]
            [editor.gamepads :as gamepads]
            [editor.graph-util :as gu]
            [editor.localization :as localization]
            [editor.pipeline :as pipeline]
            [editor.resource :as resource]
            [editor.resource-node :as resource-node]
            [editor.settings :as settings]
            [editor.settings-core :as settings-core]
            [editor.workspace :as workspace]
            [util.coll :as coll :refer [pair]]
            [util.defonce :as defonce]))

(set! *warn-on-reflection* true)

(def game-project-icon "icons/32/Icons_04-Project-file.png")

(def ^:private gamepads-setting-path ["input" "gamepads"])
(def ^:private gamepad-database-setting-path ["input" "gamepad_database"])

(defn- ignored-setting?
  [{:keys [path]}]
  (or (= ["project" "dependencies"] path)
      (= gamepad-database-setting-path path)))

;; Transform a settings map with build-time settings conversions.
(defn- transform-settings! [settings]
  ;; Map deprecated 'variable_dt' to new values for same runtime behavior
  (if (= true (get settings ["display", "variable_dt"] false))
    (-> settings
        (assoc  ["display", "vsync"] false)
        (assoc  ["display", "update_frequency"] 0))
    settings))

(defn- explicit-empty-setting? [raw-settings path]
  (= "" (settings-core/get-setting raw-settings path)))

(defn- built-resource-setting [path value meta-settings path->built-resource-settings dep-resources]
  (let [meta-setting (settings-core/get-meta-setting meta-settings path)
        build-resource (path->built-resource-settings path)]
    (cond
      (:unknown-setting meta-setting)
      {:path path :value value}

      build-resource
      {:path path :value (resource/proj-path (dep-resources build-resource))}

      (and (= gamepads-setting-path path)
           (= "" value))
      {:path path :value value}

      (and (some? value) (not= "" value))
      {:path path
       :value (if (= :resource (:type meta-setting))
                (resource/proj-path value)
                (settings-core/render-raw-setting-value meta-setting value))})))

(defn- build-game-project [resource dep-resources user-data]
  (let [{:keys [settings-map raw-settings meta-settings path->built-resource-settings]} user-data
        settings-map (cond-> (transform-settings! settings-map)
                             (explicit-empty-setting? raw-settings gamepads-setting-path)
                             (assoc gamepads-setting-path ""))
        settings (into []
                       (comp (keep (fn [[path value]]
                                     (built-resource-setting path value meta-settings path->built-resource-settings dep-resources)))
                             (remove ignored-setting?))
                       (sort-by first settings-map))
        user-data-content (settings-core/settings->str settings meta-settings :comma-separated-list)]
    {:resource resource :content (.getBytes user-data-content)}))

(defonce/record CustomResource [resource]
  ;; Only purpose is to provide resource-type with :build-ext = :ext
  resource/Resource
  (children [this] (resource/children resource))
  (ext [this] (resource/ext resource))
  (resource-type [this]
    (let [ext (resource/ext this)]
      {:ext ext
       :label (localization/message "resource.type.custom")
       :build-ext ext}))
  (source-type [this] (resource/source-type resource))
  (exists? [this] (resource/exists? resource))
  (read-only? [this] (resource/read-only? resource))
  (symlink? [this] (resource/symlink? resource))
  (path [this] (resource/path resource))
  (abs-path [this] (resource/abs-path resource))
  (proj-path [this] (resource/proj-path resource))
  (resource-name [this] (resource/resource-name resource))
  (workspace [this] (resource/workspace resource))
  (resource-hash [this] (resource/resource-hash resource))
  (openable? [this] (resource/openable? resource))
  (editable? [this] (resource/editable? resource))
  (loaded? [this] (resource/loaded? resource))

  io/IOFactory
  (make-input-stream  [this opts] (io/input-stream resource))
  (make-reader        [this opts] (io/reader resource))
  (make-output-stream [this opts] (io/output-stream resource))
  (make-writer        [this opts] (io/writer resource)))

(defn- strip-trailing-slash [path]
  (string/replace path #"/*$" ""))

(defn- file-resource? [resource]
  (= (resource/source-type resource) :file))

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
   ["input" "gamepads"] [[:build-targets :gamepads-build-targets]
                         [:_node-id :gamepads-node-id]
                         [:resource :gamepads-resource]
                         [:pb :gamepads-pb]]
   ["input" "gamepad_database"] [[:resource :gamepad-database-resource]
                                 [:lines :gamepad-database-lines]]
   ["input" "game_binding"] [[:build-targets :dep-build-targets]]})

(defn- gamepad-database-error [_node-id gamepad-database-resource]
  (when (and gamepad-database-resource
             (resource/exists? gamepad-database-resource)
             (not= "txt" (resource/ext gamepad-database-resource)))
    (g/->error _node-id :build-targets :fatal gamepad-database-resource
               (localization/message "error.game-project.gamepad-database-must-be-txt"))))

(g/defnk produce-build-targets [_node-id build-errors resource settings-map raw-settings meta-info custom-build-targets resource-settings dep-build-targets gamepads-build-targets gamepads-node-id gamepads-resource gamepads-pb gamepad-database-resource gamepad-database-lines]
  (let [gamepads-empty (explicit-empty-setting? raw-settings gamepads-setting-path)
        gamepad-database-empty (explicit-empty-setting? raw-settings gamepad-database-setting-path)
        gamepads-build-targets (when-not gamepads-empty gamepads-build-targets)
        gamepads-resource (when-not gamepads-empty gamepads-resource)
        gamepads-pb (when-not gamepads-empty gamepads-pb)
        gamepad-database-resource (when-not gamepad-database-empty gamepad-database-resource)
        gamepad-database-lines (when-not gamepad-database-empty gamepad-database-lines)]
    (g/precluding-errors [(some-> (g/flatten-errors build-errors) (assoc :_node-id _node-id))
                          gamepads-pb
                          gamepad-database-lines
                          (gamepad-database-error _node-id gamepad-database-resource)]
     (let [gamepads-build-target (if (and gamepads-build-targets
                                           (not gamepad-database-resource))
                                    (peek gamepads-build-targets)
                                    (gamepads/make-build-target (or gamepads-node-id _node-id) gamepads-resource gamepads-pb gamepad-database-resource gamepad-database-lines))
           dep-build-targets (cond-> (vec (into (flatten dep-build-targets) custom-build-targets))
                                     gamepads-build-target (conj gamepads-build-target))
           deps-by-source (into {} (map (fn [{build-resource :resource}]
                                          [(:resource build-resource) build-resource]))
                                dep-build-targets)
           path->built-resource-settings (cond-> (into {} (keep (fn [{:keys [path value]}]
                                                                  (when (resource-setting-connections-template path)
                                                                    [path (deps-by-source value)])))
                                                                resource-settings)
                                         gamepads-build-target
                                         (assoc gamepads-setting-path (:resource gamepads-build-target)))]
       [(bt/with-content-hash
          {:node-id _node-id
           :resource (workspace/make-build-resource resource)
           :build-fn build-game-project
           :user-data {:settings-map settings-map
                       :raw-settings raw-settings
                       :meta-settings (:settings meta-info)
                       :path->built-resource-settings path->built-resource-settings}
           :deps dep-build-targets})]))))

(g/defnode GameProjectNode
  (inherits resource-node/ResourceNode)

  (input display-profiles-data g/Any)
  (output display-profiles-data g/Any (gu/passthrough display-profiles-data))

  (input texture-profiles-data g/Any)
  (output texture-profiles-data g/Any (gu/passthrough texture-profiles-data))

  (input settings-map g/Any)
  ;; settings-map already cached in SettingsNode
  (output settings-map g/Any (gu/passthrough settings-map))

  (input form-data g/Any)
  (output form-data g/Any :cached (gu/passthrough form-data))

  (input raw-settings g/Any)
  (input resource-settings g/Any)

  (input gamepads-resource resource/Resource)
  (input gamepads-build-targets g/Any)
  (input gamepads-node-id g/NodeID)
  (input gamepads-pb g/Any)
  (input gamepad-database-resource resource/Resource)
  (input gamepad-database-lines g/Any)

  (input resource-map g/Any)
  (input resource-snapshot g/Any)
  (input dep-build-targets g/Any :array)
  (input meta-info g/Any)

  (input build-errors g/Any :array)

  (output ssl-certificates-directory-resource g/Any
          (g/fnk [_node-id settings-map]
            (let [directory-resource (get settings-map ["network" "ssl_certificates"])]
              (if (or (nil? directory-resource)
                      (resource/exists? directory-resource))
                directory-resource
                (g/map->error
                  {:_node-id _node-id
                   :severity :fatal
                   :message (format "SSL certificates directory not found: '%s'" (resource/proj-path directory-resource))})))))

  (output custom-resources-directory-resources g/Any
          (g/fnk [_node-id resource-map settings-map]
            (let [custom-resources-setting (get settings-map ["project" "custom_resources"])
                  directory-proj-paths (parse-custom-resource-paths custom-resources-setting)

                  directory-resources
                  (coll/into-> directory-proj-paths []
                    (map (fn [directory-proj-path]
                           (or (get resource-map directory-proj-path)
                               (g/map->error
                                 {:_node-id _node-id
                                  :severity :fatal
                                  :message (format "Custom resources directory not found: '%s'" directory-proj-path)})))))]

              (g/precluding-errors directory-resources
                directory-resources))))

  (output custom-resource+versions g/Any :cached
          (g/fnk [custom-resources-directory-resources resource-snapshot ssl-certificates-directory-resource]
            ;; We depend on the resource-snapshot to ensure this output reflects
            ;; the on-disk state of all the involved resources.
            (let [status-map (:status-map resource-snapshot)

                  directory-resources
                  (cond-> custom-resources-directory-resources
                          ssl-certificates-directory-resource (conj ssl-certificates-directory-resource))

                  custom-resources
                  (coll/into-> directory-resources []
                    (map resource/resource-seq)
                    coll/flatten-xf
                    (distinct)
                    (filter file-resource?))]

              ;; We include the version only to ensure this output is
              ;; invalidated if any of the included files change on disk.
              (coll/into-> custom-resources []
                (map (fn [resource]
                       (let [proj-path (resource/proj-path resource)
                             resource-status (status-map proj-path)
                             version (:version resource-status)]
                         (pair resource version))))))))

  (output custom-build-targets g/Any :cached
          (g/fnk [_node-id custom-resource+versions]
            (try
              (mapv (fn [[source-resource _version]]
                      ;; We don't actually need the version here, since we will
                      ;; generate a hash from the contents of each file.
                      (pipeline/make-source-bytes-build-target _node-id (->CustomResource source-resource)))
                    custom-resource+versions)
              (catch Throwable error
                (g/map->error
                  {:_node-id _node-id
                   :_label :custom-build-targets
                   :message (ex-message error)
                   :severity :fatal})))))

  (input save-value g/Any)
  (output save-value g/Any (gu/passthrough save-value))

  (output build-targets g/Any :cached produce-build-targets))

;;; loading node

(defn- load-game-project [project self resource source-value]
  (let [graph-id (g/node-id->graph-id self)
        workspace (resource/workspace resource)
        resource-setting-connections (reduce-kv (fn [m k v] (assoc m k [self v])) {} resource-setting-connections-template)]
    (concat
      (g/connect workspace :resource-map self :resource-map)
      (g/connect workspace :resource-snapshot self :resource-snapshot)
      (g/make-nodes graph-id [settings-node settings/SettingsNode]
        (g/connect settings-node :_node-id self :nodes)
        (g/connect settings-node :settings-map self :settings-map)
        (g/connect settings-node :save-value self :save-value)
        (g/connect settings-node :form-data self :form-data)
        (g/connect settings-node :raw-settings self :raw-settings)
        (g/connect settings-node :meta-info self :meta-info)
        (g/connect settings-node :resource-settings self :resource-settings)
        (g/connect settings-node :setting-errors self :build-errors)
        (settings/load-settings-node project self settings-node resource source-value gpcore/basic-meta-info resource-setting-connections)))))

;; Test support

(defn set-setting!
  "Exposed for tests"
  [game-project path value]
  (g/transact (form/set-value (:form-ops (g/node-value game-project :form-data)) path value)))

(defn get-setting
  ([game-project path]
   (g/with-auto-evaluation-context evaluation-context
     (get-setting game-project path evaluation-context)))
  ([game-project path evaluation-context]
   ((g/node-value game-project :settings-map evaluation-context) path)))

(defn register-resource-types [workspace]
  (resource-node/register-settings-resource-type workspace
    :ext "project"
    :label (localization/message "resource.type.project")
    :node-type GameProjectNode
    :load-fn load-game-project
    :meta-settings (:settings gpcore/basic-meta-info)
    :icon game-project-icon
    :icon-class :property
    :view-types [:cljfx-form-view :text]
    :language "ini"
    :view-opts {:text {:grammar ini/grammar}}))
