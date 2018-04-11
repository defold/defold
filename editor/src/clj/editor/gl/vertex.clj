(ns editor.gl.vertex
  "This namespace contains macros and functions to deal with vertex buffers.

The first step is to define a vertex format using the `defvertex` macro.
That macro uses a micro-language similar to GLSL attribute declarations to
describe the vertex attributes and how they will lay out in memory.

The macro also defines a factory function to create vertex buffers with
that format. Given `(defvertex loc+tex ,,,)`, a factory function called
`->loc+tex` will be created. That factory function requires a capacity
argument, which is the number of vertices for which it will allocate memory.

The factory function returns a vertex buffer object that acts exactly like
a Clojure transient vector. You use the core library functions conj!, set!,
assoc!, etc. to place vertices in the buffer. Before using the buffer in
rendering, however, you must make it persistent with the `persistent!`
function. After that, no more modifications are allowed. (Please note that
`persistent!` creates a new object that shares the same underlying memory.
There are no bulk memory copies needed.)

After the vertex is populated, you can bind its data to shader attributes
using the `use-with` function. This returns a binding suitable for use in
the `do-gl` macro from `editor.gl`."
  (:require [clojure.string :as str]
            [editor.buffers :as b]
            [editor.gl :as gl]
            [editor.gl.protocols :refer [GlBind]]
            [editor.gl.shader :as shader]
            [editor.scene-cache :as scene-cache]
            [editor.types :as types]
            [internal.util :as util])
  (:import [clojure.lang ITransientVector IPersistentVector IEditableCollection]
           [com.jogamp.common.nio Buffers]
           [java.nio ByteBuffer]
           [java.util.concurrent.atomic AtomicLong AtomicBoolean]
           [com.jogamp.opengl GL GL2]))

(set! *warn-on-reflection* true)

(defn put-byte   [^ByteBuffer bb position v] (.put       bb position v))
(defn put-short  [^ByteBuffer bb position v] (.putShort  bb position v))
(defn put-int    [^ByteBuffer bb position v] (.putInt    bb position v))
(defn put-float  [^ByteBuffer bb position v] (.putFloat  bb position v))
(defn put-double [^ByteBuffer bb position v] (.putDouble bb position v))
(defn put-ubyte  [^ByteBuffer bb position v] (.put       bb position (.byteValue  (Long. (bit-and v 0xff)))))
(defn put-ushort [^ByteBuffer bb position v] (.putShort  bb position (.shortValue (Long. (bit-and v 0xffff)))))
(defn put-uint   [^ByteBuffer bb position v] (.putInt    bb position (.intValue   (Long. (bit-and v 0xffffffff)))))


(defn get-byte   [^ByteBuffer bb position]   (.get       bb ^int position))
(defn get-short  [^ByteBuffer bb position]   (.getShort  bb position))
(defn get-int    [^ByteBuffer bb position]   (.getInt    bb position))
(defn get-float  [^ByteBuffer bb position]   (.getFloat  bb position))
(defn get-double [^ByteBuffer bb position]   (.getDouble bb position))
(defn get-ubyte  [^ByteBuffer bb position]   (bit-and 0xff       (short (.get bb ^int position))))
(defn get-ushort [^ByteBuffer bb position]   (bit-and 0xffff     (long  (.getShort bb position))))
(defn get-uint   [^ByteBuffer bb position]   (bit-and 0xffffffff (int   (.getInt bb position))))


