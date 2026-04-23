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

(ns support.test-support
  (:require [clojure.java.io :as io]
            [dynamo.graph :as g]
            [editor.fs :as fs]
            [editor.library :as library]
            [internal.system :as is])
  (:import [java.io File]
           [java.net URI]
           [java.util Base64]
           [org.apache.commons.codec.digest DigestUtils]))

(set! *warn-on-reflection* true)

(def enable-performance-tests
  (nil? (get (System/getenv)
             "DEFOLD_EDITOR_DISABLE_PERFORMANCE_TESTS")))

(defmacro with-clean-system
  [& forms]
  (let [configuration (if (map? (first forms)) (first forms) {:cache-size 1000})
        forms (if (map? (first forms)) (next forms)  forms)]
    `(let [system# (is/make-system ~configuration)
           ~'cache (is/system-cache system#)
           ~'world (first (keys (is/graphs system#)))]
       (binding [g/*the-system* (atom system#)]
         ~@forms))))

(defn tx-nodes [& txs]
  (g/tx-nodes-added (g/transact txs)))

(defn array= [a b]
  (and
    (= (class a) (class b))
    (= (count a) (count b))
    (every? true? (map = a b))))

(defn yield
  "Give up the thread just long enough for a context switch"
  []
  (Thread/sleep 1))

(defn undo-stack
  [graph]
  (is/undo-stack (is/graph-history @g/*the-system* graph)))

(defn redo-stack
  [graph]
  (is/redo-stack (is/graph-history @g/*the-system* graph)))

;; These *-until-new-mtime fns are hacks to support the resource-watch sync, which checks mtime

(defn do-until-new-mtime [write-fn f & args]
  (let [f (io/as-file f)
        mtime (.lastModified f)]
    (loop []
      (apply write-fn f args)
      (let [new-mtime (.lastModified f)]
        (when (= mtime new-mtime)
          (Thread/sleep 50)
          (recur))))))

(defn spit-until-new-mtime [f content & args]
  (apply do-until-new-mtime spit f content args))

(defn touch-until-new-mtime [f]
  (do-until-new-mtime (fn [f] (fs/touch-file! f)) f))

(defn write-until-new-mtime [f content]
  (do-until-new-mtime (fn [f] (fs/create-file! f content)) f))

(defn library-directory ^File [project-directory]
  (.toFile (library/directory project-directory)))

(defn library-files [project-directory]
  (seq (.listFiles (library-directory project-directory))))

(defn library-file
  ^File [project-directory ^URI library-uri tag]
  (let [hash (DigestUtils/sha1Hex (str library-uri))
        ^String tag (or tag "")
        encoded-tag (.encodeToString (Base64/getUrlEncoder) (.getBytes tag "UTF-8"))]
    (io/file (library-directory project-directory) (str hash "-" encoded-tag ".zip"))))

(defn graph-dependencies
  ([tgts]
   (graph-dependencies (g/now) tgts))
  ([basis tgts]
   (g/dependencies basis tgts)))

(defmacro with-post-ec
  "Given a symbol that resolves to a function, returns a fn that executes that
  function in an implicit evaluation-context supplied as the final argument to
  the function."
  [fn-sym]
  {:pre [(symbol? fn-sym)]}
  `(fn ~(symbol (name fn-sym)) [& ~'args]
     (g/with-auto-evaluation-context ~'evaluation-context
       (apply ~fn-sym (conj (vec ~'args) ~'evaluation-context)))))

(defn set-system-property!
  ^String [^String property-name ^String value]
  (if (nil? value)
    (System/clearProperty property-name)
    (System/setProperty property-name value)))

(defmacro with-cleared-system-properties! [property-names & body-exprs]
  {:pre [(vector? property-names)
         (every? string? property-names)
         (every? not-empty property-names)]}
  (let [sym+property-name-pairs
        (into []
              (map (fn [^String property-name]
                     [(gensym (.replace property-name "." "-"))
                      property-name]))
              property-names)]

    (list*
      'let
      (into []
            (mapcat (fn [[sym ^String property-name]]
                      [sym `(System/getProperty ~property-name)]))
            sym+property-name-pairs)
      (concat
        (map (fn [^String property-name]
               `(System/clearProperty ~property-name))
             property-names)
        [(list*
           'try
           (concat
             body-exprs
             [(list*
                'finally
                (map (fn [[sym ^String property-name]]
                       `(set-system-property! ~property-name ~sym))
                     (rseq sym+property-name-pairs)))]))]))))
