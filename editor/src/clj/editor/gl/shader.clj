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

(ns editor.gl.shader
"# Building Shaders

To construct a shader object from .vp and .fp files on disk, use `load-shaders`.

Example:

(load-shaders gl (make-project-path project \"/builtins/tools/atlas/pos_uv\"))

This will look for pos_uv.vp (a vertex shader) and pos_uv.fp (a fragment shader). It will
load both shaders and link them into a program.

To make a shader object from GLSL strings (either just as literal strings, or created
via defshader), use `make-shader`

Example
(defshader frag ,,,)
(defshader vert ,,,)
(make-shader [vert] [frag])

This will use all the strings in the first collection as sources for the vertex shader
and all the strings in the second collection as sources for the fragment shader. If you
only have one string, you can pass that instead of a collection.

# GLSL Translator

The GLSL translator is derived from Roger Allen's Shadertone project (https://github.com/overtone/shadertone).
See licenses/shadertone.txt

This is only a single-pass \"lisp\" to GLSL translator.  Very basic.
If this is useful, then we can work to improve it.

# Basic Forms

Here are the essential forms.
  * define functions
    (defn <return-type> <function-name> <function-args-vector> <body-stmt1> ... )
  * function calls
    (<name> <arg1> <arg2> ... )
  * return value
    (return <statement>)
  * variable creation/assignment
    (uniform <type> <name>)
    (setq <type> <name> <statement>)
    (setq <name> <statement>)
  * looping
    (forloop [ <init-stmt> <test-stmt> <step-stmt> ] <body-stmt1> ... )
    (while <test-stmt> <body-stmt1> ... )
    (break)
    (continue)
  * conditionals
    (if <test> <stmt>)
    (if <test> (do <body-stmt1> ...))
    (if <test> <stmt> <else-stmt>)
    (if <test> (do <body-stmt1> ...) (do <else-stmt1> ...))
  * switch
    (switch <test> <case-int-1> <case-stmt-1> ...)
    cases can only be integers or the keyword :default

# Types

Variable types are exactly the GLSL types.

# Examples

The very simplest case, a constant fragment color.

(defshader test-shader
   (defn void main []
      (setq gl_FragColor (vec4 1.0 0.5 0.5 1.0))))

Note that the \"defn\" inside of defshader resembles clojure.core/defn, but
here it specifically means to create a shader function. Note also the return
type in the declaration.

Here is an example that uses a uniform variable to be set by the application.

(defshader test-shader
  (uniform vec3 iResolution)
  (defn void main []
    (setq vec2 uv (/ gl_FragCoord.xy iResolution.xy))
    (setq gl_FragColor (vec4 uv.x uv.y 0.0 1.0))))

There are some examples in the testcases in dynamo.shader.translate-test."
  (:require [clojure.string :as str]
            [clojure.string :as string]
            [clojure.walk :as walk]
            [editor.buffers :refer [bbuf->string]]
            [editor.geom :as geom]
            [editor.gl :as gl]
            [editor.gl.protocols :refer [GlBind]]
            [editor.scene-cache :as scene-cache]
            [util.coll :refer [pair]])
  (:import [com.jogamp.opengl GL GL2]
           [java.nio ByteBuffer FloatBuffer IntBuffer]
           [java.nio.charset StandardCharsets]
           [javax.vecmath Matrix4d Point3d Vector4d Vector4f]))

(set! *warn-on-reflection* true)

;; ======================================================================
;; shader translation comes from https://github.com/overtone/shadertone.
;; See licenses/shadertone.txt

;; ======================================================================
;; translation functions for a dialect of clojure-like s-expressions
(declare shader-walk)

(defn- shader-typed-assign-str [z]
  (let [[type name value] z
        _ (assert (= 3 (count z)))
        asn-str (format "%s %s = %s;\n"
                          type name
                          (shader-walk (list value)))]
    asn-str))

(defn- shader-assign-str [z]
  (let [[name value] z
        _ (assert (= 2 (count z)))
        asn-str (format "%s = %s;\n"
                        name
                        (shader-walk (list value)))]
    asn-str))

(defn- shader-walk-assign [x]
  (case (count (rest x))
    2 (shader-assign-str (rest x))
    3 (shader-typed-assign-str (rest x))
    :else (assert false "incorrect number of args for setq statement")))

(defn- shader-walk-defn-args [x]
  (assert (vector? x))
  (if (empty? x)
    "void"
    (string/join \, (map #(apply (partial format "%s %s") %) (partition 2 x)))))

(defn- shader-walk-defn [x]
  (let [fn-str (format "%s %s(%s) {\n%s}\n"
                       (nth x 1)
                       (nth x 2)
                       (shader-walk-defn-args (nth x 3))
                       (string/join (shader-walk (drop 4 x))))]  ;; FIXME add indentation level?
   fn-str))

(defn- shader-walk-fn [x]
  (let [pre-fn (if (= (first (str (first x))) \.) "" (str (first x)))
        post-fn (if (= (first (str (first x))) \.) (str (first x)) "")
        fn-str (format "%s(%s)%s"
                       pre-fn
                       (string/join
                        \,
                        (map #(shader-walk (list %)) (rest x)))
                       post-fn)]
    fn-str))

(defn- shader-walk-infix [x]
  (let [fn-str (format "(%s)"
                       (string/join
                        (format " %s " (str (first x)))
                        (map #(shader-walk (list %)) (rest x))))]
    fn-str))

(defn- shader-stmt [x]
  (format "%s;\n" (string/join \space x)))

;; (forloop [ init-stmt test-stmt step-stmt ] body )
(defn- shader-walk-forloop [x]
  (let [[init-stmt test-stmt step-stmt] (nth x 1)
        fl-str (format "for( %s %s; %s ) {\n%s}\n"
                       (shader-walk (list init-stmt))
                       (shader-walk (list test-stmt))
                       (shader-walk (list step-stmt))
                       (string/join (shader-walk (drop 2 x))))]
    fl-str))

;; (whileloop test-stmt body )
(defn- shader-walk-while [x]
  (let [w-str (format "while%s {\n%s}\n"
                      (shader-walk (list (nth x 1)))
                      (string/join (shader-walk (drop 2 x))))]
    w-str))

(defn- shader-walk-do [x]
  (let [w-str (format "{\n%s}\n" (string/join (shader-walk (drop 1 x))))]
    w-str))

(defn- shader-walk-if [x]
  (case (count (rest x))
    2  (let [w-str (format "if%s\n%s" ;; if() {}
                           (shader-walk (list (nth x 1)))
                           (shader-walk (list (nth x 2))))]
         w-str)
    3  (let [w-str (format "if%s\n%selse\n%s" ;; if() {} else {}
                           (shader-walk (list (nth x 1)))
                           (shader-walk (list (nth x 2)))
                           (shader-walk (list (nth x 3))))]
         w-str)
    :else (assert false "incorrect number of args for if statement")))

(defn- shader-walk-case [x]
  (let [[v s] x
        _ (assert (= 2 (count x)))
        c-str (if (number? v)
                (format "case %d:" v)
                (if (= v :default)
                  "default:"
                  (assert false (format "expected integer or default:, got: %s" v))))
        w-str (format "%s\n%s"
                      c-str
                      (shader-walk (list s)))]
    w-str))

(defn- shader-walk-switch [x]
  (let [v     (nth x 1)
        v-str (if (list? v)
                (shader-walk (list v))
                (format "(%s)" (shader-walk (list v))))
        w-str (format "switch%s {\n%s}\n"
                      v-str
                      (string/join (map shader-walk-case (partition 2 (drop 2 x)))))]
    w-str))

(defn- shader-walk-return [x]
  (format "%s;\n" (shader-walk-fn x)))

(defn- shader-walk-index [x]
  (format "%s[%d]" (second x) (nth x 2)))

(defn- inner-walk
  [x]
  (cond
   (list? x)    (let [sfx (str (first x))]
                  (case sfx
                    "defn"        (shader-walk-defn x)
                    "setq"        (shader-walk-assign x)
                    "forloop"     (shader-walk-forloop x)
                    "while"       (shader-walk-while x)
                    "if"          (shader-walk-if x)
                    "do"          (shader-walk-do x)
                    "switch"      (shader-walk-switch x)
                    "break"       (shader-stmt x)
                    "continue"    (shader-stmt x)
                    "discard"     (shader-stmt x)
                    "uniform"     (shader-stmt x)
                    "varying"     (shader-stmt x)
                    "attribute"   (shader-stmt x)
                    "return"      (shader-walk-return x)
                    "nth"         (shader-walk-index x)
                    ("+" "-" "*" "/" "=" "<" ">" "<=" ">=" "==" "!=" ">>" "<<") (shader-walk-infix x)
                    (shader-walk-fn x)))
   (symbol? x)  (identity x)
   (float? x)   (identity x)
   (integer? x) (identity x)
   :else        (shader-walk x)))

(defn- outer-walk [x]
  (cond
   (list? x)     (string/join x)
   :else         (identity x)))

(defn- shader-walk [form]
  (walk/walk inner-walk outer-walk form))

(def ^:private shader-version-str "#version 120")

(defn- create-shader
  "Returns a string in GLSL suitable for compilation. Takes a list of forms.
These forms should be quoted, as if they came from a macro."
  [params] (apply str shader-version-str \newline (shader-walk params)))

;; ======================================================================
;; Public API

(defmacro defshader
  "Macro to define the fragment shader program. Defines a new var whose contents will
be the return value of `create-shader`.

This must be submitted to the driver for compilation before you can use it. See
`make-shader`"
  [name & body]
  `(def ~name ~(create-shader body)))

(defprotocol ShaderVariables
  (attribute-infos [this gl])
  (set-uniform [this gl name val])
  (set-uniform-array [this gl name count val]))

(defprotocol SamplerVariables
  (set-samplers-by-name [this gl sampler-name texture-units])
  (set-samplers-by-index [this gl sampler-index texture-units]))

(defmulti set-uniform-at-index (fn [_ _ _ val] (class val)))
(defmulti set-uniforms-at-index (fn [_ _ _ _ val] (class val)))

(defmethod set-uniform-at-index Matrix4d
  [^GL2 gl progn loc val]
  (.glUniformMatrix4fv gl loc 1 false (float-array (geom/as-array val)) 0))

(defmethod set-uniform-at-index Vector4f
  [^GL2 gl progn loc ^Vector4f val]
  (.glUniform4f gl loc (.x val) (.y val) (.z val) (.w val)))

(defmethod set-uniform-at-index Vector4d
  [^GL2 gl progn loc ^Vector4d val]
  (.glUniform4f gl loc (.x val) (.y val) (.z val) (.w val)))

(defmethod set-uniform-at-index Point3d
  [^GL2 gl progn loc ^Point3d val]
  (.glUniform3f gl loc (float (.x val)) (float (.y val)) (float (.z val))))

(defmethod set-uniform-at-index (class (float-array []))
  [^GL2 gl progn loc ^floats val]
  (case (count val)
    3 (.glUniform4f gl loc (aget val 0) (aget val 1) (aget val 2) 1)
    4 (.glUniform4f gl loc (aget val 0) (aget val 1) (aget val 2) (aget val 3))))

(defmethod set-uniform-at-index Integer
  [^GL2 gl progn loc val]
  (.glUniform1i gl loc val))

(defmethod set-uniform-at-index Long
  [^GL2 gl progn loc val]
  (.glUniform1i gl loc (int val)))

(defmethod set-uniform-at-index nil
  [^GL2 gl progn loc val]
  ;; No-Op. This is for sampler uniforms. They are just a name. Contains no
  ;; value to set.
  )

(defmethod set-uniforms-at-index (class (float-array []))
  [^GL2 gl progn loc count vals]
  (let [fb (FloatBuffer/wrap vals)]
    (.glUniform4fv gl loc count fb)))

(defn program-link-errors
  [^GL2 gl progn]
  (let [msg-len (IntBuffer/allocate 1)]
    (.glGetProgramiv gl progn GL2/GL_INFO_LOG_LENGTH msg-len)
    (let [msg (ByteBuffer/allocate (.get msg-len 0))]
      (.glGetProgramInfoLog gl progn (.capacity msg) nil msg)
      (bbuf->string msg))))

(defn- delete-program [^GL2 gl program]
  (when-not (zero? program)
    (.glDeleteProgram gl program)))

(defn- attach-shaders [^GL2 gl program shaders]
  (doseq [shader shaders]
    (.glAttachShader gl program shader)))

(defn make-program
  [^GL2 gl & shaders]
  (let [program (.glCreateProgram gl)]
    (attach-shaders gl program shaders)
    (.glLinkProgram gl program)
    (let [status (IntBuffer/allocate 1)]
      (.glGetProgramiv gl program GL2/GL_LINK_STATUS status)
      (if (= GL/GL_TRUE (.get status 0))
        program
        (try
          (throw (Exception. (str "Program link failure.\n" (program-link-errors gl program))))
          (finally
            (delete-program gl program)))))))

(defn shader-compile-errors
  ^String [^GL2 gl shader-name]
  (let [log-length-storage (IntBuffer/allocate 1)]
    (.glGetShaderiv gl shader-name GL2/GL_INFO_LOG_LENGTH log-length-storage)
    (let [null-terminated-string-length (.get log-length-storage 0) ; Note: Some implementations return zero when the log is empty, some return one.
          string-length (dec null-terminated-string-length)]
      (if (pos? string-length)
        (let [info-log-buffer (ByteBuffer/allocate null-terminated-string-length)]
          (.glGetShaderInfoLog gl shader-name null-terminated-string-length nil info-log-buffer)
          (bbuf->string info-log-buffer 0 string-length))
        ""))))

(defn delete-shader
  [^GL2 gl shader]
  (when-not (zero? shader)
    (.glDeleteShader gl shader)))

(defn make-shader*
  [type ^GL2 gl source]
  ;; Shader source can be either a string or a collection of strings.
  ;; However, it is not intended to be a collection of lines. The
  ;; shader compiler will simply read from each string in turn as if
  ;; they were concatenated. Thus, you need to have newline characters
  ;; at the end of each line.
  (assert (or (string? source) (coll? source)))
  (let [shader-name (.glCreateShader gl type)
        source-strings (into-array String
                                   (if (coll? source)
                                     source
                                     [source]))]
    (.glShaderSource gl shader-name (count source-strings) source-strings nil)
    (.glCompileShader gl shader-name)
    (let [status (IntBuffer/allocate 1)]
      (.glGetShaderiv gl shader-name GL2/GL_COMPILE_STATUS status)
      (if (= GL/GL_TRUE (.get status 0))
        shader-name
        (try
          (throw (Exception. (str "Shader compilation failure.\n" (shader-compile-errors gl shader-name))))
          (finally
            (delete-shader gl shader-name)))))))

(def make-fragment-shader (partial make-shader* GL2/GL_FRAGMENT_SHADER))
(def make-vertex-shader (partial make-shader* GL2/GL_VERTEX_SHADER))

(defn- set-uniform-impl! [gl program uniform-infos uniform-name uniform-value]
  (when-some [uniform-info (uniform-infos uniform-name)]
    (try
      (set-uniform-at-index gl program (:index uniform-info) uniform-value)
      (catch IllegalArgumentException e
        (throw (IllegalArgumentException. (format "Failed setting uniform '%s'." uniform-name) e))))))

(defn- set-sampler-uniform-impl! [gl program uniform-infos slice-sampler-uniform-names texture-units]
  (doall
    (map (fn [slice-sampler-uniform-name texture-unit]
           (if (and (int? texture-unit)
                    (not (neg? texture-unit)))
             (when-some [slice-sampler-uniform-info (uniform-infos slice-sampler-uniform-name)]
               (set-uniform-at-index gl program (:index slice-sampler-uniform-info) texture-unit))
             (throw (IllegalArgumentException. (format "Invalid texture unit '%s' for uniform '%s'." texture-unit slice-sampler-uniform-name)))))
         slice-sampler-uniform-names
         texture-units)))

(defrecord ShaderLifecycle [request-id verts frags uniforms array-sampler-name->uniform-names strip-resource-binding-namespace-regex-str]
  GlBind
  (bind [_this gl render-args]
    (let [{:keys [program uniform-infos]} (scene-cache/request-object! ::shader request-id gl [verts frags array-sampler-name->uniform-names strip-resource-binding-namespace-regex-str])]
      (.glUseProgram ^GL2 gl program)
      (when-not (zero? program)
        (doseq [[name val] uniforms
                :when (some? val)
                :let [val (if (keyword? val)
                            (get render-args val)
                            val)]]
          (set-uniform-impl! gl program uniform-infos name val)))))

  (unbind [_this gl _render-args]
    (.glUseProgram ^GL2 gl 0))

  ShaderVariables
  (attribute-infos [_this gl]
    (when-let [{:keys [attribute-infos]} (scene-cache/request-object! ::shader request-id gl [verts frags array-sampler-name->uniform-names strip-resource-binding-namespace-regex-str])]
      attribute-infos))

  (set-uniform [_this gl name val]
    (assert (string? (not-empty name)))
    (when-let [{:keys [program uniform-infos]} (scene-cache/request-object! ::shader request-id gl [verts frags array-sampler-name->uniform-names strip-resource-binding-namespace-regex-str])]
      (when (and (not (zero? program)) (= program (gl/gl-current-program gl)))
        (set-uniform-impl! gl program uniform-infos name val))))

  (set-uniform-array [_this gl name count vals]
    (assert (string? (not-empty name)))
    (when-let [{:keys [program uniform-infos]} (scene-cache/request-object! ::shader request-id gl [verts frags array-sampler-name->uniform-names strip-resource-binding-namespace-regex-str])]
      (when (and (not (zero? program)) (= program (gl/gl-current-program gl)))
        (when-some [uniform-info (uniform-infos name)]
          (try
            (set-uniforms-at-index gl program (:index uniform-info) count vals)
            (catch IllegalArgumentException e
              (throw (IllegalArgumentException. (format "Failed setting array uniform '%s'." name) e))))))))

  SamplerVariables
  (set-samplers-by-name [_this gl sampler-name texture-units]
    (assert (string? (not-empty sampler-name)))
    (when-let [{:keys [program uniform-infos sampler-name->uniform-names]}
               (scene-cache/request-object! ::shader request-id gl [verts frags array-sampler-name->uniform-names strip-resource-binding-namespace-regex-str])]
      (when (and (not (zero? program)) (= program (gl/gl-current-program gl)))
        (when-some [uniform-names (sampler-name->uniform-names sampler-name)]
          (try
            (set-sampler-uniform-impl! gl program uniform-infos uniform-names texture-units)
            (catch IllegalArgumentException e
              (throw (IllegalArgumentException. (format "Failed setting sampler uniform '%s'." sampler-name) e))))))))

  (set-samplers-by-index [_this gl sampler-index texture-units]
    (when-let [{:keys [program uniform-infos sampler-index->sampler-name sampler-name->uniform-names]}
               (scene-cache/request-object! ::shader request-id gl [verts frags array-sampler-name->uniform-names strip-resource-binding-namespace-regex-str])]
      (when (and (not (zero? program)) (= program (gl/gl-current-program gl)))
        (when-some [sampler-name (sampler-index->sampler-name sampler-index)]
          (let [uniform-names (sampler-name->uniform-names sampler-name)]
            (try
              (set-sampler-uniform-impl! gl program uniform-infos uniform-names texture-units)
              (catch IllegalArgumentException e
                (throw (IllegalArgumentException. (format "Failed setting sampler uniform '%s' at index %d." sampler-name sampler-index) e))))))))))

(defn make-shader
  "Ready a shader program for use by compiling and linking it. Takes a collection
of GLSL strings and returns an object that satisfies GlBind and GlEnable."
  ([request-id verts frags]
   (make-shader request-id verts frags {}))
  ([request-id verts frags uniforms]
   (->ShaderLifecycle request-id verts frags uniforms {} nil))
  ([request-id verts frags uniforms array-sampler-name->uniform-names]
   (->ShaderLifecycle request-id verts frags uniforms array-sampler-name->uniform-names nil))
  ([request-id verts frags uniforms array-sampler-name->uniform-names strip-resource-binding-namespace-regex-str]
   (->ShaderLifecycle request-id verts frags uniforms array-sampler-name->uniform-names strip-resource-binding-namespace-regex-str)))

(defn shader-lifecycle? [value]
  (instance? ShaderLifecycle value))

(defn is-using-array-samplers? [shader-lifecycle]
  (pos? (count (:array-sampler-name->uniform-names shader-lifecycle))))

(defn page-count-mismatch-error-message-raw [is-paged-material texture-page-count material-max-page-count image-property-name]
  (when (and (some? texture-page-count)
             (some? material-max-page-count))
    (cond
      (and is-paged-material
           (zero? texture-page-count))
      (str "The Material expects a paged Atlas, but the selected " image-property-name " is not paged")

      (and (not is-paged-material)
           (pos? texture-page-count))
      (str "The Material does not support paged Atlases, but the selected " image-property-name " is paged")

      (< material-max-page-count texture-page-count)
      (str "The Material's 'Max Page Count' is not sufficient for the number of pages in the selected " image-property-name))))

(def page-count-mismatch-error-message (memoize page-count-mismatch-error-message-raw))

(defn- gl-uniform-type->uniform-type [^long gl-uniform-type]
  (condp = gl-uniform-type
    GL2/GL_FLOAT :float
    GL2/GL_FLOAT_VEC2 :float-vec2
    GL2/GL_FLOAT_VEC3 :float-vec3
    GL2/GL_FLOAT_VEC4 :float-vec4
    GL2/GL_INT :int
    GL2/GL_INT_VEC2 :int-vec2
    GL2/GL_INT_VEC3 :int-vec3
    GL2/GL_INT_VEC4 :int-vec4
    GL2/GL_BOOL :bool
    GL2/GL_BOOL_VEC2 :bool-vec2
    GL2/GL_BOOL_VEC3 :bool-vec3
    GL2/GL_BOOL_VEC4 :bool-vec4
    GL2/GL_FLOAT_MAT2 :float-mat2
    GL2/GL_FLOAT_MAT3 :float-mat3
    GL2/GL_FLOAT_MAT4 :float-mat4
    GL2/GL_SAMPLER_2D :sampler-2d
    GL2/GL_SAMPLER_CUBE :sampler-cube))

(defn- sampler-uniform-type? [uniform-type]
  (case uniform-type
    (:sampler-2d :sampler-cube) true
    false))

(def ^:private gl-shader-parameter
  (let [out-param-value (int-array 1)]
    (fn gl-shader-parameter
      ^long [^GL2 gl ^long program param]
      (.glGetProgramiv gl program param out-param-value 0)
      (aget out-param-value 0))))

(def ^:private uniform-info
  (let [name-array-suffix-pattern #"\[\d+\]$"
        name-buffer-size 128
        out-name-length (int-array 1)
        out-count (int-array 1) ; Number of array elements.
        out-type (int-array 1)
        out-name (byte-array name-buffer-size)]
    (fn uniform-info [^GL2 gl program uniform-index]
      (.glGetActiveUniform gl program uniform-index name-buffer-size out-name-length 0 out-count 0 out-type 0 out-name 0)
      (let [name-length (aget out-name-length 0)
            name (String. out-name 0 name-length StandardCharsets/UTF_8)
            location (.glGetUniformLocation gl program name)
            type (gl-uniform-type->uniform-type (aget out-type 0))
            count (aget out-count 0)
            ;; 1. strip brackets from uniform name, i.e "uniform_name[123]"
            sanitized-name (string/replace name name-array-suffix-pattern "")]
        {:name sanitized-name
         :index location
         :type type
         :count count}))))

(def ^:private attribute-info
  (let [name-buffer-size 128
        out-name-length (int-array 1)
        out-count (int-array 1) ; Number of array elements.
        out-type (int-array 1)
        out-name (byte-array name-buffer-size)]
    (fn attribute-info [^GL2 gl program attribute-index]
      (.glGetActiveAttrib gl program attribute-index name-buffer-size out-name-length 0 out-count 0 out-type 0 out-name 0)
      (let [name-length (aget out-name-length 0)
            name (String. out-name 0 name-length StandardCharsets/UTF_8)
            location (.glGetAttribLocation gl program name)
            type (gl-uniform-type->uniform-type (aget out-type 0))
            count (aget out-count 0)]
        {:name name
         :index location
         :type type
         :count count}))))

(defn- strip-resource-namespace [uniform-info strip-resource-binding-namespace-regex]
  (if strip-resource-binding-namespace-regex
    (assoc uniform-info :name (string/replace (:name uniform-info) strip-resource-binding-namespace-regex ""))
    uniform-info))

(defn- make-shader-program [^GL2 gl [vertex-shader-source fragment-shader-source array-sampler-name->uniform-names strip-resource-binding-namespace-regex-str]]
  (let [vertex-shader (make-vertex-shader gl vertex-shader-source)
        strip-resource-binding-namespace-regex (when strip-resource-binding-namespace-regex-str (re-pattern strip-resource-binding-namespace-regex-str))]
    (try
      (let [fragment-shader (make-fragment-shader gl fragment-shader-source)]
        (try
          (let [program (make-program gl vertex-shader fragment-shader)

                attribute-infos
                (into {}
                      (map (fn [^long attribute-index]
                             (let [attribute-info (attribute-info gl program attribute-index)]
                               [(:name attribute-info) attribute-info])))
                      (range (gl-shader-parameter gl program GL2/GL_ACTIVE_ATTRIBUTES)))

                uniform-infos
                (into {}
                      (map (fn [^long uniform-index]
                             (let [uniform-info (strip-resource-namespace (uniform-info gl program uniform-index) strip-resource-binding-namespace-regex)]
                               [(:name uniform-info) uniform-info])))
                      (range (gl-shader-parameter gl program GL2/GL_ACTIVE_UNIFORMS)))

                array-sampler-uniform-name?
                (into #{}
                      (mapcat val)
                      array-sampler-name->uniform-names)

                sampler-name->uniform-names
                (into array-sampler-name->uniform-names
                      (keep (fn [[uniform-name uniform-info]]
                              (when (sampler-uniform-type? (:type uniform-info))
                                (when-not (array-sampler-uniform-name? uniform-name)
                                  (pair uniform-name [uniform-name])))))
                      uniform-infos)

                sampler-index->sampler-name
                (->> sampler-name->uniform-names
                     (mapv (fn [[sampler-name uniform-names]]
                             (pair sampler-name
                                   (uniform-infos (first uniform-names)))))
                     (sort-by (comp :index second))
                     (mapv first))]
            {:program program
             :uniform-infos uniform-infos
             :attribute-infos attribute-infos
             :sampler-name->uniform-names sampler-name->uniform-names
             :sampler-index->sampler-name #(get sampler-index->sampler-name %)})
          (finally
            (delete-shader gl fragment-shader)))) ; flag shaders for deletion: they will be deleted immediately, or when we delete the program to which they are attached
      (finally
        (delete-shader gl vertex-shader)))))

(defn- update-shader-program [^GL2 gl {:keys [program]} data]
  (delete-program gl program)
  (try
    (make-shader-program gl data)
    (catch Exception _
      {:program 0
       :uniform-infos {}
       :attribute-infos {}
       :sampler-name->uniform-names {}
       :sampler-index->sampler-name {}})))

(defn- destroy-shader-programs [^GL2 gl shader-infos _]
  (doseq [{:keys [program]} shader-infos]
    (delete-program gl program)))

(scene-cache/register-object-cache! ::shader make-shader-program update-shader-program destroy-shader-programs)
