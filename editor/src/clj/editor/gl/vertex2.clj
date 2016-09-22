(ns editor.gl.vertex2
  (:require
   [clojure.string :as str]
   [editor.gl :as gl]
   [editor.gl.protocols :refer [GlBind]]
   [editor.gl.shader :as shader]
   [editor.scene-cache :as scene-cache]
   [dynamo.graph :as g]
   [editor.buffers :as b])
  (:import
   [clojure.lang Counted]
   [com.google.protobuf ByteString]
   [com.jogamp.common.nio Buffers]
   [java.nio ByteBuffer ByteOrder]
   [javax.media.opengl GL GL2]))

(set! *warn-on-reflection* true)

(def type-sizes
  {:ubyte  Buffers/SIZEOF_BYTE
   :byte   Buffers/SIZEOF_BYTE
   :ushort Buffers/SIZEOF_SHORT
   :short  Buffers/SIZEOF_SHORT
   :uint   Buffers/SIZEOF_INT
   :int    Buffers/SIZEOF_INT
   :float  Buffers/SIZEOF_FLOAT
   :double Buffers/SIZEOF_DOUBLE})

(def gl-types
  {:ubyte   GL/GL_UNSIGNED_BYTE
   :byte    GL/GL_BYTE
   :ushort  GL/GL_UNSIGNED_SHORT
   :short   GL/GL_SHORT
   :uint    GL/GL_UNSIGNED_INT
   :int     GL2/GL_INT
   :float   GL/GL_FLOAT
   :double  GL2/GL_DOUBLE})


;; VertexBuffer object

(defprotocol IVertexBuffer
  (prepare! [this] "make this buffer ready for use with OpenGL")
  (prepared? [this]))

(deftype VertexBuffer [vertex-description ^ByteBuffer buf ^{:unsynchronized-mutable true} prepared]
  IVertexBuffer
  (prepare! [this] (.flip buf) (set! prepared true) this)
  (prepared? [this] prepared)

  clojure.lang.Counted
  (count [this] (let [bytes (if prepared (.limit buf) (.position buf))]
                  (/ bytes (:size vertex-description)))))

(defn make-vertex-buffer
  [vertex-description capacity]
  (let [nbytes (* capacity (:size vertex-description))
        buf (doto (ByteBuffer/allocateDirect nbytes)
              (.order ByteOrder/LITTLE_ENDIAN))]
    (->VertexBuffer vertex-description buf false)))


;; vertex description

(defn- attribute-sizes
  [attributes]
  (map (fn [{:keys [components type]}] (* components (type-sizes type))) attributes))

(defn- vertex-size
  [attributes]
  (reduce + (attribute-sizes attributes)))

(defn- make-vertex-description
  [attributes]
  {:attributes attributes
   :size (vertex-size attributes)})


;; defvertex macro

(defn- attribute-components
  [attributes]
  (for [{:keys [name components] :as attribute} attributes
        n (range components)]
    (-> attribute
        (update :name str (nth ["-x" "-y" "-z" "-w"] n))
        (dissoc :components))))

