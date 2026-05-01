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
