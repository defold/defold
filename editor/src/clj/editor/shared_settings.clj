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

(ns editor.shared-settings
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

(def ^:private project-shared-settings-filename "project.shared_settings")

(def project-shared-settings-proj-path (str \/ project-shared-settings-filename))

(def ^:private shared-settings-icon "icons/32/Icons_05-Project-info.png")

(def ^:private shared-settings-meta "shared-settings-meta.edn")

(def ^:private shared-settings-meta-info
  (delay
    (with-open [resource-reader (settings-core/resource-reader shared-settings-meta)
                pushback-reader (settings-core/pushback-reader resource-reader)]
      (settings-core/load-meta-info pushback-reader))))

(defn register-resource-types [workspace]
  (settings/register-simple-settings-resource-type workspace
    :ext "shared_settings"
    :label "Shared Settings"
    :icon shared-settings-icon
    :meta-info @shared-settings-meta-info))

(defn- report-load-error! [^File shared-settings-file ^Throwable exception]
  (let [header-message "Failed to load Shared Settings file."
        sub-header-message "Falling back to defaults. Your user experience might suffer."
        file-info-message (str "File: " (.getAbsolutePath shared-settings-file))
        exception-message (ex-message exception)]
    (log/warn :msg (str header-message " " sub-header-message " " file-info-message)
              :exception exception)
    (ui/run-later
      (dialogs/make-info-dialog
        {:title "Error Loading Shared Settings"
         :icon :icon/triangle-error
         :header {:fx/type fx.v-box/lifecycle
                  :children [{:fx/type fxui/label
                              :variant :header
                              :text header-message}
                             {:fx/type fxui/label
                              :text sub-header-message}]}
         :content (str file-info-message "\n\n" exception-message)}))))

(defn- read-raw-settings-or-exception [^File shared-settings-file]
  (try
    (with-open [reader (io/reader shared-settings-file)]
      (settings-core/parse-settings reader))
    (catch Exception exception
      exception)))

(defn- parse-system-config-or-exception [raw-settings]
  (try
    (let [meta-settings (:settings @shared-settings-meta-info)
          settings (settings-core/sanitize-settings meta-settings raw-settings)
          editor-cache-capacity (settings-core/get-setting settings ["editor" "cache_capacity"])]
      (when (some? editor-cache-capacity)
        (if (< -1 editor-cache-capacity)
          {:cache-size editor-cache-capacity}
          (throw (ex-info "editor.cache_capacity must be -1 (unlimited), 0 (disabled), or positive."
                          {:editor-cache-capacity editor-cache-capacity})))))
    (catch Exception exception
      exception)))

(def ^:private default-system-config {})

(defn- load-system-config [^File shared-settings-file]
  (log/info :message "Loading system config from Shared Settings file.")
  (let [raw-settings-or-exception (read-raw-settings-or-exception shared-settings-file)]
    (or (if (ex-message raw-settings-or-exception)
          (report-load-error! shared-settings-file raw-settings-or-exception)
          (let [system-config-or-exception (parse-system-config-or-exception raw-settings-or-exception)]
            (if (ex-message system-config-or-exception)
              (report-load-error! shared-settings-file system-config-or-exception)
              (when (not-empty system-config-or-exception)
                (log/info :message "Using system config from Shared Settings file." :config system-config-or-exception)
                system-config-or-exception))))
        default-system-config)))

;; Called through reflection.
(defn load-project-system-config [project-directory]
  (let [shared-settings-file (io/file project-directory project-shared-settings-filename)]
    (if (.isFile shared-settings-file)
      (load-system-config shared-settings-file)
      {})))
