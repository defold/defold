(ns util.digestable
  (:require [clojure.string :as string]
            [dynamo.graph :as g]
            [editor.math :as math]
            [editor.resource :as resource]
            [editor.workspace]
            [util.digest :as digest])
  (:import [clojure.lang MapEntry Named]
           [java.io OutputStreamWriter Writer]))

(set! *warn-on-reflection* true)

(defprotocol Digestable
  (digest! [value writer]))

(defn- pair [a b]
  (MapEntry/create a b))

(defn- named? [value]
  (or (instance? Named value)
      (string? value)))

(defn- node-id-key? [value]
  (and (named? value)
       (string/ends-with? (name value) "node-id")))

(defn- node-id-entry? [key value]
  (and (g/node-id? value)
       (node-id-key? key)))

(defn- node-id-data-representation [node-id]
  (if-some [node-type (g/node-type* node-id)]
    (pair (symbol (:k node-type))
          (g/node-value node-id :sha256))
    (throw (ex-info (str "Unknown node id in digestable: " node-id)
                    {:node-id node-id}))))

(defn- fn->symbol [fn]
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
  (loop [sequence sequence]
    (when-some [entry (first sequence)]
      (digest-entry! entry writer)
      (when-some [remaining (next sequence)]
        (digest-raw! ", " writer)
        (recur remaining))))
  (digest-raw! end writer))

(defn- digest-tagged! [tag-sym value writer]
  (digest-raw! "#dg/" writer)
  (digest-raw! (name tag-sym) writer)
  (digest-raw! " " writer)
  (digest! value writer))

(defn- digest-resource! [resource writer]
  (let [tag-sym (symbol (.getSimpleName (class resource)))]
    (digest-tagged! tag-sym (resource/resource-hash resource) writer)))

(defn- digest-map-entry! [[key value] ^Writer writer]
  (digest! key writer)
  (digest-raw! " " writer)
  (if (node-id-entry? key value)
    (digest-tagged! 'Node (node-id-data-representation value) writer)
    (digest! value writer)))

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
    (digest-tagged! 'Class (symbol (.getName value)) writer)))

(defn sha1-hash
  ^String [object]
  (with-open [digest-output-stream (digest/make-digest-output-stream "SHA-1")
              writer (OutputStreamWriter. digest-output-stream)]
    (digest! object writer)
    (.flush writer)
    (digest/digest-output-stream->hex digest-output-stream)))

(defn sha1-hash? [value]
  (and (string? value)
       (= 40 (count value))))
