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

(ns hooks.docstrings
  (:require [clj-kondo.hooks-api :as api]
            [clojure.string :as string]))

(defn- check-docstring! [node config]
  (when (api/string-node? node)
    (let [{:keys [row col]} (meta node)
          max-line-length (get-in config [:linters :defold/docstring-line-length :max-line-length])]
      (when (and row col max-line-length)
        ;; Use source text so quotes, indentation, and escapes count as columns.
        (doseq [[index line] (eduction (map-indexed vector) (string/split-lines (str node)))]
          (let [line-length (+ (if (zero? index) (dec col) 0) (count line))]
            (when (> line-length max-line-length)
              (api/reg-finding!
                {:type :defold/docstring-line-length
                 :message (format "Docstring line is longer than %s characters." max-line-length)
                 :row (+ row index)
                 :col (inc max-line-length)
                 :end-row (+ row index)
                 :end-col (inc line-length)}))))))))

(defn- check-attributes! [node config]
  (when (api/map-node? node)
    (doseq [[key-node value-node] (eduction (partition-all 2) (:children node))]
      (when (= :doc (api/sexpr key-node))
        (check-docstring! value-node config)))))

(defn- check-metadata! [node config]
  (doseq [metadata-node (:meta node)]
    (check-attributes! metadata-node config)))

(defn metadata [{:keys [node config]}]
  (check-metadata! (second (:children node)) config)
  nil)

(defn declare-call [{:keys [node config]}]
  (doseq [name-node (next (:children node))]
    (check-metadata! name-node config))
  nil)

(defn def-call [{:keys [node config] :as context}]
  (metadata context)
  ;; A string initializer in (def name "value") is not a docstring.
  (when (= 4 (count (:children node)))
    (check-docstring! (nth (:children node) 2) config))
  nil)

(defn declaration [{:keys [node config] :as context}]
  (metadata context)
  (let [[_ _ doc-or-attributes & body] (:children node)
        [attributes & body] (if (api/string-node? doc-or-attributes)
                              (do
                                (check-docstring! doc-or-attributes config)
                                body)
                              (cons doc-or-attributes body))
        body (if (api/map-node? attributes)
               (do
                 (check-attributes! attributes config)
                 body)
               (cons attributes body))
        first-body-node (first body)]
    ;; Multi-arity defn/defmacro forms may end with an attribute map.
    (when (and (api/list-node? first-body-node)
               (api/vector-node? (first (:children first-body-node))))
      (check-attributes! (last body) config)))
  nil)

(defn protocol [{:keys [node config] :as context}]
  (declaration context)
  (doseq [method-node (:children node)
          :when (api/list-node? method-node)]
    (check-metadata! (first (:children method-node)) config)
    (doseq [child (:children method-node)]
      (check-docstring! child config)))
  nil)
