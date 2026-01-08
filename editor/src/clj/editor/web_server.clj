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

(ns editor.web-server
  (:require [util.http-server :as http-server]))

(set! *warn-on-reflection* true)

(defn make-dynamic-handler
  "Create an HTTP request handler that supports setting dynamic routes

  See [[set-dynamic-routes!]]"
  [built-in-routes]
  (let [current-handler (atom (http-server/router-handler built-in-routes))]
    ^{::built-in-routes built-in-routes
      ::state current-handler}
    (fn handle-request [request]
      (@current-handler request))))

(defn set-dynamic-routes!
  "Set dynamic routes on the HTTP request handler

  If the new route set causes a conflict, throws an exception where ex-data is
  a map with the following keys:
    :type    :path-conflicts
    :data    a map from path+route-data to a set of conflicting path+route-data"
  [web-handler dynamic-routes]
  {:pre [(contains? (meta web-handler) ::state)]}
  (let [{::keys [built-in-routes state]} (meta web-handler)]
    (reset! state (http-server/router-handler (into built-in-routes dynamic-routes))))
  nil)
