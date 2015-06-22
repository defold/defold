(ns dynamo.gl.vertex
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
the `do-gl` macro from `dynamo.gl`."
  (:require [clojure.string :as str]
            [dynamo.gl :as gl]
            [dynamo.gl.protocols :refer [GlBind GlEnable]]
            [dynamo.gl.shader :as shader]
            [dynamo.types :refer [IDisposable dispose]]
            [editor.buffers :as b])
  (:import [clojure.lang ITransientVector IPersistentVector IEditableCollection]
           [com.google.protobuf ByteString]
           [com.jogamp.common.nio Buffers]
           [java.nio ByteBuffer]
           [java.util.concurrent.atomic AtomicLong AtomicBoolean]
           [javax.media.opengl GL GL2]))

(def type-sizes
  {'byte   Buffers/SIZEOF_BYTE
   'short  Buffers/SIZEOF_SHORT
   'int    Buffers/SIZEOF_INT
   'float  Buffers/SIZEOF_FLOAT
   'double Buffers/SIZEOF_DOUBLE})

(def type-setters
  {'byte   'put
   'short  'putShort
   'int    'putInt
   'float  'putFloat
   'double 'putDouble})

(def type-getters
  {'byte   'get
   'short  'getShort
   'int    'getInt
   'float  'getFloat
   'double 'getDouble})

(def gl-types
  {'byte    GL/GL_BYTE
   'short   GL/GL_SHORT
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
    `(fn [~'slices ~'idx [~@names]]
       (let ~(into [] (apply concat (vals multiplications)))
         ~@(map (fn [i nm setter refer]
                 (list '. (with-meta (list `nth 'slices i) {:tag `ByteBuffer}) setter refer nm))
               (range (count names)) names setters references)))))

(defn- make-vertex-getter
  [vertex-format]
  (let [getters         (attribute-getters (:attributes vertex-format))
        vsteps          (attribute-vsteps  vertex-format)
        multiplications (indexers "idx" vsteps)
        references      (map (comp first multiplications) vsteps)]
    `(fn [~'slices ~'idx]
       (let ~(into [] (apply concat (vals multiplications)))
         [~@(map (fn [i getter refer]
                   (list '. (with-meta (list `nth 'slices i) {:tag `ByteBuffer}) getter (list `int refer)))
                 (range (count getters)) getters references)]))))

(declare new-persistent-vertex-buffer)
(declare new-transient-vertex-buffer)

(deftype PersistentVertexBuffer [layout capacity ^ByteBuffer buffer slices ^AtomicLong count set-fn get-fn]
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
    (and
      (instance? PersistentVertexBuffer val)
      (= (.get count) (.count ^PersistentVertexBuffer val))
      (.equals this val)))

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

(defrecord VertexBufferShaderLink [^PersistentVertexBuffer vertex-buffer shader context-local-data]
  GlBind
  (bind [this gl]
    (when-not (get @context-local-data gl)
      (let [buffer-name (first (gl/gl-gen-buffers gl 1))]
        (gl/gl-bind-buffer ^GL2 gl GL/GL_ARRAY_BUFFER buffer-name)
        (gl/gl-buffer-data ^GL2 gl GL/GL_ARRAY_BUFFER (.limit ^ByteBuffer (.buffer vertex-buffer)) (.buffer vertex-buffer) GL2/GL_STATIC_DRAW)
        (let [attributes  (:attributes (.layout vertex-buffer))
              attrib-locs (vertex-locate-attribs gl shader attributes)]
          (vertex-attrib-pointers gl shader attributes)
        (swap! context-local-data assoc gl {:buffer-name buffer-name
                                            :attrib-locs attrib-locs})))))

  (unbind [this gl]
    (when-let [{:keys [buffer-name attrib-locs]} (get @context-local-data gl)]
      (gl/gl-bind-buffer ^GL2 gl GL/GL_ARRAY_BUFFER 0)
      (when (not= 0 buffer-name)
        (gl/gl-delete-buffers ^GL2 gl buffer-name))
      (swap! context-local-data dissoc gl)))

  GlEnable
  (enable [this gl]
    (when-let [{:keys [buffer-name attrib-locs]} (get @context-local-data gl)]
      (gl/gl-bind-buffer ^GL2 gl GL/GL_ARRAY_BUFFER buffer-name)
      (vertex-attrib-pointers gl shader (:attributes (.layout vertex-buffer)))
      (vertex-enable-attribs gl attrib-locs))
    )

  (disable [this gl]
    (when-let [{:keys [buffer-name attrib-locs]} (get @context-local-data gl)]
      (gl/gl-bind-buffer ^GL2 gl GL/GL_ARRAY_BUFFER 0)))

  IDisposable
  (dispose [this]
    (println :VertexBufferShaderLink.dispose)))

(defn use-with
  "Prepare a vertex buffer to be used in rendering by binding its attributes to
  the given shader's attributes. Matching is done by attribute names. An
  attribute that exists in the vertex buffer but is not used by the shader will
  simply be ignored.

  At the time when `use-with` is called, it binds the buffer to GL as a GL_ARRAY_BUFFER.
  This is also when it binds attribs to the shader.

  This function returns an object that satisfies dynamo.gl.protocols/GlEnable,
  dynamo.gl.protocols/GlDisable."
  [^PersistentVertexBuffer vertex-buffer shader]
  (->VertexBufferShaderLink vertex-buffer shader (atom {})))
