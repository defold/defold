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

(ns editor.engine-profiler
  (:require [editor.handler :as handler]
            [editor.ui :as ui]
            [editor.targets :as targets]
            [util.http-server :as http-server]
            [clojure.java.io :as io])
  (:import (org.apache.commons.io FilenameUtils)))

(def ^:const url-prefix "/engine-profiler")
(def ^:private index-path "engine-profiler/remotery/vis/patched.index.html")
(def ^:private html (atom nil))
(def ^:private port 17815)

(defn- update-profiler-html! [target-address]
  (reset! html (-> (slurp (io/resource index-path))
                   ; In the future we might want to specify a special address to the device here
                   ;(clojure.string/replace "$TARGET_ADDRESS" target-address)
                   )))

(handler/defhandler :engine-profile-show :global
  (enabled? [] true)
  (run [web-server prefs]
       (update-profiler-html! (:address (targets/selected-target prefs)))
       (if (nil? (:address (targets/selected-target prefs)))
         (ui/open-url (format "%s/engine-profiler" (http-server/local-url web-server)))
         (ui/open-url (format "%s/engine-profiler?addr=%s:%d" (http-server/local-url web-server) (:address (targets/selected-target prefs)) port)))))

(defn- get-mime-type [path]
  (let [name (.getName (io/file path))
        suffix (FilenameUtils/getExtension name)
        mime-type (case suffix
                    "html" "text/html"
                    "js" "text/javascript"
                    "json" "application/json"
                    "css" "text/css"
                    "ttf" "font/ttf"
                    "text/html")]
    mime-type))

(defn- read-resource [req]
  (let [path (str "." (:url req)) ; Make it a relative path as it starts with a "/"
        mime-type (get-mime-type path)
        resource (io/resource path)
        data (if (= resource nil)
               nil
               (slurp resource))
        code (if (= data nil) 404 200)]
    {:code code
     :headers {"Content-Type" mime-type}
     :body data}))

(defn handler [req]
  (if (or (= (:url req) "/engine-profiler")
          (= (:url req) "/engine-profiler/")
          (.contains (:url req) "/engine-profiler?"))
    {:code 200
    :headers {"Content-Type" "text/html"
              "Content-Length" (str (count @html))
              "Server" "Defold Editor"}
    :body (or @html "")
    }
    (read-resource req)))
