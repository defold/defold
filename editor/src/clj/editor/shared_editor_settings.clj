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

(ns editor.shared-editor-settings
  (:require [cljfx.fx.v-box :as fx.v-box]
            [clojure.java.io :as io]
            [dynamo.graph :as g]
            [editor.defold-project :as project]
            [editor.dialogs :as dialogs]
            [editor.fxui :as fxui]
            [editor.localization :as localization]
            [editor.settings :as settings]
            [editor.settings-core :as settings-core]
            [editor.ui :as ui]
            [service.log :as log])
  (:import [java.io File]))

(set! *warn-on-reflection* true)

(def ^:private shared-editor-settings-icon "icons/32/Icons_05-Project-info.png")

(def ^:private project-shared-editor-settings-filename "project.shared_editor_settings")

(def project-shared-editor-settings-proj-path (str \/ project-shared-editor-settings-filename))

(def ^:private setting-paths
  {:cache-capacity ["performance" "cache_capacity"]
   :non-editable-directories ["performance" "non_editable_directories"]
   :build-server ["extensions" "build_server"]})

(def ^:private meta-info
  (settings-core/finalize-meta-info
    {:settings [{:path (:cache-capacity setting-paths)
                 :type :integer
                 :default 20000
                 :label "Cache Size"
                 :help "Maximum number of temporary cache entries. Increase if you experience frequent editor slowdowns. Use -1 for unlimited, 20000 by default."}
                {:path (:non-editable-directories setting-paths)
                 :type :list
                 :label "Non-editable Directories"
                 :help "Project directories whose contents can't be edited. Use for externally generated content to conserve memory and project load time."
                 :element {:type :directory
                           :in-project true}}
                {:path (:build-server setting-paths)
                 :type :string
                 :label "Build Server"
                 :help "Build server URL used for building native extensions"}]
     :group-order ["Shared Settings"]
     :default-category "performance"
     :categories {"performance" {:help "Editor performance tweaks for your project. Some settings may require restarting the editor to take effect."
                                 :group "Shared Settings"}
                  "extensions" {:help "Common settings for native extensions"
                                :group "Shared Settings"}}}))

(defn shared-editor-settings-file
  ^File [project-directory]
  (io/file project-directory project-shared-editor-settings-filename))

(defn get-setting [project setting-path evaluation-context]
  (when-let [settings-node (project/get-resource-node project project-shared-editor-settings-proj-path evaluation-context)]
    (get (g/node-value settings-node :settings-map evaluation-context) setting-path)))

(defn- map->settings [map]
  (let [meta-settings (:settings meta-info)]
    (reduce-kv (fn [settings key value]
                 (if-some [path (setting-paths key)]
                   (settings-core/set-raw-setting settings meta-settings path value)
                   (throw (ex-info (str "No shared editor setting path for key: " key)
                                   {:key key
                                    :meta-settings meta-settings}))))
               []
               map)))

(defn register-resource-types [workspace]
  (settings/register-simple-settings-resource-type workspace
    :ext "shared_editor_settings"
    :label "Shared Editor Settings"
    :icon shared-editor-settings-icon
    :meta-info meta-info))

(defn- report-load-error! [^File shared-editor-settings-file ^Throwable exception localization]
  (let [header-pattern (localization/message "dialog.shared-editor-settings.load-error.header")
        detail-pattern (localization/message "dialog.shared-editor-settings.load-error.detail")
        content-pattern (localization/message "dialog.shared-editor-settings.load-error.content"
                                              {"file" (.getAbsolutePath shared-editor-settings-file)
                                               "error" (str (ex-message exception))})
        header-text (localization header-pattern)
        detail-text (localization detail-pattern)]
    (log/warn :msg (str header-text " " detail-text " " (localization content-pattern))
              :exception exception)
    (ui/run-later
      (dialogs/make-info-dialog
        localization
        {:title (localization/message "dialog.shared-editor-settings.load-error.title")
         :icon :icon/triangle-error
         :always-on-top true
         :header {:fx/type fx.v-box/lifecycle
                  :children [{:fx/type fxui/legacy-label
                              :variant :header
                              :text header-text}
                             {:fx/type fxui/legacy-label
                              :text detail-text}]}
         :content content-pattern}))))

(defn- load-config [^File shared-editor-settings-file parse-config-fn localization]
  (let [raw-settings-or-exception (try
                                    (with-open [reader (io/reader shared-editor-settings-file)]
                                      (settings-core/parse-settings reader))
                                    (catch Exception exception
                                      exception))]
    (if (ex-message raw-settings-or-exception)
      (report-load-error! shared-editor-settings-file raw-settings-or-exception localization)
      (let [meta-settings (:settings meta-info)
            config-or-exception (try
                                  (let [settings (settings-core/sanitize-settings meta-settings raw-settings-or-exception)]
                                    (parse-config-fn settings))
                                  (catch Exception exception
                                    exception))]
        (if (ex-message config-or-exception)
          (report-load-error! shared-editor-settings-file config-or-exception localization)
          (when (not-empty config-or-exception)
            config-or-exception))))))

(defn- load-project-config [project-directory config-type parse-config-fn localization]
  {:pre [(keyword? config-type)
         (ifn? parse-config-fn)]}
  (let [shared-editor-settings-file (shared-editor-settings-file project-directory)]
    (when (.isFile shared-editor-settings-file)
      (log/info :message (str "Loading " (name config-type) " from Shared Editor Settings file."))
      (when-some [config (not-empty (load-config shared-editor-settings-file parse-config-fn localization))]
        (log/info :message (str "Using " (name config-type) " from Shared Editor Settings file.") config-type config)
        config))))

(defn- parse-system-config [settings]
  (let [cache-capacity (settings-core/get-setting settings (:cache-capacity setting-paths))]
    (when (some? cache-capacity)
      (if (<= -1 cache-capacity)
        {:cache-size cache-capacity}
        (throw (ex-info "performance.cache_capacity must be -1 (unlimited), 0 (disabled), or positive."
                        {:cache-capacity cache-capacity}))))))

(defn non-editable-directory-proj-path? [value]
  ;; Value must be a string starting with "/", but not ending with "/".
  (and (string? value)
       (re-matches #"^\/.*(?<!\/)$" value)))

(defn- parse-workspace-config [settings]
  (let [non-editable-directories (settings-core/get-setting settings (:non-editable-directories setting-paths))]
    (when (seq non-editable-directories)
      (if (every? non-editable-directory-proj-path? non-editable-directories)
        {:non-editable-directories non-editable-directories}
        (throw (ex-info "Every entry in performance.non_editable_directories must be a proj-path starting with `/`, but not ending with `/`."
                        {:non-editable-directories non-editable-directories}))))))

(def ^:private default-system-config {})

(def ^:private default-workspace-config {})

;; Called through reflection.
(defn load-project-system-config [project-directory localization]
  (or (load-project-config project-directory :system-config parse-system-config localization)
      default-system-config))

(defn load-project-workspace-config [project-directory localization]
  (or (load-project-config project-directory :workspace-config parse-workspace-config localization)
      default-workspace-config))

;; Used by tests.
(defn map->save-data-content
  ^String [config-map]
  (let [meta-settings (:settings meta-info)
        settings (map->settings config-map)]
    (settings-core/settings->str settings meta-settings :multi-line-list)))
