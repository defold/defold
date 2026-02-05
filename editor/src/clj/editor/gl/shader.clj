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
  (:require [clojure.java.io :as io]
            [clojure.string :as string]
            [clojure.walk :as walk]
            [dynamo.graph :as g]
            [editor.buffers :refer [bbuf->string]]
            [editor.geom :as geom]
            [editor.gl :as gl]
            [editor.gl.types :as gl.types]
            [editor.graphics.types :as graphics.types]
            [editor.pipeline.shader-gen :as shader-gen]
            [editor.scene-cache :as scene-cache]
            [internal.util :as util]
            [util.array :as array]
            [util.coll :as coll :refer [pair]]
            [util.defonce :as defonce])
  (:import [com.dynamo.bob.pipeline ShaderUtil$Common]
           [com.jogamp.opengl GL2]
           [java.io FileNotFoundException]
           [java.nio ByteBuffer FloatBuffer IntBuffer]
           [java.nio.charset StandardCharsets]
           [javax.vecmath Matrix4d Point3d Vector4d Vector4f]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

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

(def max-array-samplers ShaderUtil$Common/MAX_ARRAY_SAMPLERS)

;; TODO(instancing): Remove once we have ported all uses of this macro.
(defmacro defshader
  "DEPRECATED. We want all shaders to go through the new transpilation and
  shader reflection pipeline. Instead, use the `classpath-shader` function with
  shader sources placed below `editor/resources/shaders` whenever you would
  previously have used an inline `defshader` expression.

  Macro to define the fragment shader program. Defines a new var whose contents
  will be the return value of `create-shader`.

  This must be submitted to the driver for compilation before you can use it.
  See `make-shader`."
  [name & body]
  `(def ~name ~(create-shader body)))

(defonce/protocol ShaderVariables
  (attribute-reflection-infos [this gl])
  (attribute-locations [this gl attribute-infos])
  (set-uniform [this gl name val])
  (set-uniform-array [this gl name count val]))

(defonce/protocol SamplerVariables
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
  nil)

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

(defn- delete-program [^GL2 gl ^long program]
  (when-not (zero? program)
    (.glDeleteProgram gl program)))

(defn make-program
  [^GL2 gl shaders location+attribute-name-pairs]
  (let [program (.glCreateProgram gl)]
    (doseq [^int shader shaders]
      (.glAttachShader gl program shader))
    (doseq [[^int location ^String attribute-name] location+attribute-name-pairs]
      (.glBindAttribLocation gl program location attribute-name))
    (.glLinkProgram gl program)
    (let [status (IntBuffer/allocate 1)]
      (.glGetProgramiv gl program GL2/GL_LINK_STATUS status)
      (if (= GL2/GL_TRUE (.get status 0))
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

(defn- delete-shader
  [^GL2 gl ^long shader]
  (when-not (zero? shader)
    (.glDeleteShader gl shader)))

(defn- make-shader*
  [gl-shader-type ^GL2 gl source]
  ;; Shader source can be either a string or a collection of strings.
  ;; However, it is not intended to be a collection of lines. The
  ;; shader compiler will simply read from each string in turn as if
  ;; they were concatenated. Thus, you need to have newline characters
  ;; at the end of each line.
  (assert (or (string? source) (coll? source)))
  (let [shader-name (.glCreateShader gl gl-shader-type)
        source-strings (if (string? source)
                         [source]
                         (vec source))
        source-strings-array (array/from-type String source-strings)]
    (.glShaderSource gl shader-name (count source-strings) source-strings-array nil)
    (.glCompileShader gl shader-name)
    (let [status (IntBuffer/allocate 1)]
      (.glGetShaderiv gl shader-name GL2/GL_COMPILE_STATUS status)
      (if (= GL2/GL_TRUE (.get status 0))
        shader-name
        (try
          (let [error-log (shader-compile-errors gl shader-name)]
            (throw (ex-info (str "Shader compilation failure.\n" error-log)
                            {:source-strings source-strings
                             :error-log error-log})))
          (finally
            (delete-shader gl shader-name)))))))

(defn- set-uniform-impl! [gl program uniform-infos uniform-name uniform-value]
  (when-some [uniform-info (uniform-infos uniform-name)]
    (try
      (set-uniform-at-index gl program (:location uniform-info) uniform-value)
      (catch IllegalArgumentException e
        (throw (IllegalArgumentException. (format "Failed setting uniform '%s'." uniform-name) e))))))

(defn- set-sampler-uniform-impl! [gl program uniform-infos slice-sampler-uniform-names texture-units]
  (doall
    (map (fn [slice-sampler-uniform-name texture-unit]
           (if (nat-int? texture-unit)
             (when-some [slice-sampler-uniform-info (uniform-infos slice-sampler-uniform-name)]
               (set-uniform-at-index gl program (:location slice-sampler-uniform-info) texture-unit))
             (throw (IllegalArgumentException. (format "Invalid texture unit '%s' for uniform '%s'." texture-unit slice-sampler-uniform-name)))))
         slice-sampler-uniform-names
         texture-units)))

;; TODO(instancing): Eventually we want to move the stuff related to uniform
;; reflection out of here. In here, it unnecessarily becomes part of the request
;; data comparison. If we are able to use uniform reflection info from the
;; shader transpiler instead, we don't need to feed it into the scene cache
;; request in order to get it from the OpenGL context. It could reside in the
;; ShaderLifecycle beside the attribute-reflection-infos.
(defonce/record ^:private ShaderRequestData
  [shader-type+source-pairs
   location+attribute-name-pairs
   array-sampler-name->uniform-names
   strip-resource-binding-namespace-regex-str])

(defonce/record ShaderLifecycle
  [request-id
   ^ShaderRequestData request-data
   attribute-reflection-infos
   name-key->attribute-location
   semantic-type->attribute-locations
   uniforms]

  gl.types/GLBinding
  (bind! [_this gl render-args]
    (let [{:keys [^int program uniform-infos]} (scene-cache/request-object! ::shader request-id gl request-data)]
      (.glUseProgram ^GL2 gl program)
      (when-not (zero? program)
        (doseq [[name val] uniforms
                :when (some? val)
                :let [val (if (keyword? val)
                            (get render-args val)
                            val)]]
          (set-uniform-impl! gl program uniform-infos name val)))))

  (unbind! [_this gl _render-args]
    (.glUseProgram ^GL2 gl 0))

  ShaderVariables
  (attribute-reflection-infos [_this gl]
    (cond
      (pos? (count attribute-reflection-infos))
      attribute-reflection-infos

      (some? gl)
      (:attribute-reflection-infos (scene-cache/request-object! ::shader request-id gl request-data))

      :else
      (throw
        (ex-info
          "The ShaderLifecycle does not have attribute reflection info, presumably because it was created using the deprecated defshader macro. You must provide a valid OpenGL context."
          {:request-id request-id
           :request-data request-data}))))

  (attribute-locations [_this gl attribute-infos]
    ;; TODO(instancing): Once we eliminate all defshader expressions and every
    ;; shader has reflection information, we can get rid of this fallback and
    ;; the gl argument.
    (let [[name-key->attribute-location semantic-type->attribute-locations]
          (if (pos? (count name-key->attribute-location))
            ;; This shader has reflection information - no need to query gl.
            (pair name-key->attribute-location semantic-type->attribute-locations)

            ;; This shader does not have reflection information. Presumably, it
            ;; was declared using the deprecated defshader macro. Otherwise,
            ;; we'd expect at least a position attribute to be present. We need
            ;; to get reflection information using the gl context.
            (let [{:keys [name-key->attribute-location semantic-type->attribute-locations]}
                  (scene-cache/request-object! ::shader request-id gl request-data)]
              (pair name-key->attribute-location semantic-type->attribute-locations)))]

      (reduce
        (fn [attribute-locations attribute-info]
          (if-let [location (name-key->attribute-location (:name-key attribute-info))]
            ;; We found an exact match for the attribute name in the shader.
            (conj attribute-locations location)

            ;; We don't have an exact match for the attribute name. Try to match
            ;; the first available attribute with our semantic-type.
            (let [semantic-type (:semantic-type attribute-info)
                  shader-locations-for-semantic-type (semantic-type->attribute-locations semantic-type)
                  location (coll/first-where
                             #(= -1 (coll/index-of attribute-locations %)) ; Presumably, a linear scan is fast given the small number of attributes.
                             shader-locations-for-semantic-type)]
              (conj attribute-locations (or location -1)))))
        (vector-of :int)
        attribute-infos)))

  (set-uniform [_this gl name val]
    (assert (string? (not-empty name)))
    (when-let [{:keys [^int program uniform-infos]} (scene-cache/request-object! ::shader request-id gl request-data)]
      (when (and (not (zero? program)) (= program (gl/gl-current-program gl)))
        (set-uniform-impl! gl program uniform-infos name val))))

  (set-uniform-array [_this gl name count vals]
    (assert (string? (not-empty name)))
    (when-let [{:keys [^int program uniform-infos]} (scene-cache/request-object! ::shader request-id gl request-data)]
      (when (and (not (zero? program)) (= program (gl/gl-current-program gl)))
        (when-some [uniform-info (uniform-infos name)]
          (try
            (set-uniforms-at-index gl program (:location uniform-info) count vals)
            (catch IllegalArgumentException e
              (throw (IllegalArgumentException. (format "Failed setting array uniform '%s'." name) e))))))))

  SamplerVariables
  (set-samplers-by-name [_this gl sampler-name texture-units]
    (assert (string? (not-empty sampler-name)))
    (when-let [{:keys [^int program uniform-infos sampler-name->uniform-names]}
               (scene-cache/request-object! ::shader request-id gl request-data)]
      (when (and (not (zero? program)) (= program (gl/gl-current-program gl)))
        (when-some [uniform-names (sampler-name->uniform-names sampler-name)]
          (try
            (set-sampler-uniform-impl! gl program uniform-infos uniform-names texture-units)
            (catch IllegalArgumentException e
              (throw (IllegalArgumentException. (format "Failed setting sampler uniform '%s'." sampler-name) e))))))))

  (set-samplers-by-index [_this gl sampler-index texture-units]
    (when-let [{:keys [^int program uniform-infos sampler-index->sampler-name sampler-name->uniform-names]}
               (scene-cache/request-object! ::shader request-id gl request-data)]
      (when (and (not (zero? program)) (= program (gl/gl-current-program gl)))
        (when-some [sampler-name (sampler-index->sampler-name sampler-index)]
          (let [uniform-names (sampler-name->uniform-names sampler-name)]
            (try
              (set-sampler-uniform-impl! gl program uniform-infos uniform-names texture-units)
              (catch IllegalArgumentException e
                (throw (IllegalArgumentException. (format "Failed setting sampler uniform '%s' at index %d." sampler-name sampler-index) e))))))))))

(defn- shader-type+source-pair? [value]
  (and (vector? value)
       (= 2 (count value))
       (let [[shader-type shader-source] value]
         (and (gl.types/gl-compatible-shader-type? shader-type)
              (string? shader-source)
              (pos? (count shader-source))))))

(defn- location+attribute-name-pair? [value]
  (and (vector? value)
       (= 2 (count value))
       (let [[location attribute-name] value]
         (and (nat-int? location)
              (string? attribute-name)
              (pos? (count attribute-name))))))

(defn make-shader-request-data
  ^ShaderRequestData [shader-type+source-pairs location+attribute-name-pairs array-sampler-name->uniform-names strip-resource-binding-namespace-regex-str]
  {:pre [(every? shader-type+source-pair? shader-type+source-pairs)
         (every? location+attribute-name-pair? location+attribute-name-pairs)
         (map? array-sampler-name->uniform-names)
         (or (nil? strip-resource-binding-namespace-regex-str)
             (and (string? strip-resource-binding-namespace-regex-str)
                  (pos? (count strip-resource-binding-namespace-regex-str))))]}
  (->ShaderRequestData
    (vec shader-type+source-pairs)
    (vec location+attribute-name-pairs)
    array-sampler-name->uniform-names
    strip-resource-binding-namespace-regex-str))

(defn make-shader-lifecycle
  ^ShaderLifecycle [request-id request-data attribute-reflection-infos uniform-values-by-name]
  {:pre [(graphics.types/request-id? request-id)
         (instance? ShaderRequestData request-data)
         (vector? attribute-reflection-infos)
         (every? graphics.types/attribute-reflection-info? attribute-reflection-infos)
         (map? uniform-values-by-name)
         (every? string? (keys uniform-values-by-name))]}
  ;; TODO(instancing): We have attribute-reflection-infos after transpiling the
  ;; shader, so we shouldn't need an active OpenGL context to perform shader
  ;; reflection at render-time. However, we first need to update all the
  ;; editor-specific shaders created using the defshader macro to pass through
  ;; the same transpilation pipeline as the material shaders. Unfortunately this
  ;; also involves porting them to #version 140 first, since the transpiler does
  ;; not support versions earlier than that. All such shaders have now been
  ;; removed from our editor plugins, so no need to update those.
  (let [name-key->attribute-location (coll/pair-map-by :name-key :location attribute-reflection-infos)
        semantic-type->attribute-locations (util/group-into {} (vector-of :int) :semantic-type :location attribute-reflection-infos)]
    (->ShaderLifecycle
      request-id
      request-data
      attribute-reflection-infos
      name-key->attribute-location
      semantic-type->attribute-locations
      uniform-values-by-name)))

(defn shader-lifecycle? [value]
  (instance? ShaderLifecycle value))

(defn make-shader
  (^ShaderLifecycle [request-id vertex-shader-source fragment-shader-source]
   (make-shader request-id vertex-shader-source fragment-shader-source {} {} nil))
  (^ShaderLifecycle [request-id vertex-shader-source fragment-shader-source uniforms]
   (make-shader request-id vertex-shader-source fragment-shader-source uniforms {} nil))
  (^ShaderLifecycle [request-id vertex-shader-source fragment-shader-source uniforms array-sampler-name->uniform-names]
   (make-shader request-id vertex-shader-source fragment-shader-source uniforms array-sampler-name->uniform-names nil))
  (^ShaderLifecycle [request-id vertex-shader-source fragment-shader-source uniforms array-sampler-name->uniform-names strip-resource-binding-namespace-regex-str]
   ;; TODO(instancing): This is used on the output of the defshader macro, which
   ;; we'd like to get rid of. Once we do, we can rename make-shader-lifecycle
   ;; to make-shader.
   ;;
   ;; TODO(instancing): We don't have attribute location information unless we
   ;; go through shader-gen/transpile-shader-source. Trouble is, the transpiler
   ;; only works on GLSL sources of #version 140 or higher. The defshader macro
   ;; emits an older syntax that is incompatible with the transpiler. The lack
   ;; of attribute location information results in the driver choosing whatever
   ;; locations it likes for the attributes. It also forces us to have a current
   ;; OpenGL context while looking up the attribute locations. For the dynamic
   ;; VertexBuffer class, we use the ShaderVariables attribute-locations method
   ;; to locate the attributes. It will fall back on OpenGL context reflection
   ;; if there is no attribute reflection info, but the AttributeBufferBinding
   ;; class will not work with a shader returned from here since it relies on
   ;; the attribute reflection info being pre-determined by the shader
   ;; reflection stage of the transpiler.
   (let [location+attribute-name-pairs []
         attribute-reflection-infos []

         shader-type+source-pairs
         [(pair :shader-type-vertex vertex-shader-source)
          (pair :shader-type-fragment fragment-shader-source)]

         request-data
         (make-shader-request-data
           shader-type+source-pairs
           location+attribute-name-pairs
           array-sampler-name->uniform-names
           strip-resource-binding-namespace-regex-str)]

     (make-shader-lifecycle request-id request-data attribute-reflection-infos uniforms))))

(defn read-combined-shader-info [shader-paths opts shader-path->source]
  (let [max-page-count (long (or (:max-page-count opts) 0))

        augmented-shader-infos
        (coll/into-> shader-paths []
          (map (fn [^String shader-path]
                 (let [shader-source (shader-path->source shader-path)]
                   (shader-gen/transpile-shader-source shader-path shader-source max-page-count)))))]

    (shader-gen/combined-shader-info augmented-shader-infos)))

(defn read-shader
  ^ShaderLifecycle [request-type shader-paths opts shader-path->source]
  {:pre [(keyword? request-type)]}
  (let [coordinate-space (:coordinate-space opts)
        max-page-count (or (:max-page-count opts) 0)
        uniform-values-by-name (or (:uniforms opts) {})
        _ (assert (graphics.types/concrete-coordinate-space? coordinate-space))
        _ (assert (nat-int? max-page-count))

        request-id
        {:request-type request-type
         :max-page-count max-page-count
         :shader-paths (vec (sort shader-paths))}

        {:keys [array-sampler-name->slice-sampler-names
                attribute-reflection-infos
                location+attribute-name-pairs
                shader-type+source-pairs
                strip-resource-binding-namespace-regex-str]}
        (read-combined-shader-info shader-paths opts shader-path->source)

        shader-request-data
        (make-shader-request-data
          shader-type+source-pairs
          location+attribute-name-pairs
          array-sampler-name->slice-sampler-names
          strip-resource-binding-namespace-regex-str)

        attribute-reflection-infos
        (mapv #(editor.graphics.types/assign-attribute-transform % coordinate-space)
              attribute-reflection-infos)]

    (make-shader-lifecycle request-id shader-request-data attribute-reflection-infos uniform-values-by-name)))

(defn- classpath-shader-path->source
  ^String [^String shader-path]
  (if-some [url (io/resource shader-path)]
    (slurp url)
    (throw (FileNotFoundException. (format "'%s' was not found on the classpath." shader-path)))))

(defn classpath-shader
  ^ShaderLifecycle [opts & shader-paths]
  {:pre [(map? opts)]}
  (if (coll/empty? shader-paths)
    (throw (IllegalArgumentException. "At least one shader-path must be supplied."))
    (read-shader :classpath-shader shader-paths opts classpath-shader-path->source)))

(defn is-using-array-samplers? [^ShaderLifecycle shader-lifecycle]
  (let [^ShaderRequestData request-data (.-request-data shader-lifecycle)
        array-sampler-name->uniform-names (.-array-sampler-name->uniform-names request-data)]
    (pos? (count array-sampler-name->uniform-names))))

(defn- first-shader-source-of-type
  ^String [shader-type ^ShaderLifecycle shader-lifecycle]
  (some (fn [shader-type+source-pair]
          (when (= shader-type (first shader-type+source-pair))
            (second shader-type+source-pair)))
        (let [^ShaderRequestData request-data (.-request-data shader-lifecycle)]
          (.-shader-type+source-pairs request-data))))

;; Used by tests.
(def vertex-shader-source (partial first-shader-source-of-type :shader-type-vertex))
(def fragment-shader-source (partial first-shader-source-of-type :shader-type-fragment))

(defn page-count-mismatch-error-message-raw [is-paged-material texture-page-count material-max-page-count image-property-name]
  (when (and (some? texture-page-count)
             (some? material-max-page-count))
    (let [texture-page-count (int texture-page-count)
          material-max-page-count (int material-max-page-count)]
      (cond
        (and is-paged-material
             (zero? texture-page-count))
        (str "The Material expects a paged Atlas, but the selected " image-property-name " is not paged")

        (and (not is-paged-material)
             (pos? texture-page-count))
        (str "The Material does not support paged Atlases, but the selected " image-property-name " is paged")

        (< material-max-page-count texture-page-count)
        (str "The Material's 'Max Page Count' is not sufficient for the number of pages in the selected " image-property-name)))))

(def page-count-mismatch-error-message (memoize page-count-mismatch-error-message-raw))

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
        out-array-size (int-array 1)
        out-type (int-array 1)
        out-name (byte-array name-buffer-size)]
    (fn uniform-info [^GL2 gl program uniform-index]
      {:post [(graphics.types/uniform-reflection-info? %)]}
      (.glGetActiveUniform gl program uniform-index name-buffer-size out-name-length 0 out-array-size 0 out-type 0 out-name 0)
      (let [name-length (aget out-name-length 0)
            name (String. out-name 0 name-length StandardCharsets/UTF_8)
            base-location (.glGetUniformLocation gl program name)
            uniform-type (gl.types/gl-uniform-type-uniform-type (aget out-type 0))
            array-size (aget out-array-size 0)
            ;; 1. strip brackets from uniform name, i.e "uniform_name[123]"
            sanitized-name (string/replace name name-array-suffix-pattern "")]
        {:name sanitized-name
         :uniform-type uniform-type
         :location base-location
         :array-size array-size}))))

;; TODO(instancing): Remove this once we get rid of all defshader expressions
;; and have shader reflection information for all shaders.
(def ^:private attribute-reflection-info
  (let [name-buffer-size 128
        out-name-length (int-array 1)
        out-array-size (int-array 1)
        out-gl-type (int-array 1)
        out-name (byte-array name-buffer-size)]
    (fn attribute-reflection-info [^GL2 gl program attribute-index]
      {:post [(graphics.types/attribute-reflection-info? %)]}
      (.glGetActiveAttrib gl program attribute-index name-buffer-size out-name-length 0 out-array-size 0 out-gl-type 0 out-name 0)
      (let [name-length (aget out-name-length 0)]
        (when (pos? name-length)
          (when-let [[vector-type data-type] (gl.types/gl-attribute-type-vector-type+data-type (aget out-gl-type 0))]
            (let [name (String. out-name 0 name-length StandardCharsets/UTF_8)
                  name-key (graphics.types/attribute-name-key name)
                  inferred-semantic-type (graphics.types/infer-semantic-type name-key)
                  base-location (.glGetAttribLocation gl program name)
                  array-size (aget out-array-size 0)]
              {:name name
               :name-key name-key
               :semantic-type inferred-semantic-type
               :vector-type vector-type
               :data-type data-type
               :normalize false
               :coordinate-space :coordinate-space-default
               :step-function :vertex-step-function-vertex
               :location base-location
               :array-size array-size})))))))

(defn- strip-resource-namespace [uniform-info strip-resource-binding-namespace-regex]
  (if strip-resource-binding-namespace-regex
    (assoc uniform-info :name (string/replace (:name uniform-info) strip-resource-binding-namespace-regex ""))
    uniform-info))

(defn- make-shader-program [^GL2 gl ^ShaderRequestData request-data]
  (let [gl-program
        (let [gl-shaders
              (reduce
                (fn [gl-shaders shader-type+source-pair]
                  (try
                    (let [[shader-type shader-source] shader-type+source-pair
                          gl-shader-type (gl.types/shader-type-gl-type shader-type)
                          gl-shader (make-shader* gl-shader-type gl shader-source)]
                      (conj gl-shaders gl-shader))
                    (catch Throwable error
                      ;; One of the input shaders failed to compile. Clean up
                      ;; any successfully created shaders before re-throwing.
                      (doseq [gl-shader (rseq gl-shaders)]
                        (.glDeleteShader gl gl-shader))
                      (throw error))))
                (vector-of :int)
                (.-shader-type+source-pairs request-data))]
          (try
            ;; This attaches all the created gl-shaders to the gl-program,
            ;; increasing their reference count.
            (make-program gl gl-shaders (.-location+attribute-name-pairs request-data))
            (finally
              ;; Regardless of if the created gl-shaders failed to link, we
              ;; should decrease their reference count now that they have been
              ;; attached to the gl-program.
              (doseq [gl-shader (rseq gl-shaders)]
                (.glDeleteShader gl gl-shader)))))

        strip-resource-binding-namespace-regex (some-> (.-strip-resource-binding-namespace-regex-str request-data) re-pattern)
        array-sampler-name->uniform-names (.-array-sampler-name->uniform-names request-data)

        attribute-reflection-infos
        (into []
              (keep (fn [^long attribute-index]
                      (attribute-reflection-info gl gl-program attribute-index)))
              (range (gl-shader-parameter gl gl-program GL2/GL_ACTIVE_ATTRIBUTES)))

        name-key->attribute-location (coll/pair-map-by :name-key :location attribute-reflection-infos)
        semantic-type->attribute-locations (util/group-into {} (vector-of :int) :semantic-type :location attribute-reflection-infos)

        uniform-infos
        (into {}
              (map (fn [^long uniform-index]
                     (let [uniform-info (strip-resource-namespace (uniform-info gl gl-program uniform-index) strip-resource-binding-namespace-regex)]
                       (pair (:name uniform-info) uniform-info))))
              (range (gl-shader-parameter gl gl-program GL2/GL_ACTIVE_UNIFORMS)))

        array-sampler-uniform-name?
        (into #{}
              (mapcat val)
              array-sampler-name->uniform-names)

        sampler-name->uniform-names
        (into array-sampler-name->uniform-names
              (keep (fn [[uniform-name uniform-info]]
                      (when (graphics.types/sampler-uniform-type? (:uniform-type uniform-info))
                        (when-not (array-sampler-uniform-name? uniform-name)
                          (pair uniform-name [uniform-name])))))
              uniform-infos)

        sampler-index->sampler-name
        (->> sampler-name->uniform-names
             (mapv (fn [[sampler-name uniform-names]]
                     (pair sampler-name
                           (uniform-infos (first uniform-names)))))
             (sort-by (comp :location second))
             (mapv first))]

    ;; TODO(instancing): Move most of this stuff out of here once we've gotten
    ;; rid of the defshader macro. We should be able to obtain attribute
    ;; reflection info during the shader transpilation step, and we could also
    ;; move the uniform values out to a separate kind of GLBinding for uniforms.
    ;; The attribute reflection info is already available in any ShaderLifecycle
    ;; that is not created from a defshader expression.
    {:program gl-program
     :uniform-infos uniform-infos
     :attribute-reflection-infos attribute-reflection-infos
     :name-key->attribute-location name-key->attribute-location
     :semantic-type->attribute-locations semantic-type->attribute-locations
     :sampler-name->uniform-names sampler-name->uniform-names
     :sampler-index->sampler-name sampler-index->sampler-name}))

(defn- update-shader-program [^GL2 gl {:keys [program]} request-data]
  (delete-program gl program)
  (try
    (make-shader-program gl request-data)
    (catch Exception _
      {:program 0
       :uniform-infos {}
       :attribute-reflection-infos []
       :name-key->attribute-location {}
       :semantic-type->attribute-locations {}
       :sampler-name->uniform-names {}
       :sampler-index->sampler-name {}})))

(defn- destroy-shader-programs [^GL2 gl shader-infos _]
  (doseq [{:keys [program]} shader-infos]
    (delete-program gl program)))

(scene-cache/register-object-cache! ::shader make-shader-program update-shader-program destroy-shader-programs)
