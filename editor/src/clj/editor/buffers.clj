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
  (byte-pack ^ByteString [source] "Return a Protocol Buffer compatible byte string from the given source."))

(defn- new-buffer [s] (ByteBuffer/allocateDirect s))

(defn new-byte-buffer ^ByteBuffer [& dims] (new-buffer (reduce * 1 dims)))

(defn byte-order ^ByteBuffer [order ^ByteBuffer b] (.order b order))
(def ^ByteBuffer little-endian (partial byte-order ByteOrder/LITTLE_ENDIAN))
(def ^ByteBuffer big-endian    (partial byte-order ByteOrder/BIG_ENDIAN))
(def ^ByteBuffer native-order  (partial byte-order (ByteOrder/nativeOrder)))

(defn as-int-buffer ^IntBuffer [^ByteBuffer b] (.asIntBuffer b))

(defn ^ByteBuffer copy-buffer
  [^ByteBuffer b]
  (let [sz      (.capacity b)
        clone   (if (.isDirect b) (ByteBuffer/allocateDirect sz) (ByteBuffer/allocate sz))
        ro-copy (.asReadOnlyBuffer b)]
    (.flip ro-copy)
    (.put clone ro-copy)))

(defn slice [^ByteBuffer bb offsets]
  (let [dup (.duplicate bb)]
    (mapv (fn [o] (do
                   (.position dup o)
                   (doto (.slice dup)
                     (.order (.order bb))))) offsets)))

(extend-type ByteBuffer
  ByteStringCoding
  (byte-pack [buffer] (ByteString/copyFrom (.asReadOnlyBuffer buffer))))
