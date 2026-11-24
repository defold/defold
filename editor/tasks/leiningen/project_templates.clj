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

(ns leiningen.project-templates
  (:require [clojure.edn :as edn]
            [leiningen.util.http-cache :as http-cache]
            [clojure.java.io :as io])
  (:import [org.apache.commons.io FileUtils]))

(defn project-templates [_project]
  (FileUtils/deleteQuietly (io/file "resources/template-projects"))
  (->> (io/file "resources/welcome/welcome.edn")
       (slurp)
       (edn/read-string)
       :new-project
       :categories
       (eduction
         (mapcat :templates)
         (filter :bundle)
         (map (juxt #(http-cache/download (:zip-url %)) :name)))
       (run! (fn [[zip name]]
               (println (str "Bundle '" name "' template project"))
               (FileUtils/copyFile (io/file zip) (io/file "resources/template-projects" (str name ".zip")))))))
