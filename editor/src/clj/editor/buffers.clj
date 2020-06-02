;; Copyright 2020 The Defold Foundation
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

(ns editor.buffers
  (:import [java.nio Buffer ByteBuffer ByteOrder IntBuffer]
           [com.google.protobuf ByteString]))

(set! *warn-on-reflection* true)

(defn slurp-bytes
  [^ByteBuffer buff]
  (let [buff (.duplicate buff)
        n (.remaining buff)
        bytes (byte-array n)]
    (.get buff bytes)
    bytes))

(defn alias-buf-bytes
  "Avoids copy if possible."
  [^ByteBuffer buff]
  (if (and (.hasArray buff) (= (.remaining buff) (alength (.array buff))))
    (.array buff)
    (slurp-bytes buff)))

(defn bbuf->string [^ByteBuffer bb] (String. ^bytes (alias-buf-bytes bb) "UTF-8"))

(defprotocol ByteStringCoding
  (byte-pack [source] "Return a Protocol Buffer compatible ByteString from the given source."))

(defn- new-buffer [s] (ByteBuffer/allocateDirect s))

(defn new-byte-buffer ^ByteBuffer [& dims] (new-buffer (reduce * 1 dims)))

(defn byte-order ^ByteBuffer [order ^ByteBuffer b] (.order b order))
(def ^ByteBuffer little-endian (partial byte-order ByteOrder/LITTLE_ENDIAN))
(def ^ByteBuffer big-endian    (partial byte-order ByteOrder/BIG_ENDIAN))
(def ^ByteBuffer native-order  (partial byte-order (ByteOrder/nativeOrder)))

(defn as-int-buffer ^IntBuffer [^ByteBuffer b] (.asIntBuffer b))

(defn copy-buffer ^ByteBuffer [^ByteBuffer b]
  (let [sz      (.capacity b)
        clone   (if (.isDirect b) (ByteBuffer/allocateDirect sz) (ByteBuffer/allocate sz))
        ro-copy (.asReadOnlyBuffer b)]
    (.flip ro-copy)
    (.put clone ro-copy)))

(defn slice [^ByteBuffer bb offsets]
  (let [dup (.duplicate bb)]
    (mapv (fn [o] (do
                   (.position dup (int o))
                   (doto (.slice dup)
                     (.order (.order bb))))) offsets)))

(extend-type ByteBuffer
  ByteStringCoding
  (byte-pack [this] (ByteString/copyFrom (.asReadOnlyBuffer this))))

(defn make-put-fn-form [^long component-count source-data-tag-sym put-method-sym]
  (let [offset-sym (gensym "offset")
        source-data-sym (gensym "source-data")
        byte-buffer-sym (gensym "byte-buffer")
        vertex-index-sym (gensym "vertex-index")]
    `(fn [~(with-meta source-data-sym {:tag source-data-tag-sym})
          ~(with-meta byte-buffer-sym {:tag `ByteBuffer})
          ~(with-meta vertex-index-sym {:tag `long})]
       (let [~offset-sym (* ~vertex-index-sym ~component-count)]
         ~@(map (fn [^long component-index]
                  `(. ~byte-buffer-sym ~put-method-sym (aget ~source-data-sym (+ ~offset-sym ~component-index))))
                (range component-count))))))
