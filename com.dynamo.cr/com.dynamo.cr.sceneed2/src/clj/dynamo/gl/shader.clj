(ns dynamo.gl.shader
  (:require [clojure.osgi.core :refer [get-bundle]]
            [dynamo.geom :as g]
            [dynamo.file :refer [replace-extension]]
            [dynamo.resource :refer [IDisposable dispose]]
            [dynamo.gl.protocols :refer :all]
            [service.log :as log])
  (:import [java.nio IntBuffer ByteBuffer]
           [javax.media.opengl GL GL2]
           [javax.vecmath Matrix4d Vector4f]
           [dynamo.file PathManipulation]))

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

(defprotocol ShaderProgram
  (shader-program [this]))

(defprotocol ShaderVariables
  (set-uniform [this name val]))

(defmulti set-uniform-at-index (fn [_ _ _ val] (class val)))

(defmethod set-uniform-at-index Matrix4d
  [^GL2 gl progn loc val]
  (.glUniformMatrix4fv gl loc 1 false (g/as-array val) 0))

(defmethod set-uniform-at-index Vector4f
  [^GL2 gl progn loc ^Vector4f val]
  (.glUniform4f gl loc (.x val) (.y val) (.z val) (.w val)))

(defmethod set-uniform-at-index (class (float-array []))
  [^GL2 gl progn loc ^floats val]
  (case (count val)
    3 (.glUniform4f gl loc (aget val 0) (aget val 1) (aget val 2) 1)
    4 (.glUniform4f gl loc (aget val 0) (aget val 1) (aget val 2) (aget val 3))))

(defmethod set-uniform-at-index Integer
  [^GL2 gl progn loc val]
  (.glUniform1i gl loc val))

(defn program-link-errors
  [^GL2 gl progn]
  (let [msg-len (IntBuffer/allocate 1)]
    (.glGetProgramiv gl progn GL2/GL_INFO_LOG_LENGTH msg-len)
    (let [msg (ByteBuffer/allocate (.get msg-len 0))]
      (.glGetProgramInfoLog gl progn (.capacity msg) nil msg)
      (bbuf->string msg))))

(defn make-program
  [^GL2 gl & shaders]
  (let [progn (.glCreateProgram gl)]
    (doseq [s shaders]
      (.glAttachShader gl progn s))
    (.glLinkProgram gl progn)
    (let [status (IntBuffer/allocate 1)]
      (.glGetProgramiv gl progn GL2/GL_LINK_STATUS status)
      (if (= GL/GL_TRUE (.get status 0))
        progn
        (try
          (throw (Exception. (str "Program link failure.\n" (program-link-errors gl progn))))
          (finally
            (.glDeleteProgram gl progn)))))
    progn))

(defn shader-compile-errors
  [^GL2 gl shader-name]
  (let [msg-len (IntBuffer/allocate 1)]
    (.glGetShaderiv gl shader-name GL2/GL_INFO_LOG_LENGTH msg-len)
    (let [msg (ByteBuffer/allocate (.get msg-len 0))]
      (.glGetShaderInfoLog gl shader-name (.capacity msg) nil msg)
      (bbuf->string msg))))

(defn make-shader*
  [type ^GL2 gl source]
  (let [shader-name (.glCreateShader gl type)]
    (.glShaderSource gl shader-name 1 (into-array String [source]) nil)
    (.glCompileShader gl shader-name)
    (let [status (IntBuffer/allocate 1)]
      (.glGetShaderiv gl shader-name GL2/GL_COMPILE_STATUS status)
      (if (= GL/GL_TRUE (.get status 0))
        shader-name
        (try
          (throw (Exception. (str "Shader compilation failure.\n" (shader-compile-errors gl shader-name))))
          (finally
            (.glDeleteShader gl shader-name)))))))

(def make-fragment-shader (partial make-shader* GL2/GL_FRAGMENT_SHADER))
(def make-vertex-shader (partial make-shader* GL2/GL_VERTEX_SHADER))

(defn make-shaders
  "return a shader with the program defined in sdef.{vp,fp} within the given bundle"
  [^GL2 gl ^PathManipulation sdef]

  (let [vs      (make-vertex-shader   gl (slurp (replace-extension sdef "vp")))
        fs      (make-fragment-shader gl (slurp (replace-extension sdef "fp")))
        program (make-program gl vs fs)]
    (reify
      ShaderProgram
      (shader-program [this] program)

      IDisposable
      (dispose [this]
        (when (not= 0 vs)
          (.glDeleteShader gl vs))
        (when (not= 0 fs)
          (.glDeleteShader gl fs))
        (when (not= 0 program)
          (.glDeleteProgram gl program)))

      GlEnable
      (enable [this]
        (.glUseProgram gl program))

      GlDisable
      (disable [this]
        (.glUseProgram gl 0))

      ShaderVariables
      (set-uniform [this name val]
        (let [loc (.glGetUniformLocation ^GL2 gl program name)]
          (set-uniform-at-index gl program loc val))))))
