;; Copyright 2020-2025 The Defold Foundation
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
            [editor.graphics.types :as types]
            [editor.scene-cache :as scene-cache]
            [util.coll :as coll]
            [util.defonce :as defonce]
            [util.eduction :as e])
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

;; VertexBuffer object

(defonce/protocol IVertexBuffer
  (flip! [this] "make this buffer ready for use with OpenGL")
  (flipped? [this])
  (clear! [this])
  (position! [this position])
  (version [this]))

(defonce/type VertexBuffer [vertex-description usage ^Buffer buf ^long buf-items-per-vertex ^:unsynchronized-mutable ^long version]
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
        buffer-item-byte-size (buffers/item-byte-size buffer)
        buffer-items-per-vertex (quot vertex-byte-size buffer-item-byte-size)]
    (assert (zero? (rem vertex-byte-size buffer-item-byte-size)))
    (assert (pos? buffer-items-per-vertex))
    buffer-items-per-vertex))

(defn wrap-vertex-buffer
  [vertex-description usage ^Buffer buffer]
  (let [buffer-items-per-vertex (buffer-items-per-vertex buffer vertex-description)]
    (->VertexBuffer vertex-description usage buffer buffer-items-per-vertex 0)))

(defn wrap-buf
  ^ByteBuffer [^bytes byte-array]
  (buffers/wrap-byte-array byte-array :byte-order/little-endian))

(defn make-buf
  ^ByteBuffer [byte-capacity]
  (buffers/new-byte-buffer byte-capacity :byte-order/little-endian))

(defn make-vertex-buffer
  [vertex-description usage ^long vertex-capacity]
  (let [^long vertex-byte-size (:size vertex-description)
        _ (assert (pos? vertex-byte-size))
        byte-capacity (* vertex-capacity vertex-byte-size)
        byte-buffer (make-buf byte-capacity)]
    (wrap-vertex-buffer vertex-description usage byte-buffer)))

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

(defn- vertex-size
  ^long [attributes]
  (transduce (map attribute-size) + 0 attributes))

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

