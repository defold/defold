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

(ns editor.web-server
  (:require [editor.command-requests :as command-requests]
            [editor.console :as console]
            [editor.engine-profiler :as engine-profiler]
            [editor.hot-reload :as hot-reload]
            [editor.pipeline.bob :as bob]
            [editor.web-profiler :as web-profiler]
            [util.http-server :as http-server]))

(defn handler [workspace project console-view ui-node render-reload-progress!]
  (http-server/router-handler
    (into [] cat [(engine-profiler/routes)
                  (web-profiler/routes)
                  (console/routes console-view)
                  (hot-reload/routes workspace)
                  (bob/routes project)
                  (command-requests/router ui-node render-reload-progress!)])))
