(ns editor.docs
  (:require [clojure.java.io :as io]
            [clojure.string :as string]
            [editor.editor-extensions.docs :as ext-docs]
            [editor.util :as util])
  (:import [java.io Writer]))

(defn- script-doc-group [script-doc]
  (if (= "editor" (:name script-doc))
    :module
    (case (:type script-doc)
      :function :function
      (:variable :constant) :variable
      :discard)))

(defn- write-as-comment [^Writer w s]
  (let [lines (string/split-lines s)]
    (when (seq lines)
      (.write w "/*# ")
      (.write w ^String (first lines))
      (.write w "\n")
      (doseq [line (rest lines)]
        (.write w " * ")
        (.write w ^String line)
        (.write w "\n"))
      (.write w " */\n"))))

(defn- ->brief [description]
  (let [s (first (string/split-lines description))]
    (str (util/lower-case* (subs s 0 1))
         (subs s 1))))

(defn params->string [tag parameters]
  (->> parameters
       (map (fn [{:keys [name types doc]}]
              (str "@" tag " " name " [type:" (string/join "|" types) "] " (string/replace doc "\n" " "))))
       (string/join "\n")))

(defn- write-docs [output-dir]
  (let [groups (group-by script-doc-group (ext-docs/editor-script-docs))
        {functions :function variables :variable} groups
        docs (into
               []
               cat
               [(eduction
                  (map (fn [m] (assoc m :type :variable)))
                  variables)
                (eduction
                  (map (fn [m]
                         (-> m
                             (update :returnvalues (fn [returnvalues]
                                                     (mapv
                                                       (fn [m]
                                                         (update m :doc #(or % "")))
                                                       returnvalues))))))
                  functions)])]
    (with-open [w (io/writer (doto (io/file output-dir "editor.apidoc") io/make-parents))]
      (write-as-comment w (format "Editor scripting documentation\n\n@document\n@name Editor\n@namespace editor"))
      (doseq [{:keys [description type name] :as doc} docs
              :let [brief (->brief description)]]
        (case type
          :variable
          (write-as-comment w (str brief "\n\n" description "\n\n" "@variable\n@name " name))
          :function
          (let [{:keys [returnvalues parameters examples]} doc]
            (write-as-comment w (str brief "\n\n" description "\n\n"
                                     "@name " name "\n"
                                     (params->string "param" parameters) "\n"
                                     (params->string "return" returnvalues) "\n"
                                     (when examples
                                       (str "@examples\n\n" examples "\n"))))))))))

(defn -main [output-dir]
  (write-docs output-dir))

(comment

  (write-docs "target/docs")

  #__)