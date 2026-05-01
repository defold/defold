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

(ns editor.lua-completion
  (:require [clojure.string :as string]
            [editor.code-completion :as code-completion])
  (:import [java.net URI]))

(defn args-doc-html
  "Convert lua args data structure to html string

   Args is a collection of maps with the following keys:
     :name     string, required
     :doc      optional html string
     :types    coll of strings, i.e. union"
  [args]
  (str "<dl>"
       (->> args
            (map
              (fn [{:keys [name doc types]}]
                (format "<dt><code>%s%s</code></dt>%s"
                        name
                        (if (pos? (count types))
                          (format " <small>%s</small>" (string/join ", " types))
                          "")
                        (if doc
                          (format "<dd>%s</dd>" doc)
                          ""))))
            string/join)
       "</dl>"))

(defn- make-markdown-doc [{:keys [description type examples parameters] :as el} ^URI base-url site-url]
  (let [sections (cond-> []
                         (pos? (count description))
                         (conj description)

                         (= :function type)
                         (into (let [{:keys [returnvalues]} el]
                                 (cond-> []
                                         (pos? (count parameters))
                                         (conj (str
                                                 "**Parameters:**\n\n"
                                                 (args-doc-html parameters)))
                                         (pos? (count returnvalues))
                                         (conj (str "**Returns:**\n\n"
                                                    (args-doc-html returnvalues))))))

                         (pos? (count examples))
                         (conj (str "**Examples:**\n\n" examples))

                         site-url
                         (conj (format "[Open in Browser](%s)" site-url)))]
    (cond-> {:type :markdown
             :value (string/join "\n\n" sections)}
            base-url
            (assoc :base-url base-url))))

(defn- make-display-string [{:keys [name type parameters]}]
  (str name (when (= :function type)
              (str "(" (string/join ", " (mapv :name parameters)) ")"))))

(defn- make-snippet [{:keys [name type parameters]}]
  (str name (when (= :function type)
              (str "("
                   (->> parameters
                        (keep-indexed (fn [i {:keys [name]}]
                                        (when-not (string/starts-with? name "[")
                                          (format "${%s:%s}" (inc i) name))))
                        (string/join ", "))
                   ")"))))

(defn make
  "Make a lua-specific code completion

  Args:
    doc    documentation map with the following keys:
             :name            string, required, element name
             :type            keyword
             :parameters      for :function type, optional vector of parameter
                              maps with the following keys:
                                :name     string, required
                                :doc      optional html string
                                :types    coll of strings, i.e. union
             :returnvalues    for :function type, same as :parameters
             :description     string, optional, markdown
             :examples        string, optional, markdown

  Optional kv-args:
    :base-url    URI for resolving relative links in the docs and examples
    :url         string, may be relative to :base-url if it was provided"
  [doc & {:keys [base-url url]}]
  (code-completion/make (:name doc)
                        :type (:type doc)
                        :display-string (make-display-string doc)
                        :insert (make-snippet doc)
                        :doc (make-markdown-doc doc base-url url)))