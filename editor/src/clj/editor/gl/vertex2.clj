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

(ns editor.gl.vertex2
  (:require [clojure.string :as str]
            [editor.buffers :as buffers]
            [editor.gl :as gl]
            [editor.gl.protocols :refer [GlBind]]
            [editor.gl.shader :as shader]
            [editor.protobuf :as protobuf]
            [editor.scene-cache :as scene-cache])
  (:import [clojure.lang Counted]
           [com.jogamp.common.nio Buffers]
           [com.jogamp.opengl GL GL2]
           [java.nio Buffer ByteBuffer IntBuffer]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(def gl-types
  {:ubyte   GL/GL_UNSIGNED_BYTE
   :byte    GL/GL_BYTE
   :ushort  GL/GL_UNSIGNED_SHORT
   :short   GL/GL_SHORT
   :uint    GL/GL_UNSIGNED_INT
   :int     GL2/GL_INT
   :float   GL/GL_FLOAT
   :double  GL2/GL_DOUBLE})

(def usage-types
  {:static GL2/GL_STATIC_DRAW
   :dynamic GL2/GL_DYNAMIC_DRAW})

(defn type->stream-type [type]
  (case type
    :float :value-type-float32
    :ubyte :value-type-uint8
    :ushort :value-type-uint16
    :uint :value-type-uint32
    :byte :value-type-int8
    :short :value-type-int16
    :int :value-type-int32))

;; TODO: Might need to add support for uint64/int64 in the future.
;;       (OpenGL ES 2 doesn't support this currently, but Vulkan might.)
(defn stream-type->type [stream-type]
  (case stream-type
    :value-type-float32 :float
    :value-type-uint8 :ubyte
    :value-type-uint16 :ushort
    :value-type-uint32 :uint
    :value-type-int8 :byte
    :value-type-int16 :short
    :value-type-int32 :int))

(defn attribute-name->key [^String name]
  (protobuf/field-name->key name))

(defn attribute-key->semantic-type [attribute-key]
  (case attribute-key
    :position :semantic-type-position
    :normal :semantic-type-normal
    :tangent :semantic-type-tangent
    :binormal :semantic-type-binormal
    :color :semantic-type-color
    (:texcoord :texcoord0) :semantic-type-texcoord
    :page-index :semantic-type-page-index
    :blend-indices :semantic-type-blend-indices
    :blend-weights :semantic-type-blend-weights
    :mtx-world :semantic-type-world-matrix
    :mtx-normal :semantic-type-normal-matrix
    :semantic-type-none))

;; VertexBuffer object

(defprotocol IVertexBuffer
  (flip! [this] "make this buffer ready for use with OpenGL")
  (flipped? [this])
  (clear! [this])
  (position! [this position])
  (version [this]))

(deftype VertexBuffer [vertex-description usage ^Buffer buf ^long buf-items-per-vertex ^:unsynchronized-mutable ^long version]
  IVertexBuffer
  (flip! [this] (.flip buf) (set! version (inc version)) this)
  (flipped? [_this] (= 0 (.position buf)))
  (clear! [this] (.clear buf) this)
  (position! [this position] (.position buf (int (* (int position) buf-items-per-vertex))) this)
  (version [_this] version)

  Counted
  (count [_this]
    (let [item-count (if (pos? (.position buf)) (.position buf) (.limit buf))]
      (/ item-count buf-items-per-vertex))))

(defn- buffer-items-per-vertex
  ^long [^Buffer buffer vertex-description]
  (let [^long vertex-byte-size (:size vertex-description)
        buffer-item-byte-size (buffers/item-byte-size buffer)]
    (/ vertex-byte-size buffer-item-byte-size)))

(defn- buffer-size-in-bytes
  ^long [^Buffer buffer]
  (* (buffers/item-byte-size buffer) (.limit buffer)))

(defn wrap-vertex-buffer
  [vertex-description usage ^Buffer buffer]
  (let [buffer-items-per-vertex (buffer-items-per-vertex buffer vertex-description)]
    (->VertexBuffer vertex-description usage buffer buffer-items-per-vertex 0)))

(defn wrap-buf
  ^ByteBuffer [^bytes byte-array]
  (buffers/little-endian (buffers/wrap-byte-array byte-array)))

(defn make-buf
  ^ByteBuffer [byte-capacity]
  (buffers/little-endian (buffers/new-byte-buffer byte-capacity)))

(defn make-vertex-buffer
  [vertex-description usage ^long capacity]
  (let [nbytes (* capacity ^long (:size vertex-description))
        buf (make-buf nbytes)]
    (wrap-vertex-buffer vertex-description usage buf)))

;; low-level access

(definline buf-blit! [buffer byte-offset bytes]
  `(buffers/blit! ~buffer ~byte-offset ~bytes))

(definline buf-put-floats! [buffer byte-offset numbers]
  `(buffers/put-floats! ~buffer ~byte-offset ~numbers))

(definline buf-put! [buf byte-offset data-type normalize numbers]
  `(buffers/put! ~buf ~byte-offset ~data-type ~normalize ~numbers))

(defn put!
  ^VertexBuffer [^VertexBuffer vbuf byte-offset data-type normalize numbers]
  (buf-put! (.buf vbuf) byte-offset data-type normalize numbers)
  vbuf)

(definline buf-push-floats! [buffer numbers]
  `(buffers/push-floats! ~buffer ~numbers))

(definline buf-push! [buf data-type normalize numbers]
  `(buffers/push! ~buf ~data-type ~normalize ~numbers))

(defn push!
  ^VertexBuffer [^VertexBuffer vbuf data-type normalize numbers]
  (buf-push! (.buf vbuf) data-type normalize numbers)
  vbuf)

;; vertex description

(defn attribute-size
  ^long [{:keys [^long components type]}]
  (* components (buffers/type-size type)))

(defn- attribute-sizes
  [attributes]
  (map attribute-size
       attributes))

(defn- vertex-size
  [attributes]
  (reduce + (attribute-sizes attributes)))

(defn make-vertex-description
  [name attributes]
  {:name name
   :attributes attributes
   :size (vertex-size attributes)})


;; defvertex macro

(defn- attribute-components
  [attributes]
  (for [{:keys [components] :as attribute} attributes
        n (range components)]
    (-> attribute
        (update :name str (nth ["-x" "-y" "-z" "-w"] n))
        (dissoc :components))))

(defn make-put-fn
  [attributes]
  (let [args (for [{:keys [name] :as component} (attribute-components attributes)]
               (assoc component :arg (symbol name)))]
    `(fn [~(with-meta 'vbuf {:tag `VertexBuffer}) ~@(map :arg args)]
       (doto ~(with-meta '(.buf vbuf) {:tag `ByteBuffer})
         ~@(for [{:keys [arg type]} args]
             (let [arg (with-meta arg {:tag (case type
                                              :byte   `Byte
                                              :short  `Short
                                              :int    `Integer
                                              :float  `Float
                                              :double `Double
                                              :ubyte  `Byte
                                              :ushort `Short
                                              :uint   `Integer)})]
               (case type
                 :byte   `(.put       ~arg)
                 :short  `(.putShort  ~arg)
                 :int    `(.putInt    ~arg)
                 :float  `(.putFloat  ~arg)
                 :double `(.putDouble ~arg)
                 :ubyte  `(.put       (.byteValue  (Long/valueOf (bit-and ~arg 0xff))))
                 :ushort `(.putShort  (.shortValue (Long/valueOf (bit-and ~arg 0xffff))))
                 :uint   `(.putInt    (.intValue   (Long/valueOf (bit-and ~arg 0xffffffff))))))))
       ~'vbuf)))

(def ^:private type-component-counts
  {:vec1 1
   :vec2 2
   :vec3 3
   :vec4 4})

(defn- parse-attribute-definition
  [form]
  (let [[type nm & [normalize]] form
        [prefix suffix]  (str/split (name type) #"\.")
        prefix           (keyword prefix)
        suffix           (keyword (or suffix "float"))
        num-components   (type-component-counts prefix)
        attribute-name   (name nm)
        attribute-key    (attribute-name->key attribute-name)]
    (assert num-components (str type " is not a valid type name. It must start with vec1, vec2, vec3, or vec4."))
    (assert (get gl-types suffix) (str type " is not a valid type name. It must end with byte, short, int, float, or double. (Defaults to float if no suffix.)"))
    {:name attribute-name
     :name-key attribute-key
     :type suffix
     :components num-components
     :normalize (true? normalize)
     :coordinate-space :coordinate-space-world
     :semantic-type (attribute-key->semantic-type attribute-key)})) ; TODO: Typically determined by vertex-space setting of material.

(defmacro defvertex
  [name & attribute-definitions]
  (let [attributes         (mapv parse-attribute-definition attribute-definitions)
        vertex-description (make-vertex-description name attributes)
        ctor-name          (symbol (str "->" name))
        put-name           (symbol (str name "-put!"))
        gen-put?           (not (:no-put (meta name)))]
    `(do
       (def ~name '~vertex-description)
       (defn ~ctor-name
         ([capacity#]
          (make-vertex-buffer '~vertex-description :static capacity#))
         ([capacity# usage#]
          (assert (contains? usage-types usage#) (format "usage must be %s" (str/join " or " (map str (keys usage-types)))))
          (make-vertex-buffer '~vertex-description usage# capacity#)))
       ~(when gen-put?
          `(def ~put-name ~(make-put-fn (:attributes vertex-description)))))))


;; GL stuff

(defn- vertex-locate-attribs
  [^GL2 gl shader attribs]
  (let [attribute-infos (shader/attribute-infos shader gl)]
    (mapv (fn [attrib]
            (let [attribute-name (:name attrib)]
              (if-some [attribute-info (get attribute-infos attribute-name)]
                (:index attribute-info)
                -1)))
          attribs)))

(defn- vertex-attrib-pointer
  [^GL2 gl attrib loc stride offset]
  (let [{:keys [components type normalize]} attrib]
    (when (not= -1 loc)
      (gl/gl-vertex-attrib-pointer gl ^int loc ^int components ^int (gl-types type) ^boolean normalize ^int stride ^long offset))))

(defn- vertex-attrib-pointers
  [^GL2 gl attribs attrib-locs]
  (let [offsets (reductions + 0 (attribute-sizes attribs))
        stride  (vertex-size attribs)]
    (doall
      (map
        (fn [offset attrib loc]
          (vertex-attrib-pointer gl attrib loc stride offset))
        offsets attribs attrib-locs))))

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

(defn- request-vbo [^GL2 gl request-id ^VertexBuffer vertex-buffer shader]
  (scene-cache/request-object! ::vbo2 request-id gl {:vertex-buffer vertex-buffer :version (version vertex-buffer) :shader shader}))

(defn- vertex-attribute-binding-info [vertex-attribute]
  (let [shader-type (:shader-type vertex-attribute)
        component-count (:components vertex-attribute)]
    (cond (= shader-type :shader-type-mat2) [2 true]
          (= shader-type :shader-type-mat3) [3 true]
          (= shader-type :shader-type-mat4) [4 true]
          (= component-count 9) [3 true]
          (= component-count 16) [4 true]
          :else [1 false])))

(defn- expand-vertex-attributes+locs [vertex-attributes vertex-attribute-locs]
  (let [vertex-attributes+locs (map vector vertex-attributes vertex-attribute-locs)
        expanded-vertex-attributes (mapcat (fn [[attribute _]]
                                             (let [[row-column-count is-matrix-type] (vertex-attribute-binding-info attribute)]
                                               (if is-matrix-type
                                                 (repeat row-column-count (assoc attribute :components row-column-count))
                                                 [attribute])))
                                           vertex-attributes+locs)
        expandedn-vertex-locs (mapcat (fn [[attribute loc]]
                                        (let [[row-column-count is-matrix-type] (vertex-attribute-binding-info attribute)]
                                          (if is-matrix-type
                                            (map-indexed (fn [^long idx ^long loc-base]
                                                           (+ idx loc-base))
                                                         (repeat row-column-count loc))
                                            [loc])))
                                      vertex-attributes+locs)]
    [expanded-vertex-attributes, expandedn-vertex-locs]))

(defn- bind-vertex-buffer-with-shader! [^GL2 gl request-id ^VertexBuffer vertex-buffer shader]
  (let [[vbo attribute-locations] (request-vbo gl request-id vertex-buffer shader)
        attributes (:attributes (.vertex-description vertex-buffer))
        [expanded-attributes expanded-attribute-locs] (expand-vertex-attributes+locs attributes attribute-locations)]
    (gl/gl-bind-buffer gl GL/GL_ARRAY_BUFFER vbo)
    (vertex-attrib-pointers gl expanded-attributes expanded-attribute-locs)
    (vertex-enable-attribs gl expanded-attribute-locs)))

(defn- unbind-vertex-buffer-with-shader! [^GL2 gl request-id ^VertexBuffer vertex-buffer shader]
  (let [[_ attrib-locs] (request-vbo gl request-id vertex-buffer shader)]
    (vertex-disable-attribs gl attrib-locs))
  (gl/gl-bind-buffer gl GL/GL_ARRAY_BUFFER 0))

(defn- bind-index-buffer! [^GL2 gl request-id ^IntBuffer index-buffer]
  ;; TODO: Feed in actual version. Perhaps deftype IndexBuffer with IntBuffer + version?
  (let [ibo (scene-cache/request-object! ::ibo2 request-id gl {:index-buffer index-buffer :version 0})]
    (gl/gl-bind-buffer gl GL/GL_ELEMENT_ARRAY_BUFFER ibo)))

(defn- unbind-index-buffer! [^GL2 gl]
  (gl/gl-bind-buffer gl GL/GL_ELEMENT_ARRAY_BUFFER 0))

(defrecord VertexBufferShaderLink [request-id ^VertexBuffer vertex-buffer shader]
  GlBind
  (bind [_this gl _render-args]
    (bind-vertex-buffer-with-shader! gl request-id vertex-buffer shader))

  (unbind [_this gl _render-args]
    (unbind-vertex-buffer-with-shader! gl request-id vertex-buffer shader)))

(defrecord VertexIndexBufferShaderLink [request-id ^VertexBuffer vertex-buffer ^IntBuffer index-buffer shader]
  GlBind
  (bind [_this gl _render-args]
    (bind-vertex-buffer-with-shader! gl request-id vertex-buffer shader)
    (bind-index-buffer! gl request-id index-buffer))

  (unbind [_this gl _render-args]
    (unbind-vertex-buffer-with-shader! gl request-id vertex-buffer shader)
    (unbind-index-buffer! gl)))

(defn use-with
  ([request-id ^VertexBuffer vertex-buffer shader]
   (->VertexBufferShaderLink request-id vertex-buffer shader))
  ([request-id ^VertexBuffer vertex-buffer ^IntBuffer index-buffer shader]
   (->VertexIndexBufferShaderLink request-id vertex-buffer index-buffer shader)))

(defn- update-vbo [^GL2 gl [vbo _] data]
  (gl/gl-bind-buffer gl GL/GL_ARRAY_BUFFER vbo)
  (let [^VertexBuffer vbuf (:vertex-buffer data)
        ^Buffer buf (.buf vbuf)
        shader (:shader data)
        attributes (:attributes (.vertex-description vbuf))
        attrib-locs (vertex-locate-attribs gl shader attributes)]
    (assert (flipped? vbuf) "VertexBuffer must be flipped before use.")
    (gl/gl-buffer-data ^GL2 gl GL/GL_ARRAY_BUFFER (buffer-size-in-bytes buf) buf (usage-types (.usage vbuf)))
    [vbo attrib-locs]))

(defn- make-vbo [^GL2 gl data]
  (let [vbo (first (gl/gl-gen-buffers gl 1))]
    (update-vbo gl [vbo nil] data)))

(defn- destroy-vbos [^GL2 gl objs _]
  (apply gl/gl-delete-buffers gl (map first objs)))

(scene-cache/register-object-cache! ::vbo2 make-vbo update-vbo destroy-vbos)

(defn- update-ibo [^GL2 gl ibo data]
  (gl/gl-bind-buffer gl GL/GL_ELEMENT_ARRAY_BUFFER ibo)
  (let [^IntBuffer int-buffer (:index-buffer data)
        count (.remaining int-buffer)
        size (* count Buffers/SIZEOF_INT)]
    (gl/gl-buffer-data ^GL2 gl GL/GL_ELEMENT_ARRAY_BUFFER size int-buffer GL2/GL_STATIC_DRAW))
  ibo)

(defn- make-ibo [^GL2 gl data]
  (let [ibo (first (gl/gl-gen-buffers gl 1))]
    (update-ibo gl ibo data)))

(defn- destroy-ibos [^GL2 gl ibos _]
  (apply gl/gl-delete-buffers gl ibos))

(scene-cache/register-object-cache! ::ibo2 make-ibo update-ibo destroy-ibos)
