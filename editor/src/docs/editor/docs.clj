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

(ns editor.docs
  (:require [clojure.java.io :as io]
            [clojure.string :as string]
            [editor.editor-extensions.docs :as ext-docs]
            [editor.util :as util]
            [util.coll :as coll])
  (:import [java.io Writer]))

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
              (str "@" tag " " name " [type:" (coll/join-to-string "|" types) "] " (string/replace (str doc) "\n" " "))))
       (coll/join-to-string "\n")))

(defn- write-docs [output-dir]
  (with-open [w (io/writer (doto (io/file output-dir "editor.apidoc") io/make-parents))]
    (write-as-comment w "Editor scripting documentation\n\n@document\n@name Editor\n@namespace editor\n@language Lua")
    (doseq [{:keys [description type name] :as doc} (ext-docs/editor-script-docs)
            :let [brief (->brief description)]]
      (case type
        :variable
        (write-as-comment w (str brief "\n\n" description "\n\n" "@variable\n@name " name))

        :constant
        (let [{:keys [types]} doc]
          (write-as-comment w (str brief "\n\n" description "\n\n"
                                   "@constant"
                                   (when (seq types)
                                     (str " [type:" (coll/join-to-string "|" types) "]"))
                                   "\n@name " name)))
        :enum
        (let [{:keys [types]} doc]
          (write-as-comment w (str brief "\n\n" description "\n\n"
                                   "@enum\n@name " name "\n"
                                   (when (seq types)
                                     (str "@param value [type:" (coll/join-to-string "|" types) "] enum value")))))
        :struct
        (let [{:keys [members]} doc]
          (write-as-comment w (str brief "\n\n" description "\n\n"
                                   "@struct\n@name " name "\n"
                                   (params->string "member" members))))
        :typedef
        (let [{:keys [types]} doc]
          (write-as-comment w (str brief "\n\n" description "\n\n"
                                   "@typedef\n@name " name "\n"
                                   (params->string "param" [{:name "value" :types types :doc brief}]))))
        :function
        (let [{:keys [returnvalues parameters examples]} doc]
          (write-as-comment w (str brief "\n\n" description "\n\n"
                                   "@name " name "\n"
                                   (params->string "param" parameters) "\n"
                                   (params->string "return" returnvalues) "\n"
                                   (when examples
                                     (str "@examples\n\n" examples "\n")))))
        
        nil))))

(defn -main [output-dir]
  (write-docs output-dir))

(comment

  (write-docs "target/docs")

  #__)
