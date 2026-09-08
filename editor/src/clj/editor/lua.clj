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

(ns editor.lua
  (:require [clojure.string :as string]
            [editor.fs :as fs]
            [editor.protobuf :as protobuf]
            [internal.java :as java]
            [service.log :as log]
            [util.coll :as coll])
  (:import [com.dynamo.scriptdoc.proto ScriptDoc$Document]
           [java.io IOException]
           [java.nio.charset MalformedInputException]
           [org.apache.commons.io FilenameUtils]))

(set! *warn-on-reflection* true)

(def preinstalled-modules
  (coll/into-> (fs/class-path-walker java/class-loader "doc") #{}
    (mapcat
      (fn [path]
        (when-let [{:keys [info elements]}
                   (try
                     (protobuf/read-map-with-defaults ScriptDoc$Document path)
                     (catch MalformedInputException _
                       (log/warn :message "Ignoring legacy-format documentation file." :path (str path))
                       nil)
                     (catch IOException e
                       (log/error :message "Failed to read documentation file." :path (str path) :exception e)
                       nil))]
          (when (= "Lua" (:language info))
            (coll/into-> elements []
              (remove #(= :typedef (:type %)))
              (mapcat
                (fn [{:keys [name]}]
                  (let [name-parts (string/split name #"\.")]
                    (loop [index 1
                           modules []]
                      (if (= index (count name-parts))
                        modules
                        (recur (inc index)
                               (conj modules (coll/join-to-string "." (subvec name-parts 0 index))))))))))))))))

(defn lua-module->path [module]
  (str "/" (string/replace module #"\." "/") ".lua"))

(defn path->lua-module [path]
  (-> (if (string/starts-with? path "/") (subs path 1) path)
      (string/replace #"/" ".")
      (FilenameUtils/removeExtension)))

(defn lua-module->build-path [module]
  (str (lua-module->path module) "c"))
