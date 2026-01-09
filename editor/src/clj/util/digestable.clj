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

(ns util.digestable
  (:require [clojure.string :as string]
            [dynamo.graph :as g]
            [editor.math :as math]
            [editor.resource :as resource]
            [editor.workspace]
            [util.digest :as digest])
  (:import [clojure.lang Named]
           [com.defold.util IDigestable]
           [com.dynamo.bob.textureset TextureSetGenerator$LayoutResult]
           [com.dynamo.graphics.proto Graphics$ShaderDesc]
           [java.io BufferedWriter OutputStreamWriter Writer]
           [java.util Arrays]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

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

(defn- augment-throwable-with-path [throwable digestable path-token]
  (let [ex-data (ex-data throwable)]
    (if (= ::digest-error (:ex-type ex-data))
      (ex-info (ex-message throwable)
               (-> ex-data
                   (update :path conj path-token)
                   (assoc :digestable digestable))
               (ex-cause throwable))
      (ex-info "Digest process ran into an error."
               {:ex-type ::digest-error
                :path (list path-token)
                :digestable digestable}
               throwable))))

(defn- digest-raw! [^String value ^Writer writer]
  (.write writer value))

(defn- digest-sequence! [^String begin sequence ^String end ^Writer writer opts]
  (let [last-index (dec (count sequence))]
    (digest-raw! begin writer)
    (reduce (fn digest-element! [^long index value]
              (try
                (digest! value writer opts)
                (when (< index last-index)
                  (digest-raw! ", " writer))
                (inc index)
                (catch Throwable error
                  (throw (augment-throwable-with-path error sequence index)))))
            0
            sequence)
    (digest-raw! end writer)))

(defn- digest-header! [^String type-name writer]
  (digest-raw! "#dg/" writer)
  (digest-raw! type-name writer)
  (digest-raw! " " writer))

(defn- digest-tagged! [tag-sym value writer opts]
  {:pre [(symbol? tag-sym)]}
  (digest-header! (name tag-sym) writer)
  (digest! value writer opts))

(def ^"[C" hex-chars (.toCharArray "0123456789abcdef"))

(defn- digest-tagged-bytes! [tag-sym ^bytes value ^Writer writer]
  (digest-raw! "#dg/" writer)
  (digest-raw! (name tag-sym) writer)
  (digest-raw! " 0x" writer)
  (let [length (alength value)]
    (if (zero? length)
      (digest-raw! "0" writer) ; Empty = 0x0, Array with single zero element = 0x00 since we write two chars for every byte.
      (loop [index 0]
        (when (< index length)
          (let [signed-byte (aget value index)
                high-char-index (unsigned-bit-shift-right (bit-and signed-byte 0xff) 4)
                low-char-index (bit-and signed-byte 0x0f)]
            (.write writer hex-chars high-char-index 1)
            (.write writer hex-chars low-char-index 1))
          (recur (unchecked-inc-int index)))))))

(defn- digest-resource! [resource writer opts]
  (let [tag-sym (symbol (.getSimpleName (class resource)))]
    (digest-tagged! tag-sym (resource/resource-hash resource) writer opts)))

(defn- to-sorted-map [map]
  {:pre [(map? map)]}
  (if (or (sorted? map)
          (< (count map) 2))
    map
    (into (sorted-map)
          map)))

(defn- digest-map! [coll writer opts]
  (let [sorted-map (to-sorted-map coll)
        last-index (dec (count sorted-map))]
    (digest-raw! "{" writer)
    (reduce-kv (fn digest-map-entry! [^long index key value]
                 (if (ignored-key? key)
                   index
                   (try
                     (digest! key writer opts)
                     (digest-raw! " " writer)
                     (if (node-id-entry? key value)
                       (digest-tagged! 'Node (persistent-node-id-value value opts) writer opts)
                       (digest! value writer opts))
                     (when (< index last-index)
                       (digest-raw! ", " writer))
                     (inc index)
                     (catch Throwable error
                       (throw (augment-throwable-with-path error coll key))))))
               0
               sorted-map)
    (digest-raw! "}" writer)))

(defn- to-sorted-set [set]
  {:pre [(set? set)]}
  (if (or (sorted? set)
          (< (count set) 2))
    set
    (into (sorted-set)
          set)))

(defn- digest-set! [coll writer opts]
  (let [sorted-set (to-sorted-set coll)]
    (digest-sequence! "#{" sorted-set "}" writer opts)))

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
  {:digest! (fn digest-byte-array! [value writer _opts]
              (digest-tagged-bytes! 'Bytes value writer))})

(extend-protocol Digestable
  com.google.protobuf.ByteString
  (digest! [value writer _opts]
    (digest-tagged-bytes! 'ByteString (.toByteArray value) writer))

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
    (digest-set! value writer opts))

  java.util.Map
  (digest! [value writer opts]
    (if (resource/resource? value)
      (digest-resource! value writer opts)
      (digest-map! value writer opts)))

  java.util.List
  (digest! [value writer opts]
    (digest-sequence! "[" value "]" writer opts))

  TextureSetGenerator$LayoutResult
  (digest! [value writer opts]
    (digest-tagged! 'LayoutResult (str value) writer opts))

  Graphics$ShaderDesc
  (digest! [value writer opts]
    (digest-tagged! 'ShaderDesc (str value) writer opts))

  Class
  (digest! [value writer opts]
    (digest-tagged! 'Class (symbol (.getName value)) writer opts))

  IDigestable
  (digest! [value writer _opts]
    (digest-header! (.getName (class value)) writer)
    (.digest value writer))

  Object
  (digest! [value _writer _opts]
    (throw (ex-info (str "Encountered undigestable value: " value)
                    {:value value}))))

(defn sha1-hash
  (^String [object]
   (sha1-hash object nil))
  (^String [object opts]
   (with-open [digest-output-stream (digest/make-digest-output-stream "SHA-1")
               writer (BufferedWriter. (OutputStreamWriter. digest-output-stream))]
     (digest! object writer opts)
     (.flush writer)
     (digest/completed-stream->hex digest-output-stream))))

(def sha1-hash? digest/sha1-hex?)

(defn sha1s->unordered-sha1-hex
  "Takes a collection of SHA-1 byte arrays, and adds them all together
  as 160-bit unsigned integers (mod 2^160) to produce an order-independent hash."
  [byte-arrays]
  (let [modulus (.shiftLeft BigInteger/ONE 160)
        sum (reduce
              (fn [acc ba]
                (-> (BigInteger. 1 ^bytes ba)
                    (.add acc)
                    (.mod modulus)))
              BigInteger/ZERO
              byte-arrays)
        result-bytes ^bytes (.toByteArray ^BigInteger sum)
        ;; NOTE: BigInteger.toByteArray() returns variable length:
        ;; 21 bytes if high bit set (adds sign byte), <20 if leading zeros, else 20
        len (alength result-bytes)
        normalized (case len
                     20 result-bytes
                     21 (Arrays/copyOfRange result-bytes 1 21)
                     (let [padded (byte-array 20)]
                       (System/arraycopy result-bytes 0 padded (- 20 len) len)
                       padded))]
    (util.digest/bytes->hex normalized)))

