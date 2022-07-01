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

(ns editor.editor-tweaks
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

(def ^:private editor-tweaks-icon "icons/32/Icons_05-Project-info.png")

(def ^:private editor-tweaks-meta "editor-tweaks-meta.edn")

(def ^:private editor-tweaks-meta-info
  (delay
    (with-open [resource-reader (settings-core/resource-reader editor-tweaks-meta)
                pushback-reader (settings-core/pushback-reader resource-reader)]
      (settings-core/load-meta-info pushback-reader))))

(defn register-resource-types [workspace]
  (settings/register-simple-settings-resource-type workspace
    :ext "editor_tweaks"
    :label "Editor Tweaks"
    :icon editor-tweaks-icon
    :meta-info @editor-tweaks-meta-info))

(defn- report-load-error! [^File editor-tweaks-file ^Throwable exception]
  (let [header-message "Failed to load project Editor Tweaks file."
        sub-header-message "Falling back to defaults. Your editor experience might suffer."
        file-info-message (str "File: " (.getAbsolutePath editor-tweaks-file))
        exception-message (ex-message exception)]
    (log/warn :msg (str header-message " " sub-header-message " " file-info-message)
              :exception exception)
    (ui/run-later
      (dialogs/make-info-dialog
        {:title "Error Loading Editor Tweaks"
         :icon :icon/triangle-error
         :header {:fx/type fx.v-box/lifecycle
                  :children [{:fx/type fxui/label
                              :variant :header
                              :text header-message}
                             {:fx/type fxui/label
                              :text sub-header-message}]}
         :content (str file-info-message "\n\n" exception-message)}))))

(defn- read-raw-settings-or-exception [^File editor-tweaks-file]
  (try
    (with-open [reader (io/reader editor-tweaks-file)]
      (settings-core/parse-settings reader))
    (catch Exception exception
      exception)))

(defn- parse-system-config-or-exception [raw-settings]
  (try
    (let [meta-settings (:settings @editor-tweaks-meta-info)
          settings (settings-core/sanitize-settings meta-settings raw-settings)
          graph-cache-capacity (settings-core/get-setting settings ["graph" "cache_capacity"])]
      (when (some? graph-cache-capacity)
        (if (< -1 graph-cache-capacity)
          {:cache-size graph-cache-capacity}
          (throw (ex-info "graph.cache_capacity must be -1 (unlimited), 0 (disabled), or positive."
                          {:graph-cache-capacity graph-cache-capacity})))))
    (catch Exception exception
      exception)))

(def ^:private default-system-config {})

(defn load-system-config [^File editor-tweaks-file]
  (log/info :msg "Loading system config from Editor Tweaks file.")
  (let [raw-settings-or-exception (read-raw-settings-or-exception editor-tweaks-file)]
    (or (if (ex-message raw-settings-or-exception)
          (report-load-error! editor-tweaks-file raw-settings-or-exception)
          (let [system-config-or-exception (parse-system-config-or-exception raw-settings-or-exception)]
            (if (ex-message system-config-or-exception)
              (report-load-error! editor-tweaks-file system-config-or-exception)
              (when (not-empty system-config-or-exception)
                (log/info :msg "Using system config from Editor Tweaks file." :config system-config-or-exception)
                system-config-or-exception))))
        default-system-config)))
