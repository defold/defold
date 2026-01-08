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

(ns editor.engine-profiler
  (:require [clojure.java.io :as io]
            [editor.handler :as handler]
            [editor.targets :as targets]
            [editor.ui :as ui]
            [util.http-server :as http-server]))

(def ^:private port 17815)

(handler/defhandler :run.open-profiler :global
  (run [web-server prefs]
       (if (nil? (:address (targets/selected-target prefs)))
         (ui/open-url (format "%s/engine-profiler" (http-server/local-url web-server)))
         (ui/open-url (format "%s/engine-profiler?addr=%s:%d" (http-server/local-url web-server) (:address (targets/selected-target prefs)) port)))))

(handler/defhandler :run.open-resource-profiler :global
  (enabled? [prefs] (some? (targets/selected-target prefs)))
  (run [prefs]
       (let [address (:address (targets/selected-target prefs))]
         (ui/open-url (format "http://%s:8002/" address)))))

(defn routes []
  {"/engine-profiler/{*path}"
   {"GET" (fn [request]
            (let [path (:path (:path-params request))
                  path (str "engine-profiler/" (if (= "" path) "remotery/vis/patched.index.html" path))
                  resource (io/resource path)]
              (if resource
                (http-server/response 200 resource)
                http-server/not-found)))}})
