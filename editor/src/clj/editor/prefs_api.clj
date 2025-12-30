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

(ns editor.prefs-api
  (:require [clojure.data.json :as json]
            [clojure.java.io :as io]
            [editor.prefs :as prefs]
            [util.http-server :as http-server]))

(defonce ^:const url-prefix "/prefs")
(defonce ^:const allowed-keys [:code :tools])

(set! *warn-on-reflection* true)

(defn- json-response [status content]
  (http-server/response
   status
   {"content-type" (http-server/ext->content-type "json")}
   (json/write-str content)))

(defn- read-json-body [request]
  (json/read (io/reader (:body request)) :key-fn keyword))

(defn handle-prefs [_]
  (json-response 200 (select-keys (prefs/get (prefs/global) []) allowed-keys)))

(defn handle-submit-prefs [request]
  (try
    (prefs/set! (prefs/global) [] (select-keys (read-json-body request) allowed-keys))
    (prefs/sync!)
    (json-response 200 {:status "OK"})

    (catch Exception e
      (json-response 500 {:error (.getMessage e)}))))

(defn router []
  {"/prefs" {"GET"  #'handle-prefs
             "POST" #'handle-submit-prefs}})


