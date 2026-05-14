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

(ns editor.gl.vertex2
  (:require [clojure.string :as str]
            [editor.buffers :as buffers]
            [editor.gl :as gl]
            [editor.gl.shader :as shader]
            [editor.gl.types :as gl.types]
            [editor.graphics.types :as graphics.types]
            [editor.scene-cache :as scene-cache]
            [util.coll :as coll]
            [util.defonce :as defonce])
  (:import [clojure.lang Counted]
           [com.jogamp.opengl GL GL2]
           [java.nio Buffer ByteBuffer]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

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
      (quot item-count buf-items-per-vertex))))

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
  {:pre [(graphics.types/vertex-description? vertex-description)
         (graphics.types/usage? usage)
         (instance? Buffer buffer)]}
  (let [buffer-items-per-vertex (buffer-items-per-vertex buffer vertex-description)]
    (->VertexBuffer vertex-description usage buffer buffer-items-per-vertex 0)))

(defn wrap-buf
  ^ByteBuffer [^bytes byte-array]
  (buffers/wrap-byte-array byte-array :byte-order/native))

(defn make-buf
  ^ByteBuffer [byte-capacity]
  (buffers/new-byte-buffer byte-capacity :byte-order/native))

(defn make-vertex-buffer
  [vertex-description usage ^long vertex-capacity]
  (let [^long vertex-byte-size (:size vertex-description)
        _ (assert (pos? vertex-byte-size))
        byte-capacity (* vertex-capacity vertex-byte-size)
        byte-buffer (make-buf byte-capacity)]
    (wrap-vertex-buffer vertex-description usage byte-buffer)))