(def type-sizes
  {'ubyte  Buffers/SIZEOF_BYTE
   'byte   Buffers/SIZEOF_BYTE
   'ushort Buffers/SIZEOF_SHORT
   'short  Buffers/SIZEOF_SHORT
   'uint   Buffers/SIZEOF_INT
   'int    Buffers/SIZEOF_INT
   'float  Buffers/SIZEOF_FLOAT
   'double Buffers/SIZEOF_DOUBLE})

(def type-setters
  {'byte   put-byte
   'ubyte  put-ubyte
   'short  put-short
   'ushort put-ushort
   'int    put-int
   'uint   put-uint
   'float  put-float
   'double put-double})

(def type-getters
  {'byte   get-byte
   'ubyte  get-ubyte
   'short  get-short
   'ushort get-ushort
   'int    get-int
   'uint   get-uint
   'float  get-float
   'double get-double})

(def gl-types
  {'ubyte   GL/GL_UNSIGNED_BYTE
   'byte    GL/GL_BYTE
   'ushort  GL/GL_UNSIGNED_SHORT
   'short   GL/GL_SHORT
   'uint    GL/GL_UNSIGNED_INT
   'int     GL2/GL_INT
   'float   GL/GL_FLOAT
   'double  GL2/GL_DOUBLE})

(defn- sizesof [[name size type & _]]
  (repeat size (type-sizes type)))

(defn- components [[name size & _]]
  size)

(defn- component-size [[name size type & _]]
  (* size (type-sizes type)))

(defn- attribute-name [[name components & _ ]]
  (assert (<= 1 components 4))
  (take components (map (comp symbol str) (repeat name) ["-x" "-y" "-z" "-w"])))

(defn- attribute-names
  [attribs]
  (mapcat attribute-name attribs))

(defn- attribute-positions
  [attribs]
  (reductions + 0 (mapcat sizesof attribs)))

(defn- attribute-offsets
  [attribs]
  (butlast (attribute-positions attribs)))

(defn- attribute-sizes
  [attribs]
  (map (fn [[_ sz tp & _]] (* sz (type-sizes tp))) attribs))

(defn- vertex-size [attribs]
  (reduce + (mapcat sizesof attribs)))

(defn- attribute-setters
  [attribs]
  (mapcat (fn [[_ components type & _]] (repeat components (type-setters type))) attribs))

(defn- attribute-getters
  [attribs]
  (mapcat (fn [[_ components type & _]] (repeat components (type-getters type))) attribs))

(defn- attribute-vsteps
  [{:keys [packing attributes]}]
  (case packing
    :interleaved   (repeat (reduce + (map components attributes)) (vertex-size attributes))
    :chunked       (mapcat (fn [[_ sz tp & _]] (repeat sz (type-sizes tp))) attributes)))

(defn- chunk-component-offsets [attributes]
  (mapcat attribute-offsets (partition 1 attributes)))

(defn- replicate-elements [s1 s2]
  (mapcat #(repeat %2 %1) s1 s2))

(defn- chunk-attribute-starts [capacity attributes]
  (butlast (reductions + 0 (map * (repeat capacity) (attribute-sizes attributes)))))

(defn- chunk-component-starts [capacity attributes]
  (replicate-elements (chunk-attribute-starts capacity attributes) (map second attributes)))

(defn- buffer-starts
  [capacity {:keys [packing attributes]}]
  (case packing
    :interleaved (attribute-offsets attributes)
    :chunked     (map + (chunk-component-starts capacity attributes)
                      (chunk-component-offsets attributes))))

(defn- indexers
  [prefix vsteps]
  (reduce (fn [result multiplicand]
            (assoc result multiplicand [(symbol (str prefix multiplicand))
                                        `(* ~(symbol prefix) ~multiplicand)]))
          {}
          vsteps))

(defn- make-vertex-setter
  [vertex-format]
  (let [names           (attribute-names   (:attributes vertex-format))
        setters         (attribute-setters (:attributes vertex-format))
        vsteps          (attribute-vsteps  vertex-format)
        arglist         (into [] (concat ['slices 'idx] names))
        multiplications (indexers "idx" vsteps)
        references      (map (comp first multiplications) vsteps)]
    `(fn [~'slices ~(with-meta 'idx {:tag 'long}) [~@names]]
       (let ~(into [] (apply concat (vals multiplications)))
         ~@(map (fn [i nm setter refer]
                 (list setter (with-meta (list `nth 'slices i) {:tag `ByteBuffer}) refer nm))
               (range (count names)) names setters references)))))

(defn- make-vertex-getter
  [vertex-format]
  (let [getters         (attribute-getters (:attributes vertex-format))
        vsteps          (attribute-vsteps  vertex-format)
        multiplications (indexers "idx" vsteps)
        references      (map (comp first multiplications) vsteps)]
    `(fn [~'slices ~(with-meta 'idx {:tag 'long})]
       (let ~(into [] (apply concat (vals multiplications)))
         [~@(map (fn [i getter refer]
                   (list getter (with-meta (list `nth 'slices i) {:tag `ByteBuffer}) (list `int refer)))
                 (range (count getters)) getters references)]))))

(declare new-persistent-vertex-buffer)
(declare new-transient-vertex-buffer)

(deftype PersistentVertexBuffer [layout capacity ^ByteBuffer buffer slices ^AtomicLong count set-fn get-fn]
  Object
  (equals [this other]
    (and (instance? PersistentVertexBuffer other)
         (= (.get count) (.count ^PersistentVertexBuffer other))
         (= (seq this) (seq other))))

  b/ByteStringCoding
  (byte-pack [this] (b/byte-pack buffer))

  IEditableCollection
  (asTransient [this])

  IPersistentVector
  ;; information
  (length [this] (.get count))
  (count [this]  (.get count))

  ;; predicates
  (containsKey [this i]
    (assert (integer? i) "Key must be an integer")
    (< i (.get count)))
  (equiv [this val]
    (.equals this val))

  ;; accessors
  (valAt [this i not-found]
    (if (.containsKey this i)
      (get-fn slices i)
      not-found))
  (valAt [this i]         (.valAt this i nil))
  (entryAt [this i]       (.valAt this i nil))
  (nth [this i]           (.valAt this i nil))
  (nth [this i not-found] (.valAt this i not-found))
  (peek [this]            (.valAt this (dec (.get count))))

  ;; sequence fns
  (rseq [this]
    (reverse (.seq this)))
  (seq [this]
    (map #(get-fn slices %1) (range (.get count))))

  (empty [this] nil)

  ;; unsupported
  (cons   [this val]
    (throw (UnsupportedOperationException. "Cons would be slow here. Use 'transient' to create an editable vertex buffer.")))
  (assocN [this key val]
    (throw (UnsupportedOperationException. "Assoc would be slow here. Use 'transient' to create an editable vertex buffer." )))
  (assoc  [this key val]
    (throw (UnsupportedOperationException. "Assoc would be slow here. Use 'transient' to create an editable vertex buffer." )))
  (pop    [this]
    (throw (UnsupportedOperationException. "Pop would be slow here. Use 'transient' to create an editable vertex buffer." ))))

(deftype TransientVertexBuffer [layout capacity ^ByteBuffer buffer slices ^AtomicBoolean editable ^AtomicLong position set-fn get-fn]
  b/ByteStringCoding
  (byte-pack [this] (b/byte-pack buffer))

  ITransientVector
  (assocN [this i val]
    (assert (.get editable) "Transient used after persistent! call")
    (assert (integer? i) "Key must be an integer")
    (set-fn slices i val)
    this)

  (pop [this]
    (throw (UnsupportedOperationException. "Not implemented")))

  ;; from clojure.lang.ITransientAssociative
  (assoc [this key val]
    (.assocN this key val))

  ;; from clojure.lang.ITransientCollection
  (conj [this val]
    (assert (.get editable) "Transient used after persistent! call")
    (assert (< (.get position) capacity) "Buffer is full.")
    (let [idx (.getAndIncrement position)]
      (.assocN this idx val)))

  (persistent [this]
    (assert (.get editable) "Transient used after persistent! call")
    (.set editable false)
    (new-persistent-vertex-buffer this))

  ;; from clojure.lang.ILookup
  (valAt [this key]
    (.valAt this key nil))

  (valAt [this key not-found]
    (assert (.get editable) "Transient used after persistent! call")
    (assert (integer? key)  "Key must be an integer")
    (if (> key (.get position))
      not-found
      (get-fn slices key)))

  ;; from clojure.lang.Indexed
  (nth [this i]           (.valAt this i nil))
  (nth [this i not-found] (.valAt this i not-found))

  ;; from clojure.lang.Counted
  (count [this]
    (assert (.get editable) "Transient used after persistent! call")
    (.get position)))

(defn new-persistent-vertex-buffer
  [^TransientVertexBuffer transient-vertex-buffer]
  (PersistentVertexBuffer. (.layout   transient-vertex-buffer)
                           (.capacity transient-vertex-buffer)
                           (.buffer   transient-vertex-buffer)
                           (.slices   transient-vertex-buffer)
                           (.position transient-vertex-buffer)
                           (.set-fn   transient-vertex-buffer)
                           (.get-fn   transient-vertex-buffer)))

(defn- not-allowed
  [_ _]
  (assert false "Vertex overlay buffers cannot be accessed by index."))

(defn vertex-overlay
  "Use a vertex layout together with an existing ByteBuffer. Returns a vertex buffer suitable
   for the `use-with` function.

   This will assume the ByteBuffer is an integer multiple of the vertex size."
  [layout ^ByteBuffer buffer]
  (assert layout)
  (assert (= 0 (mod (.limit buffer) (:vertex-size layout))))
  (let [^ByteBuffer buffer (.duplicate buffer)
        limit         (.limit buffer)
        count         (mod limit (:vertex-size layout))
        buffer-starts (buffer-starts limit layout)
        slices        (b/slice buffer (map min (repeat limit) buffer-starts))]
    (->PersistentVertexBuffer layout count buffer slices (AtomicLong. count) not-allowed not-allowed)))

(defn new-transient-vertex-buffer
  ([^PersistentVertexBuffer persistent-vertex-buffer]
    (let [buffer-starts (buffer-starts (.capacity persistent-vertex-buffer) (.layout persistent-vertex-buffer))
          storage       (b/copy-buffer (.buffer persistent-vertex-buffer))
          slices        (b/slice storage buffer-starts)]
      (TransientVertexBuffer.    (.layout persistent-vertex-buffer)
                                 (.capacity persistent-vertex-buffer)
                                 storage
                                 slices
                                 (AtomicBoolean. true)
                                 (AtomicLong. (.count persistent-vertex-buffer))
                                 (.set-fn persistent-vertex-buffer)
                                 (.get-fn persistent-vertex-buffer))))
  ([capacity layout vx-size set-fn get-fn]
    (let [buffer-starts (buffer-starts capacity layout)
          storage       (b/little-endian (b/new-byte-buffer capacity vx-size))
          slices        (b/slice storage buffer-starts)]
      (TransientVertexBuffer. layout
                              capacity
                              storage
                              slices
                              (AtomicBoolean. true)
                              (AtomicLong. 0)
                              set-fn
                              get-fn))))

(def ^:private type-component-counts
  {'vec1 1
   'vec2 2
   'vec3 3
   'vec4 4})

(defn- compile-attribute
  [form]
  (let [[type nm & more] form
        [prefix suffix]  (str/split (name type) #"\.")
        prefix           (symbol prefix)
        suffix           (symbol (or suffix "float"))
        num-components   (type-component-counts prefix)]
    (assert num-components (str type " is not a valid type name. It must start with vec1, vec2, vec3, or vec4."))
    (assert (get type-getters suffix) (str type " is not a valid type name. It must end with byte, short, int, float, or double. (Defaults to float if no suffix.)"))
    `['~nm ~num-components '~suffix ~@more]))

(defn- compile-vertex-layout
  [name packing defs]
  `{:name '~name
    :packing ~packing
    :attributes ~(mapv compile-attribute defs)})

(defmacro defvertex
  "This macro creates a new vertex buffer layout definition with the given name.

  The layout is defined by a micro-language that resembles GLSL attribute
  declarations. The layout forms will be used later to locate and bind to
  shader attributes.

  Each layout form has the shape (attribute-type name & [normalized]).

  attribute-type is like X.Y, where X can be 'vec1', 'vec2', 'vec3', or 'vec4'.
  Y is the component type. It can be 'byte', 'short', 'int', 'float', or 'double'.

  Because 'float' is the most common component type, it will be assumed if you
  leave it off.

  Normalized is optional. It defaults to false.

  Example: a vertex has x, y, z location, plus u, v texture coords. All are float.

  (defvertex location+texture
     (vec3 location)
     (vec2 texcoord))

  Example: a vertex has a 4-vector for location, u, v texture coords as normalized
  shorts, plus a normal vector and a specular color.

  (defvertex materialized
     (vec4        location)
     (vec2.short  texture)
     (vec3        normal)
     (vec4.double specularity))

  You can specify a memory layout with the keywords :interleaved or :chunked right
  after the name. The default memory layout for a vertex is interleaved
  (array-of-structs style.) Chunked layout is struct-of-arrays style.

  This macro creates a factory function with `->` prepended to the vertex format name.
  The factory function takes one integer for the number of vertices for which it will
  allocate memory."
  [nm & more]
  (let [packing      (if (keyword? (first more)) (first more) :interleaved)
        more         (if (keyword? (first more)) (rest more) more)
        ctor         (symbol (str "->" nm))
        layout-sexps (compile-vertex-layout nm packing more)
        layout       (eval layout-sexps)
        vx-size      (vertex-size (:attributes layout))
        layout-sexps (conj layout-sexps [:vertex-size vx-size])
        setter-sexps (make-vertex-setter layout)
        getter-sexps (make-vertex-getter layout)]
    `(let [setter-fn# ~setter-sexps
           getter-fn# ~getter-sexps]
       (def ~nm ~layout-sexps)
       (defn ~ctor [capacity#]
         (new-transient-vertex-buffer capacity# ~nm ~vx-size setter-fn# getter-fn#)))))

(defn- vertex-locate-attribs
  [^GL2 gl shader attribs]
  (map
    #(shader/get-attrib-location shader gl (name (first %)))
    attribs))

(defn- vertex-attrib-pointer
  [^GL2 gl shader attrib stride offset]
  (let [[nm sz tp & more] attrib
        loc               (shader/get-attrib-location shader gl (name nm))
        norm              (if (not (nil? (first more))) (first more) false)]
    (when (not= -1 loc)
      (gl/gl-vertex-attrib-pointer gl ^int loc ^int sz ^int (gl-types tp) ^boolean norm ^int stride ^long offset))))

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

(def ^:private access-type-map
  {[:static :draw] GL2/GL_STATIC_DRAW
   [:dynamic :draw] GL2/GL_DYNAMIC_DRAW
   [:stream :draw] GL2/GL_STREAM_DRAW
   [:static :read] GL2/GL_STATIC_READ
   [:dynamic :read] GL2/GL_DYNAMIC_READ
   [:stream :read] GL2/GL_STREAM_READ
   [:static :copy] GL2/GL_STATIC_COPY
   [:dynamic :copy] GL2/GL_DYNAMIC_COPY
   [:stream :copy] GL2/GL_STREAM_COPY})

(defn- find-attribute-index [attribute-name attributes]
  (util/first-index-where (fn [[name-sym]] (= attribute-name (name name-sym)))
                          attributes))

(defn- bind-vertex-buffer! [^GL2 gl request-id ^PersistentVertexBuffer vertex-buffer]
  (let [vbo (scene-cache/request-object! ::vbo request-id gl vertex-buffer)
        attributes (:attributes (.layout vertex-buffer))
        position-index (find-attribute-index "position" attributes)]
    (when (some? position-index)
      (let [offsets (reductions + 0 (attribute-sizes attributes))
            stride (vertex-size attributes)
            position-attribute (nth attributes position-index)
            position-offset (nth offsets position-index)
            [_ sz tp] position-attribute]
        (gl/gl-bind-buffer gl GL/GL_ARRAY_BUFFER vbo)
        (.glVertexPointer gl ^int sz ^int (gl-types tp) ^int stride ^long position-offset)
        (.glEnableClientState gl GL2/GL_VERTEX_ARRAY)))))

(defn- unbind-vertex-buffer! [^GL2 gl]
  (.glDisableClientState gl GL2/GL_VERTEX_ARRAY)
  (gl/gl-bind-buffer gl GL/GL_ARRAY_BUFFER 0))

(defn- bind-vertex-buffer-with-shader! [^GL2 gl request-id ^PersistentVertexBuffer vertex-buffer shader]
  (let [vbo (scene-cache/request-object! ::vbo request-id gl vertex-buffer)]
    (gl/gl-bind-buffer gl GL/GL_ARRAY_BUFFER vbo)
    (let [attributes (:attributes (.layout vertex-buffer))
          attrib-locs (vertex-locate-attribs gl shader attributes)]
      (vertex-attrib-pointers gl shader attributes)
      (vertex-enable-attribs gl attrib-locs))))

(defn- unbind-vertex-buffer-with-shader! [^GL2 gl ^PersistentVertexBuffer vertex-buffer shader]
  (let [attributes (:attributes (.layout vertex-buffer))
        attrib-locs (vertex-locate-attribs gl shader attributes)]
    (vertex-disable-attribs gl attrib-locs)
    (gl/gl-bind-buffer gl GL/GL_ARRAY_BUFFER 0)))

(defrecord VertexBufferShaderLink [request-id ^PersistentVertexBuffer vertex-buffer shader]
  GlBind
  (bind [_this gl render-args]
    (if (types/selection? (:pass render-args))
      (bind-vertex-buffer! gl request-id vertex-buffer)
      (bind-vertex-buffer-with-shader! gl request-id vertex-buffer shader)))

  (unbind [_this gl render-args]
    (if (types/selection? (:pass render-args))
      (unbind-vertex-buffer! gl)
      (unbind-vertex-buffer-with-shader! gl vertex-buffer shader))))

(defn use-with
  "Return a GlBind implementation that can match vertex buffer attributes to the
  given shader's attributes. Matching is done by attribute names. An attribute
  that exists in the vertex buffer but is not used by the shader will simply be
  ignored."
  [request-id ^PersistentVertexBuffer vertex-buffer shader]
  (->VertexBufferShaderLink request-id vertex-buffer shader))

(defn- update-vbo [^GL2 gl vbo data]
  (gl/gl-bind-buffer gl GL/GL_ARRAY_BUFFER vbo)
  (let [^PersistentVertexBuffer vertex-buffer data]
    (gl/gl-buffer-data ^GL2 gl GL/GL_ARRAY_BUFFER (.limit ^ByteBuffer (.buffer vertex-buffer)) (.buffer vertex-buffer) GL2/GL_STATIC_DRAW))
  vbo)

(defn- make-vbo [^GL2 gl data]
  (let [vbo (first (gl/gl-gen-buffers gl 1))]
    (update-vbo gl vbo data)))

(defn- destroy-vbos [^GL2 gl vbos _]
  (apply gl/gl-delete-buffers gl vbos))

(scene-cache/register-object-cache! ::vbo make-vbo update-vbo destroy-vbos)
