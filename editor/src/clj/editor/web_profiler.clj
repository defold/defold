;; Copyright 2020 The Defold Foundation
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
  (:require [editor.handler :as handler]
            [editor.ui :as ui]
            [util.http-server :as http-server]
            [clojure.java.io :as io])
  (:import  [com.defold.util Profiler]))

(def ^:private template-path "profiler_template.html")
(def ^:private html (atom nil))

(defn- dump-profiler []
  (reset! html (-> (slurp (io/resource template-path))
                   (clojure.string/replace "$PROFILER_DATA" (Profiler/dumpJson)))))

(handler/defhandler :profile :global
  (enabled? [] true)
  (run [] (dump-profiler)))

(handler/defhandler :profile-show :global
  (enabled? [] true)
  (run [web-server]
       (dump-profiler)
       (ui/open-url (format "%s/profiler" (http-server/local-url web-server)))))

(defn handler [req]
  {:code 200
   :headers {"Content-Type" "text/html"}
   :body (or @html (dump-profiler))})