(definline buf
  ^ByteBuffer [^VertexBuffer vertex-buffer]
  `~(with-meta `(.buf ~(with-meta vertex-buffer {:tag `VertexBuffer})) {:tag `ByteBuffer}))

;; low-level access

(definline buf-blit! [buffer byte-offset bytes]
  `(buffers/blit! ~buffer ~byte-offset ~bytes))

(definline buf-put-floats! [buffer byte-offset numbers]
  `(buffers/put-floats! ~buffer ~byte-offset ~numbers))

(definline buf-put! [buf byte-offset buffer-data-type normalize numbers]
  `(buffers/put! ~buf ~byte-offset ~buffer-data-type ~normalize ~numbers))

(definline buf-push-floats! [buffer numbers]
  `(buffers/push-floats! ~buffer ~numbers))

(definline buf-push! [buf buffer-data-type normalize numbers]
  `(buffers/push! ~buf ~buffer-data-type ~normalize ~numbers))

;; defvertex macro

(defn- attribute-components [attribute-infos]
  (for [{attribute-name :name :keys [name-key vector-type] :as attribute-info} attribute-infos
        n (range (graphics.types/vector-type-component-count vector-type))]
    (let [suffix (nth ["-x" "-y" "-z" "-w"] n)
          attribute-name (str attribute-name suffix)
          attribute-key (keyword (str (name name-key) suffix))]
      (assoc attribute-info
        :name attribute-name
        :name-key attribute-key
        :vector-type :vector-type-scalar))))

(defn make-put-fn [attribute-infos]
  (let [args (for [{:keys [name] :as component} (attribute-components attribute-infos)]
               (assoc component :arg (symbol name)))]
    `(fn [~(with-meta 'vbuf {:tag `VertexBuffer}) ~@(map :arg args)]
       (doto ~(with-meta '(.buf vbuf) {:tag `ByteBuffer})
         ~@(for [{:keys [arg data-type]} args]
             (let [arg (with-meta arg {:tag (case data-type
                                              :type-byte `Byte
                                              :type-short `Short
                                              :type-int `Integer
                                              :type-float `Float
                                              :type-double `Double
                                              :type-unsigned-byte `Byte
                                              :type-unsigned-short `Short
                                              :type-unsigned-int `Integer)})]
               (case data-type
                 :type-byte `(.put ~arg)
                 :type-short `(.putShort ~arg)
                 :type-int `(.putInt ~arg)
                 :type-float `(.putFloat ~arg)
                 :type-double `(.putDouble ~arg)
                 :type-unsigned-byte `(.put (.byteValue (Long/valueOf (bit-and ~arg 0xff))))
                 :type-unsigned-short `(.putShort (.shortValue (Long/valueOf (bit-and ~arg 0xffff))))
                 :type-unsigned-int `(.putInt (.intValue (Long/valueOf (bit-and ~arg 0xffffffff))))))))
       ~'vbuf)))

(defn- parse-attribute-definition [form]
  {:post [(graphics.types/attribute-info? %)]}
  (let [[type nm & [normalize]] form
        [prefix suffix]  (str/split (name type) #"\.")
        prefix           (keyword prefix)
        suffix           (keyword (or suffix "float"))
        vector-type      (case prefix
                           :vec1 :vector-type-scalar
                           :vec2 :vector-type-vec2
                           :vec3 :vector-type-vec3
                           :vec4 :vector-type-vec4)
        data-type        (graphics.types/buffer-data-type-data-type suffix)
        attribute-name   (name nm)
        attribute-key    (graphics.types/attribute-name-key attribute-name)
        semantic-type    (graphics.types/infer-semantic-type attribute-key)

        ;; TODO: You can't define a local-space attribute using defvertex.
        coordinate-space (case semantic-type
                           (:semantic-type-position :semantic-type-normal :semantic-type-tangent) :coordinate-space-world
                           :coordinate-space-local)]
    {:name attribute-name
     :name-key attribute-key
     :normalize (boolean normalize)
     :coordinate-space coordinate-space
     :step-function :vertex-step-function-vertex
     :vector-type vector-type
     :data-type data-type
     :semantic-type semantic-type}))

(defmacro defvertex
  [name & attribute-definitions]
  (let [attributes         (mapv parse-attribute-definition attribute-definitions)
        vertex-description (-> attributes
                               (graphics.types/make-vertex-description)
                               (assoc :defvertex-name-sym name)) ;
        ctor-name          (symbol (str "->" name))
        put-name           (symbol (str name "-put!"))
        gen-put?           (not (:no-put (meta name)))]
    `(do
       (def ~name '~vertex-description)
       (defn ~ctor-name
         ([capacity#]
          (make-vertex-buffer '~vertex-description :static capacity#))
         ([capacity# usage#]
          (make-vertex-buffer '~vertex-description usage# capacity#)))
       ~(when gen-put?
          `(def ~put-name ~(make-put-fn attributes))))))


;; GL stuff

(defn- assign-attributes! [^GL2 gl attributes attribute-locations]
  {:pre [(vector? attributes)
         (vector? attribute-locations)]}
  (let [attribute-count (count attributes)
        attribute-sizes (into (vector-of :int)
                              (map graphics.types/attribute-info-byte-size)
                              attributes)
        ^int stride (reduce + attribute-sizes)]
    (assert (= attribute-count (count attribute-locations)))
    (reduce
      (fn [^long buffer-offset ^long attribute-index]
        (let [^int attribute-location (attribute-locations attribute-index)
              ^int attribute-size (attribute-sizes attribute-index)]
          (when-not (neg? attribute-location)
            (let [attribute (attributes attribute-index)
                  component-count (graphics.types/vector-type-component-count (:vector-type attribute))
                  gl-type (gl.types/data-type-gl-type (:data-type attribute))
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

(defn- attribute-info->row-column-count [{:keys [vector-type] :as _attribute-info}]
  {:pre [(graphics.types/vector-type? vector-type)]}
  (let [vector-type-row-column-count (graphics.types/vector-type-row-column-count vector-type)]
    (when (pos? vector-type-row-column-count)
      vector-type-row-column-count)))

;; Takes a list of vertex attribute and a matching list of its attribute locations
;; and expands these if the attribute vector type is a matrix.
;; This is needed because to bind a matrix as attribute in OpenGL, we need
;; to bind each column of the vector type individually.
(defn- expand-attributes+locations [attributes attribute-locations]
  ;; TODO(instancing): No need to "expand" the attributes - we should just move
  ;; this logic into the assign-attributes! and clear-attributes! functions and
  ;; avoid creating these temporary collections.
  {:pre [(vector? attributes)
         (vector? attribute-locations)
         (= (count attributes) (count attribute-locations))]}
  (let [expanded-attributes
        (coll/into-> attributes (empty attributes)
          (mapcat
            (fn [attribute]
              (if-let [row-column-count (attribute-info->row-column-count attribute)]
                (repeat row-column-count (assoc attribute :vector-type (graphics.types/component-count-vector-type row-column-count false)))
                [attribute]))))

        expanded-attribute-locations
        (coll/into-> attribute-locations (empty attribute-locations)
          (coll/mapcat-indexed
            (fn [^long attribute-index ^long base-location]
              (let [attribute (attributes attribute-index)
                    row-column-count (attribute-info->row-column-count attribute)]
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

(defonce/type VertexBufferShaderLink [request-id ^VertexBuffer vertex-buffer shader ^:unsynchronized-mutable expanded-attribute-locations]
  gl.types/GLBinding
  (bind! [_this gl _render-args]
    (set! expanded-attribute-locations (bind-vertex-buffer-with-shader! gl request-id vertex-buffer shader)))

  (unbind! [_this gl _render-args]
    (unbind-vertex-buffer-with-shader! gl expanded-attribute-locations)
    (set! expanded-attribute-locations nil)))

(defn use-with [request-id ^VertexBuffer vertex-buffer shader]
  {:pre [(graphics.types/request-id? request-id)
         (instance? VertexBuffer vertex-buffer)
         (satisfies? shader/ShaderVariables shader)]}
  (->VertexBufferShaderLink request-id vertex-buffer shader nil))

(defn- update-vbo [^GL2 gl [vbo _] data]
  (let [^VertexBuffer vbuf (:vertex-buffer data)
        ^Buffer buf (.buf vbuf)
        shader (:shader data)
        attribute-infos (:attributes (.vertex-description vbuf))
        attribute-locations (shader/attribute-locations shader gl attribute-infos)
        gl-usage (gl.types/usage-gl-usage (.usage vbuf))]
    (assert (flipped? vbuf) "VertexBuffer must be flipped before use.")
    (gl/gl-bind-buffer gl GL/GL_ARRAY_BUFFER vbo)
    (gl/gl-buffer-data gl GL/GL_ARRAY_BUFFER (buffers/total-byte-size buf) buf gl-usage)
    (gl/gl-bind-buffer gl GL/GL_ARRAY_BUFFER 0)
    [vbo attribute-locations]))

(defn- make-vbo [^GL2 gl data]
  (let [vbo (gl/gl-gen-buffer gl)]
    (update-vbo gl [vbo nil] data)))

(defn- destroy-vbos [^GL2 gl objs _]
  (gl/gl-delete-buffers gl (mapv first objs)))

(scene-cache/register-object-cache! ::vbo2 make-vbo update-vbo destroy-vbos)