(defn- parse-attribute-definition
  [form]
  (let [[type nm & [normalize]] form
        [prefix suffix]  (str/split (name type) #"\.")
        prefix           (keyword prefix)
        suffix           (keyword (or suffix "float"))
        num-components   (case prefix
                           :vec1 1
                           :vec2 2
                           :vec3 3
                           :vec4 4)
        attribute-name   (name nm)
        attribute-key    (types/attribute-name-key attribute-name)
        semantic-type    (types/infer-semantic-type attribute-key)
        vector-type      (case num-components
                           1 :vector-type-scalar
                           2 :vector-type-vec2
                           3 :vector-type-vec3
                           4 :vector-type-vec4)]
    (assert num-components (str type " is not a valid type name. It must start with vec1, vec2, vec3, or vec4."))
    (assert (get gl-types suffix) (str type " is not a valid type name. It must end with byte, short, int, float, or double. (Defaults to float if no suffix.)"))
    {:name attribute-name
     :name-key attribute-key
     :type suffix
     :components num-components
     :normalize (true? normalize)
     :coordinate-space :coordinate-space-world
     :vector-type vector-type
     :semantic-type semantic-type}))

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

(defn- assign-attributes! [^GL2 gl attributes attribute-locations]
  {:pre [(vector? attributes)
         (vector? attribute-locations)]}
  (let [attribute-count (count attributes)
        attribute-sizes (into (vector-of :int)
                              (map attribute-size)
                              attributes)
        ^int stride (reduce + attribute-sizes)]
    (assert (= attribute-count (count attribute-locations)))
    (reduce
      (fn [^long buffer-offset ^long attribute-index]
        (let [^int attribute-location (attribute-locations attribute-index)
              ^int attribute-size (attribute-sizes attribute-index)]
          (when-not (neg? attribute-location)
            (let [attribute (attributes attribute-index)
                  ^int component-count (:components attribute)
                  ^int gl-type (gl-types (:type attribute))
                  ^boolean normalize (:normalize attribute)]
              (gl/gl-enable-vertex-attrib-array gl attribute-location)
              (gl/gl-vertex-attrib-pointer gl attribute-location component-count gl-type normalize stride buffer-offset)))
          (+ buffer-offset attribute-size)))
      0
      (range attribute-count))))

(defn- clear-attributes!
  [^GL2 gl attribute-locations]
  (doseq [^int location attribute-locations
          :when (not= location -1)]
    (gl/gl-disable-vertex-attrib-array gl location)))

(defn vertex-attribute->row-column-count [vertex-attribute]
  (let [vector-type-row-column-count (types/vector-type-row-column-count (:vector-type vertex-attribute))]
    (if (pos? vector-type-row-column-count)
      vector-type-row-column-count
      (case (long (:components vertex-attribute -1))
        9 3
        16 4
        nil))))

;; Takes a list of vertex attribute and a matching list of its attribute locations
;; and expands these if the attribute vector type is a matrix.
;; This is needed because to bind a matrix as attribute in OpenGL, we need
;; to bind each column of the vector type individually.
(defn- expand-attributes+locations [attributes attribute-locations]
  {:pre [(vector? attributes)
         (vector? attribute-locations)
         (= (count attributes) (count attribute-locations))]}
  (let [expanded-attributes
        (coll/transfer attributes (empty attributes)
          (mapcat
            (fn [attribute]
              (if-let [row-column-count (vertex-attribute->row-column-count attribute)]
                (repeat row-column-count (assoc attribute :components row-column-count))
                [attribute]))))

        expanded-attribute-locations
        (coll/transfer attribute-locations (empty attribute-locations)
          (coll/mapcat-indexed
            (fn [^long attribute-index ^long base-location]
              (let [attribute (attributes attribute-index)
                    row-column-count (vertex-attribute->row-column-count attribute)]
                (if (nil? row-column-count)
                  [base-location]
                  (range base-location
                         (+ base-location
                            (long row-column-count))))))))]

    [expanded-attributes expanded-attribute-locations]))

(defn- bind-vertex-buffer-with-shader! [^GL2 gl request-id ^VertexBuffer vertex-buffer shader]
  (let [[vbo attribute-locations] (scene-cache/request-object! ::vbo2 request-id gl {:vertex-buffer vertex-buffer :version (version vertex-buffer) :shader shader})
        attributes (:attributes (.vertex-description vertex-buffer))
        [expanded-attributes expanded-attribute-locations] (expand-attributes+locations attributes attribute-locations)]
    (gl/gl-bind-buffer gl GL/GL_ARRAY_BUFFER vbo)
    (assign-attributes! gl expanded-attributes expanded-attribute-locations)
    (gl/gl-bind-buffer gl GL/GL_ARRAY_BUFFER 0)
    expanded-attribute-locations))

(defn- unbind-vertex-buffer-with-shader! [^GL2 gl expanded-attribute-locations]
  (clear-attributes! gl expanded-attribute-locations))

(defn- bind-index-buffer! [^GL2 gl request-id ^IntBuffer index-buffer]
  ;; TODO: Feed in actual version. Perhaps deftype IndexBuffer with IntBuffer + version?
  (let [ibo (scene-cache/request-object! ::ibo2 request-id gl {:index-buffer index-buffer :version 0})]
    (gl/gl-bind-buffer gl GL/GL_ELEMENT_ARRAY_BUFFER ibo)))

(defn- unbind-index-buffer! [^GL2 gl]
  (gl/gl-bind-buffer gl GL/GL_ELEMENT_ARRAY_BUFFER 0))

(defonce/type VertexBufferShaderLink [request-id ^VertexBuffer vertex-buffer shader ^:unsynchronized-mutable expanded-attribute-locations]
  GlBind
  (bind [_this gl _render-args]
    (set! expanded-attribute-locations (bind-vertex-buffer-with-shader! gl request-id vertex-buffer shader)))

  (unbind [_this gl _render-args]
    (unbind-vertex-buffer-with-shader! gl expanded-attribute-locations)
    (set! expanded-attribute-locations nil)))

(defonce/type VertexIndexBufferShaderLink [request-id ^VertexBuffer vertex-buffer ^IntBuffer index-buffer shader ^:unsynchronized-mutable expanded-attribute-locations]
  GlBind
  (bind [_this gl _render-args]
    (set! expanded-attribute-locations (bind-vertex-buffer-with-shader! gl request-id vertex-buffer shader))
    (bind-index-buffer! gl request-id index-buffer))

  (unbind [_this gl _render-args]
    (unbind-vertex-buffer-with-shader! gl expanded-attribute-locations)
    (set! expanded-attribute-locations nil)
    (unbind-index-buffer! gl)))

(defn use-with
  ([request-id ^VertexBuffer vertex-buffer shader]
   (->VertexBufferShaderLink request-id vertex-buffer shader nil))
  ([request-id ^VertexBuffer vertex-buffer ^IntBuffer index-buffer shader]
   (->VertexIndexBufferShaderLink request-id vertex-buffer index-buffer shader nil)))

(defn- update-vbo [^GL2 gl [vbo _] data]
  (gl/gl-bind-buffer gl GL/GL_ARRAY_BUFFER vbo) ; TODO(instancing): Move to right before gl-buffer-data call?
  (let [^VertexBuffer vbuf (:vertex-buffer data)
        ^Buffer buf (.buf vbuf)
        shader (:shader data)
        attribute-names (e/map :name (:attributes (.vertex-description vbuf)))
        attribute-locations (shader/attribute-locations shader gl attribute-names)]
    (assert (flipped? vbuf) "VertexBuffer must be flipped before use.")
    (gl/gl-buffer-data gl GL/GL_ARRAY_BUFFER (buffers/total-byte-size buf) buf (usage-types (.usage vbuf)))
    [vbo attribute-locations]))

(defn- make-vbo [^GL2 gl data]
  (let [vbo (gl/gl-gen-buffer gl)]
    (update-vbo gl [vbo nil] data)))

(defn- destroy-vbos [^GL2 gl objs _]
  (gl/gl-delete-buffers gl (mapv first objs)))

(scene-cache/register-object-cache! ::vbo2 make-vbo update-vbo destroy-vbos)

(defn- update-ibo [^GL2 gl ibo data]
  (gl/gl-bind-buffer gl GL/GL_ELEMENT_ARRAY_BUFFER ibo)
  (let [^IntBuffer int-buffer (:index-buffer data)
        count (.remaining int-buffer)
        size (* count Buffers/SIZEOF_INT)]
    (gl/gl-buffer-data gl GL/GL_ELEMENT_ARRAY_BUFFER size int-buffer GL2/GL_STATIC_DRAW))
  ibo)

(defn- make-ibo [^GL2 gl data]
  (let [ibo (gl/gl-gen-buffer gl)]
    (update-ibo gl ibo data)))

(defn- destroy-ibos [^GL2 gl ibos _]
  (gl/gl-delete-buffers gl ibos))

(scene-cache/register-object-cache! ::ibo2 make-ibo update-ibo destroy-ibos)
