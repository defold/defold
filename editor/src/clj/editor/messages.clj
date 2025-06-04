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

(ns editor.messages
  (:import [java.util ResourceBundle])
  (:require [clojure.java.io :as io]))

(set! *warn-on-reflection* true)

(defn bundle-map
  [^ResourceBundle bundle]
  (let [ks (enumeration-seq (.getKeys bundle))]
    (zipmap ks (map #(.getString bundle %) ks))))

(defn load-bundle-in-namespace
  [ns-sym bundle]
  (create-ns ns-sym)
  (doseq [[k v] (bundle-map bundle)]
    (intern ns-sym (with-meta (symbol k) {:bundle bundle}) v)))

(defn resource-bundle
  [path]
  (ResourceBundle/getBundle path))

(load-bundle-in-namespace 'editor.messages (resource-bundle "messages"))
