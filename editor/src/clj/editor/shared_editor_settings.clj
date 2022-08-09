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

(ns editor.shared-editor-settings
  (:require [cljfx.fx.v-box :as fx.v-box]
            [clojure.java.io :as io]
            [editor.dialogs :as dialogs]
            [editor.fxui :as fxui]
            [editor.settings :as settings]
            [editor.settings-core :as settings-core]
            [editor.ui :as ui]
            [service.log :as log])
  (:import [java.io File]))

(set! *warn-on-reflection* true)

(def ^:private project-shared-editor-settings-filename "project.shared_editor_settings")

(def project-shared-editor-settings-proj-path (str \/ project-shared-editor-settings-filename))

(def ^:private shared-editor-settings-icon "icons/32/Icons_05-Project-info.png")

(def ^:private shared-editor-settings-meta "shared-editor-settings-meta.edn")

(def ^:private shared-editor-settings-meta-info
  (delay
    (with-open [resource-reader (settings-core/resource-reader shared-editor-settings-meta)
                pushback-reader (settings-core/pushback-reader resource-reader)]
      (settings-core/load-meta-info pushback-reader))))

(defn register-resource-types [workspace]
  (settings/register-simple-settings-resource-type workspace
    :ext "shared_editor_settings"
    :label "Shared Editor Settings"
    :icon shared-editor-settings-icon
    :meta-info @shared-editor-settings-meta-info))

(defn- report-load-error! [^File shared-editor-settings-file ^Throwable exception]
  (let [header-message "Failed to load Shared Editor Settings file."
        sub-header-message "Falling back to defaults. Your user experience might suffer."
        file-info-message (str "File: " (.getAbsolutePath shared-editor-settings-file))
        exception-message (ex-message exception)]
    (log/warn :msg (str header-message " " sub-header-message " " file-info-message)
              :exception exception)
    (ui/run-later
      (dialogs/make-info-dialog
        {:title "Error Loading Shared Editor Settings"
         :icon :icon/triangle-error
         :always-on-top true
         :header {:fx/type fx.v-box/lifecycle
                  :children [{:fx/type fxui/label
                              :variant :header
                              :text header-message}
                             {:fx/type fxui/label
                              :text sub-header-message}]}
         :content (str file-info-message "\n\n" exception-message)}))))

(defn- read-raw-settings-or-exception [^File shared-editor-settings-file]
  (try
    (with-open [reader (io/reader shared-editor-settings-file)]
      (settings-core/parse-settings reader))
    (catch Exception exception
      exception)))

(defn- parse-system-config-or-exception [raw-settings]
  (try
    (let [meta-settings (:settings @shared-editor-settings-meta-info)
          settings (settings-core/sanitize-settings meta-settings raw-settings)
          cache-capacity (settings-core/get-setting settings ["performance" "cache_capacity"])]
      (when (some? cache-capacity)
        (if (<= -1 cache-capacity)
          {:cache-size cache-capacity}
          (throw (ex-info "performance.cache_capacity must be -1 (unlimited), 0 (disabled), or positive."
                          {:cache-capacity cache-capacity})))))
    (catch Exception exception
      exception)))

(def ^:private default-system-config {})

(defn- load-system-config [^File shared-editor-settings-file]
  (log/info :message "Loading system config from Shared Editor Settings file.")
  (let [raw-settings-or-exception (read-raw-settings-or-exception shared-editor-settings-file)]
    (or (if (ex-message raw-settings-or-exception)
          (report-load-error! shared-editor-settings-file raw-settings-or-exception)
          (let [system-config-or-exception (parse-system-config-or-exception raw-settings-or-exception)]
            (if (ex-message system-config-or-exception)
              (report-load-error! shared-editor-settings-file system-config-or-exception)
              (when (not-empty system-config-or-exception)
                (log/info :message "Using system config from Shared Editor Settings file." :config system-config-or-exception)
                system-config-or-exception))))
        default-system-config)))

;; Called through reflection.
(defn load-project-system-config [project-directory]
  (let [shared-editor-settings-file (io/file project-directory project-shared-editor-settings-filename)]
    (if (.isFile shared-editor-settings-file)
      (load-system-config shared-editor-settings-file)
      default-system-config)))
