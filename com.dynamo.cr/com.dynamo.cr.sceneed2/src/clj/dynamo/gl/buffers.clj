(ns dynamo.gl.buffers
  (:require [dynamo.resource :refer [IDisposable dispose]]
            [dynamo.gl.protocols :refer :all]
            [dynamo.gl :as gl])
  (:import [java.nio IntBuffer Buffer]
           [com.jogamp.common.nio Buffers]
           [javax.media.opengl GL GL2]))

(set! *warn-on-reflection* true)

(defprotocol DirectBuffer
  (direct-buffer [this]))

(def type-sizes
  {GL/GL_BYTE           Buffers/SIZEOF_BYTE
   GL/GL_UNSIGNED_BYTE  Buffers/SIZEOF_BYTE
   GL/GL_SHORT          Buffers/SIZEOF_SHORT
   GL/GL_UNSIGNED_SHORT Buffers/SIZEOF_SHORT
   GL2/GL_INT           Buffers/SIZEOF_INT
   GL/GL_UNSIGNED_INT   Buffers/SIZEOF_INT
   GL/GL_FLOAT          Buffers/SIZEOF_FLOAT
   GL2/GL_DOUBLE        Buffers/SIZEOF_DOUBLE})

(defn size-of
  [[_ cnt tp & _]]
  (let [sz (get type-sizes tp)]
    (assert (not (nil? sz)) (str "Unknown vertex attribute type " tp))
    (* cnt sz)))

(defn vertex-size
  [attribs]
  (reduce + (map size-of attribs)))

(defn vertex-enable-attrib
  [^GL2 gl shader attrib stride offset]
  (let [[nm sz tp norm] attrib
        loc (gl/gl-get-attrib-location gl shader nm)]
    (when (not= -1 loc)
      (gl/gl-vertex-attrib-pointer gl loc ^int sz ^int tp ^boolean norm ^int stride ^long offset)
      (gl/gl-enable-vertex-attrib-array gl loc)
      loc)))

(defn vertex-enable-attribs
  [^GL2 gl shader attribs]
  (let [offsets (reductions + 0 (map size-of attribs))
        stride  (last offsets)]
    (doall
      (map
        (fn [offset attrib]
          (vertex-enable-attrib gl shader attrib stride offset))
        offsets attribs))))

(defn vertex-disable-attrib
  [^GL2 gl loc]
  (when (and loc (not= -1 loc))
    (gl/gl-disable-vertex-attrib-array gl loc)))

(defn vertex-disable-attribs
  [^GL2 gl locs]
  (doseq [l locs]
    (vertex-disable-attrib gl l)))

(defn make-vertex-buffer [^GL2 gl shader attribs byte-count ^Buffer data]
  (let [buffer-name (first (gl/gl-gen-buffers gl 1))
        _           (gl/gl-bind-buffer gl GL/GL_ARRAY_BUFFER buffer-name)
        _           (gl/gl-buffer-data gl GL/GL_ARRAY_BUFFER byte-count data GL2/GL_STATIC_DRAW)
        locs        (vertex-enable-attribs gl shader attribs)]
    (reify
      GlEnable
      (enable [this]
        (gl/gl-bind-buffer gl GL/GL_ARRAY_BUFFER buffer-name))

      GlDisable
      (disable [this]
        (gl/gl-bind-buffer gl GL/GL_ARRAY_BUFFER 0))

      IDisposable
      (dispose [this]
        (gl/gl-bind-buffer gl GL/GL_ARRAY_BUFFER buffer-name)
        (vertex-disable-attribs gl locs)
        (gl/gl-bind-buffer gl GL/GL_ARRAY_BUFFER 0)
        (when (not= 0 buffer-name)
          (gl/gl-delete-buffers gl buffer-name)))

      DirectBuffer
      (direct-buffer [this] data))))
