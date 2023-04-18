;; Copyright 2020-2023 The Defold Foundation
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
            [util.digest :as digest])
  (:import [clojure.lang Named]
           [java.io OutputStreamWriter Writer]))

(set! *warn-on-reflection* true)

(defprotocol Digestable
  (digest! [value writer opts]))

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

(defn- persistent-node-id-value [node-id opts]
  (if-some [persistent-value (get (:node-id->persistent-value opts) node-id)]
    persistent-value
    (throw (ex-info "Unable to convert node-id to persistent value in digestable. Use :digest-ignored namespace prefix on key or provide :node-id->persistent-value in opts."
                    {:node-id node-id
                     :node-id->persistent-value (:node-id->persistent-value opts)}))))

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

(defn- digest-sequence! [^String begin digest-entry! ^String end sequence ^Writer writer opts]
  (digest-raw! begin writer)
  (when-some [sequence (seq sequence)]
    (loop [sequence sequence]
      (digest-entry! (first sequence) writer opts)
      (when-some [remaining (next sequence)]
        (digest-raw! ", " writer)
        (recur remaining))))
  (digest-raw! end writer))

(defn- digest-tagged! [tag-sym value writer opts]
  (digest-raw! "#dg/" writer)
  (digest-raw! (name tag-sym) writer)
  (digest-raw! " " writer)
  (digest! value writer opts))

(defn- digest-resource! [resource writer opts]
  (let [tag-sym (symbol (.getSimpleName (class resource)))]
    (digest-tagged! tag-sym (resource/resource-hash resource) writer opts)))

(defn- digest-map-entry! [[key value] ^Writer writer opts]
  (when-not (ignored-key? key)
    (digest! key writer opts)
    (digest-raw! " " writer)
    (if (node-id-entry? key value)
      (digest-tagged! 'Node (persistent-node-id-value value opts) writer opts)
      (digest! value writer opts))))

(defn- digest-map! [coll writer opts]
  (let [sorted-sequence (if (sorted? coll) coll (sort-by key coll))]
    (digest-sequence! "{" digest-map-entry! "}" sorted-sequence writer opts)))

(let [simple-digestable-impl {:digest! (fn digest-simple-value! [value writer _opts]
                                         (print-method value writer))}
      simple-digestable-classes [nil
                                 Boolean
                                 CharSequence
                                 Named
                                 Number]]
  (doseq [class simple-digestable-classes]
    (extend class Digestable simple-digestable-impl)))

(extend (Class/forName "[B") Digestable
  {:digest! (fn digest-byte-array! [value writer opts]
              (digest-sequence! "#dg/Bytes [" digest! "]" value writer opts))})

(extend-protocol Digestable
  com.google.protobuf.ByteString
  (digest! [value writer opts]
    (digest-sequence! "#dg/ByteString [" digest! "]" (.toByteArray value) writer opts))

  java.net.URI
  (digest! [value writer opts]
    (digest-tagged! 'URI (str value) writer opts))

  javax.vecmath.Matrix4d
  (digest! [value writer opts]
    (digest-tagged! 'Matrix4d (math/vecmath->clj value) writer opts))

  clojure.lang.AFunction
  (digest! [value writer opts]
    (digest-tagged! 'Function (fn->symbol value) writer opts))

  clojure.lang.IPersistentSet
  (digest! [value writer opts]
    (let [sorted-sequence (if (sorted? value) value (sort value))]
      (digest-sequence! "#{" digest! "}" sorted-sequence writer opts)))

  java.util.Map
  (digest! [value writer opts]
    (if (satisfies? resource/Resource value)
      (digest-resource! value writer opts)
      (digest-map! value writer opts)))

  java.util.List
  (digest! [value writer opts]
    (digest-sequence! "[" digest! "]" value writer opts))

  Class
  (digest! [value writer opts]
    (digest-tagged! 'Class (symbol (.getName value)) writer opts)))

(defn sha1-hash
  (^String [object]
   (sha1-hash object nil))
  (^String [object opts]
   (with-open [digest-output-stream (digest/make-digest-output-stream "SHA-1")
               writer (OutputStreamWriter. digest-output-stream)]
     (digest! object writer opts)
     (.flush writer)
     (digest/digest-output-stream->hex digest-output-stream))))

(defn sha1-hash? [value]
  (and (string? value)
       (= 40 (count value))))