(defn make-put-fn
  [attributes]
  (let [args (for [{:keys [name type] :as component} (attribute-components attributes)]
               (assoc component :arg (symbol name)))]
    `(fn [~(with-meta 'vbuf {:tag `VertexBuffer}) ~@(map :arg args)]
       (doto ~(with-meta '(.buf vbuf) {:tag `ByteBuffer})
         ~@(for [{:keys [arg type]} args]
             (case type
               :byte   `(.put       ~arg)
               :short  `(.putShort  ~arg)
               :int    `(.putInt    ~arg)
               :float  `(.putFloat  ~arg)
               :double `(.putDouble ~arg)
               :ubyte  `(.put       (.byteValue  (Long. (bit-and ~arg 0xff))))
               :ushort `(.putShort  (.shortValue (Long. (bit-and ~arg 0xffff))))
               :uint   `(.putInt    (.intValue   (Long. (bit-and ~arg 0xffffffff)))))))
       ~'vbuf)))

(def ^:private type-component-counts
  {:vec1 1
   :vec2 2
   :vec3 3
   :vec4 4})

(defn- parse-attribute-definition
  [form]
  (let [[type nm & [normalized?]] form
        [prefix suffix]  (str/split (name type) #"\.")
        prefix           (keyword prefix)
        suffix           (keyword (or suffix "float"))
        num-components   (type-component-counts prefix)]
    (assert num-components (str type " is not a valid type name. It must start with vec1, vec2, vec3, or vec4."))
    (assert (get gl-types suffix) (str type " is not a valid type name. It must end with byte, short, int, float, or double. (Defaults to float if no suffix.)"))
    {:components num-components
     :type suffix
     :name (name nm)
     :normalized? (true? normalized?)}))

(defmacro defvertex
  [name & attribute-definitions]
  (let [attributes         (mapv parse-attribute-definition attribute-definitions)
        vertex-description (make-vertex-description attributes)
        ctor-name          (symbol (str "->" name))
        put-name           (symbol (str name "-put!"))]
    `(do
       (def ~name '~vertex-description)
       (defn ~ctor-name [capacity#]
         (make-vertex-buffer '~vertex-description capacity#))
       (def ~put-name ~(make-put-fn (:attributes vertex-description))))))


;; GL stuff

(defn- vertex-locate-attribs
  [^GL2 gl shader attribs]
  (map #(shader/get-attrib-location shader gl (:name %)) attribs))

(defn- vertex-attrib-pointer
  [^GL2 gl shader attrib stride offset]
  (let [{:keys [name components type normalized?]} attrib
        loc (shader/get-attrib-location shader gl name)]
    (when (not= -1 loc)
      (gl/gl-vertex-attrib-pointer gl ^int loc ^int components ^int (gl-types type) ^boolean normalized? ^int stride ^long offset))))

(defn- vertex-attrib-pointers
  [^GL2 gl shader attribs]
  (let [offsets (reductions + 0 (attribute-sizes attribs))
        stride  (vertex-size attribs)]
    (doall
     (map
      (fn [offset attrib]
        (vertex-attrib-pointer gl shader attrib stride offset))
      offsets attribs))))

(defn- vertex-enable-attribs
  [^GL2 gl locs]
  (doseq [l locs
          :when (not= l -1)]
    (gl/gl-enable-vertex-attrib-array gl l)))

(defn- vertex-disable-attribs
  [^GL2 gl locs]
  (doseq [l locs
          :when (not= l -1)]
    (gl/gl-disable-vertex-attrib-array gl l)))

(defrecord VertexBufferShaderLink [request-id ^VertexBuffer vertex-buffer shader]
  GlBind
  (bind [this gl _]
        (let [vbo (scene-cache/request-object! ::vbo2 request-id gl vertex-buffer)]
          (gl/gl-bind-buffer ^GL2 gl GL/GL_ARRAY_BUFFER vbo)
          (let [attributes  (:attributes (.vertex-description vertex-buffer))
                attrib-locs (vertex-locate-attribs gl shader attributes)]
            (vertex-attrib-pointers gl shader attributes)
            (vertex-enable-attribs gl attrib-locs))))

  (unbind [this gl]
          (gl/gl-bind-buffer ^GL2 gl GL/GL_ARRAY_BUFFER 0)))

(defn use-with
  [request-id vertex-buffer shader]
  (->VertexBufferShaderLink request-id vertex-buffer shader))

(defn- update-vbo [^GL2 gl vbo data]
  (gl/gl-bind-buffer gl GL/GL_ARRAY_BUFFER vbo)
  (let [^VertexBuffer vbuf data
        ^ByteBuffer buf (.buf vbuf)]
    (assert (prepared? vbuf) "VertexBuffer must be prepared before use.")
    (gl/gl-buffer-data ^GL2 gl GL/GL_ARRAY_BUFFER (.limit buf) buf GL2/GL_STATIC_DRAW))
  vbo)

(defn- make-vbo [^GL2 gl data]
  (let [vbo (first (gl/gl-gen-buffers gl 1))]
    (update-vbo gl vbo data)))

(defn- destroy-vbos [^GL2 gl vbos _]
  (apply gl/gl-delete-buffers gl vbos))

(scene-cache/register-object-cache! ::vbo2 make-vbo update-vbo destroy-vbos)
