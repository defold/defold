(ns editor.docs
  (:require [clojure.data.json :as json]
            [clojure.java.io :as io]
            [clojure.string :as string]
            [editor.editor-extensions.docs :as ext-docs]
            [editor.protobuf :as protobuf]
            [editor.util :as util])
  (:import [com.dynamo.scriptdoc.proto ScriptDoc$Document]))

(defn- script-doc-group [script-doc]
  (if (= "editor" (:name script-doc))
    :module
    (case (:type script-doc)
      :function :function
      (:variable :constant) :variable
      :discard)))

(defn- produce-docs []
  (let [groups (group-by script-doc-group (ext-docs/editor-script-docs))
        {[module] :module functions :function variables :variable} groups]
    (protobuf/bytes->map-with-defaults
      ScriptDoc$Document
      (protobuf/map->bytes
        ScriptDoc$Document
        {:elements (into
                     []
                     cat
                     [(eduction
                        (map (fn [m]
                               (-> m
                                   (assoc :brief (first (string/split (:description m) #"\n" 2)))
                                   (update :returnvalues (fn [returnvalues]
                                                           (mapv
                                                             (fn [m]
                                                               (update m :doc #(or % "")))
                                                             returnvalues))))))
                        functions)
                      (eduction
                        (map (fn [m]
                               (assoc m :type :variable
                                        :brief (:description m))))
                        variables)])
         :info {:description (:description module)
                :namespace (:name module)
                :name "Editor"
                :brief "Editor scripting documentation"
                :path ""
                :file ""}}))))

(defn- write-docs [output-dir]
  (let [docs (produce-docs)]
    (with-open [w (io/writer (doto (io/file output-dir "editor_doc.json")
                               io/make-parents))]
      (json/write docs w
                  :value-fn (fn [k v]
                              (if (= k :type)
                                (util/upper-case* (name v))
                                v))
                  :indent true))
    (spit (io/file output-dir "editor_doc.sdoc") (protobuf/map->str ScriptDoc$Document docs))))

(defn -main [output-dir]
  (write-docs output-dir))

(comment

  (write-docs "target/docs")

  #__)