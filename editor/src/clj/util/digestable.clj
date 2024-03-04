;; Copyright 2020-2024 The Defold Foundation
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

(ns util.digestable
  (:require [clojure.string :as string]
            [dynamo.graph :as g]
            [editor.math :as math]
            [editor.resource :as resource]
            [editor.workspace]
            [util.coll :refer [pair]]
            [util.digest :as digest])
  (:import [clojure.lang Named]
           [com.defold.util IDigestable]
           [java.io OutputStreamWriter Writer]))

(set! *warn-on-reflection* true)

(defprotocol Digestable
  (digest! [value writer]))

(defn- named? [value]
  (or (instance? Named value)
      (string? value)))

(defn- ignored-key? [value]
  (and (instance? Named value)
       (= "digest-ignored" (namespace value))))

(defn- node-id-key? [value]
  (and (named? value)
       (string/ends-with? (name value) "node-id")))

(defn node-id-entry? [key value]
  (and (g/node-id? value)
       (node-id-key? key)))

(defn- node-id-data-representation [node-id]
  (if-some [node-type-kw (g/node-type-kw node-id)]
    (pair (symbol node-type-kw)
          (g/node-value node-id :sha256))
    (throw (ex-info (str "Unknown node id in digestable: " node-id)
                    {:node-id node-id}))))

(defn fn->symbol [fn]
  (let [class-name (.getName (class fn))]
    (if (re-find #"__\d+$" class-name)
      (throw (ex-info (str "Lambda function in digestable: " class-name)
                      {:fn class-name}))
      (let [[namespace name more] (string/split class-name #"\$")]
        (assert (empty? more))
        (symbol (string/replace namespace \_ \-)
                (string/replace name \_ \-))))))

(defn- digest-raw! [^String value ^Writer writer]
  (.write writer value))

(defn- digest-sequence! [^String begin digest-entry! ^String end sequence ^Writer writer]
  (digest-raw! begin writer)
  (when-some [sequence (seq sequence)]
    (loop [sequence sequence]
      (digest-entry! (first sequence) writer)
      (when-some [remaining (next sequence)]
        (digest-raw! ", " writer)
        (recur remaining))))
  (digest-raw! end writer))

(defn- digest-header! [^String type-name writer]
  (digest-raw! "#dg/" writer)
  (digest-raw! type-name writer)
  (digest-raw! " " writer))

(defn- digest-tagged! [tag-sym value writer]
  {:pre [(symbol? tag-sym)]}
  (digest-header! (name tag-sym) writer)
  (digest! value writer))

(defn- digest-resource! [resource writer]
  (let [tag-sym (symbol (.getSimpleName (class resource)))]
    (digest-tagged! tag-sym (resource/resource-hash resource) writer)))

(defn- digest-map-entry! [[key value] ^Writer writer]
  (when-not (ignored-key? key)
    (digest! key writer)
    (digest-raw! " " writer)
    (if (node-id-entry? key value)
      (digest-tagged! 'Node (node-id-data-representation value) writer)
      (digest! value writer))))

(defn- digest-map! [coll writer]
  (let [sorted-sequence (if (sorted? coll) coll (sort-by key coll))]
    (digest-sequence! "{" digest-map-entry! "}" sorted-sequence writer)))

(let [simple-digestable-impl {:digest! print-method}
      simple-digestable-classes [nil
                                 Boolean
                                 CharSequence
                                 Named
                                 Number]]
  (doseq [class simple-digestable-classes]
    (extend class Digestable simple-digestable-impl)))

(extend (Class/forName "[B") Digestable
  {:digest! (fn digest-byte-array! [value writer]
              (digest-sequence! "#dg/Bytes [" digest! "]" value writer))})

(extend-protocol Digestable
  com.google.protobuf.ByteString
  (digest! [value writer]
    (digest-sequence! "#dg/ByteString [" digest! "]" (.toByteArray value) writer))

  java.net.URI
  (digest! [value writer]
    (digest-tagged! 'URI (str value) writer))

  javax.vecmath.Matrix4d
  (digest! [value writer]
    (digest-tagged! 'Matrix4d (math/vecmath->clj value) writer))

  clojure.lang.AFunction
  (digest! [value writer]
    (digest-tagged! 'Function (fn->symbol value) writer))

  clojure.lang.IPersistentSet
  (digest! [value writer]
    (let [sorted-sequence (if (sorted? value) value (sort value))]
      (digest-sequence! "#{" digest! "}" sorted-sequence writer)))

  java.util.Map
  (digest! [value writer]
    (if (satisfies? resource/Resource value)
      (digest-resource! value writer)
      (digest-map! value writer)))

  java.util.List
  (digest! [value writer]
    (digest-sequence! "[" digest! "]" value writer))

  Class
  (digest! [value writer]
    (digest-tagged! 'Class (symbol (.getName value)) writer))

  IDigestable
  (digest! [value writer]
    (digest-header! (.getName (class value)) writer)
    (.digest value writer))

  Object
  (digest! [value _writer]
    (throw (ex-info (str "Encountered undigestable value: " value)
                    {:value value}))))

(defn sha1-hash
  ^String [object]
  (with-open [digest-output-stream (digest/make-digest-output-stream "SHA-1")
              writer (OutputStreamWriter. digest-output-stream)]
    (digest! object writer)
    (.flush writer)
    (digest/completed-stream->hex digest-output-stream)))

(def sha1-hash? digest/sha1-hex?)
