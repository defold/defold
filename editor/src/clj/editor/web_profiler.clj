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

(ns editor.web-profiler
  (:require [clojure.java.io :as io]
            [clojure.string :as string]
            [editor.handler :as handler]
            [editor.system :as system]
            [editor.ui :as ui]
            [util.http-server :as http-server])
  (:import [com.defold.util Profiler]))

(when (system/defold-dev?)
  (handler/defhandler :dev.open-profiler :global
    (run [web-server]
      (ui/open-url (str (http-server/local-url web-server) "/profiler")))))

(defn routes []
  {"/profiler"
   {"GET" (fn [_]
            (http-server/response
              200
              {"content-type" "text/html"}
              (-> (slurp (io/resource "profiler_template.html"))
                  (string/replace "$PROFILER_DATA" (Profiler/dumpJson)))))}})