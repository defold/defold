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
  "Shader loading, reflection, and OpenGL lifecycle support."
  (:require [clojure.java.io :as io]
            [clojure.string :as string]
            [dynamo.graph :as g]
            [editor.buffers :refer [bbuf->string]]
            [editor.geom :as geom]
            [editor.gl :as gl]
            [editor.gl.types :as gl.types]
            [editor.graphics.types :as graphics.types]
            [editor.localization :as localization]
            [editor.pipeline.shader-gen :as shader-gen]
            [editor.scene-cache :as scene-cache]
            [internal.util :as util]
            [util.array :as array]
            [util.coll :as coll :refer [pair]]
            [util.defonce :as defonce])
  (:import [com.dynamo.bob.pipeline ShaderUtil$Common]
           [com.jogamp.opengl GL3]
           [java.io FileNotFoundException]
           [java.nio ByteBuffer FloatBuffer IntBuffer]
           [java.nio.charset StandardCharsets]
           [javax.vecmath Matrix4d Point3d Vector4d Vector4f]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

;; ======================================================================
;; Public API

(def max-array-samplers ShaderUtil$Common/MAX_ARRAY_SAMPLERS)

(defonce/protocol ShaderVariables
  (attribute-reflection-infos [this])
  (attribute-locations [this attribute-infos])
  (set-uniform [this gl name val])
  (set-uniform-array [this gl name count val]))

(defonce/protocol SamplerVariables
  (set-samplers-by-name [this gl sampler-name texture-units])
  (set-samplers-by-index [this gl sampler-index texture-units]))

(defmulti set-uniform-at-index (fn [_ _ _ val] (class val)))
(defmulti set-uniforms-at-index (fn [_ _ _ _ val] (class val)))

(defmethod set-uniform-at-index Matrix4d
  [^GL3 gl progn loc val]
  (.glUniformMatrix4fv gl loc 1 false (float-array (geom/as-array val)) 0))

(defmethod set-uniform-at-index Vector4f
  [^GL3 gl progn loc ^Vector4f val]
  (.glUniform4f gl loc (.x val) (.y val) (.z val) (.w val)))

(defmethod set-uniform-at-index Vector4d
  [^GL3 gl progn loc ^Vector4d val]
  (.glUniform4f gl loc (.x val) (.y val) (.z val) (.w val)))

(defmethod set-uniform-at-index Point3d
  [^GL3 gl progn loc ^Point3d val]
  (.glUniform3f gl loc (float (.x val)) (float (.y val)) (float (.z val))))

(defmethod set-uniform-at-index (class (float-array []))
  [^GL3 gl progn loc ^floats val]
  (case (count val)
    3 (.glUniform4f gl loc (aget val 0) (aget val 1) (aget val 2) 1)
    4 (.glUniform4f gl loc (aget val 0) (aget val 1) (aget val 2) (aget val 3))))

(defmethod set-uniform-at-index Integer
  [^GL3 gl progn loc val]
  (.glUniform1i gl loc val))

(defmethod set-uniform-at-index Long
  [^GL3 gl progn loc val]
  (.glUniform1i gl loc (int val)))

(defmethod set-uniform-at-index nil
  [^GL3 gl progn loc val]
  ;; No-Op. This is for sampler uniforms. They are just a name. Contains no
  ;; value to set.
  nil)

(defmethod set-uniforms-at-index (class (float-array []))
  [^GL3 gl progn loc count vals]
  (let [fb (FloatBuffer/wrap vals)]
    (.glUniform4fv gl loc count fb)))

(defn program-link-errors
  [^GL3 gl progn]
  (let [msg-len (IntBuffer/allocate 1)]
    (.glGetProgramiv gl progn GL3/GL_INFO_LOG_LENGTH msg-len)
    (let [msg (ByteBuffer/allocate (.get msg-len 0))]
      (.glGetProgramInfoLog gl progn (.capacity msg) nil msg)
      (bbuf->string msg))))

(defn- delete-program [^GL3 gl ^long program]
  (when-not (zero? program)
    (.glDeleteProgram gl program)))

(defn make-program
  [^GL3 gl shaders location+attribute-name-pairs]
  (let [program (.glCreateProgram gl)]
    (doseq [^int shader shaders]
      (.glAttachShader gl program shader))
    (doseq [[^int location ^String attribute-name] location+attribute-name-pairs]
      (.glBindAttribLocation gl program location attribute-name))
    (.glLinkProgram gl program)
    (let [status (IntBuffer/allocate 1)]
      (.glGetProgramiv gl program GL3/GL_LINK_STATUS status)
      (if (= GL3/GL_TRUE (.get status 0))
        program
        (try
          (throw (Exception. (str "Program link failure.\n" (program-link-errors gl program))))
          (finally
            (delete-program gl program)))))))

(defn shader-compile-errors
  ^String [^GL3 gl shader-name]
  (let [log-length-storage (IntBuffer/allocate 1)]
    (.glGetShaderiv gl shader-name GL3/GL_INFO_LOG_LENGTH log-length-storage)
    (let [null-terminated-string-length (.get log-length-storage 0) ; Note: Some implementations return zero when the log is empty, some return one.
          string-length (dec null-terminated-string-length)]
      (if (pos? string-length)
        (let [info-log-buffer (ByteBuffer/allocate null-terminated-string-length)]
          (.glGetShaderInfoLog gl shader-name null-terminated-string-length nil info-log-buffer)
          (bbuf->string info-log-buffer 0 string-length))
        ""))))

(defn- delete-shader
  [^GL3 gl ^long shader]
  (when-not (zero? shader)
    (.glDeleteShader gl shader)))

(defn- make-shader*
  [gl-shader-type ^GL3 gl source]
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
      (.glGetShaderiv gl shader-name GL3/GL_COMPILE_STATUS status)
      (if (= GL3/GL_TRUE (.get status 0))
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
;; TODO(instancing): Move the uniform values from ShaderLifecycle into a
;; separate GLBinding.
(defonce/record ^:private ShaderRequestData
  [shader-type+source-pairs
   location+attribute-name-pairs
   array-sampler-name->uniform-names
   strip-resource-binding-namespace-regex-str
   preview-light-capacity])

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
      (.glUseProgram ^GL3 gl program)
      (when-not (zero? program)
        (doseq [[name val] uniforms
                :when (some? val)
                :let [val (if (keyword? val)
                            (get render-args val)
                            val)]]
          (set-uniform-impl! gl program uniform-infos name val)))))

  (unbind! [_this gl _render-args]
    (.glUseProgram ^GL3 gl 0))

  ShaderVariables
  (attribute-reflection-infos [_this]
    attribute-reflection-infos)

  (attribute-locations [_this attribute-infos]
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
      attribute-infos))

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
  (^ShaderRequestData [shader-type+source-pairs location+attribute-name-pairs array-sampler-name->uniform-names strip-resource-binding-namespace-regex-str]
   (make-shader-request-data shader-type+source-pairs location+attribute-name-pairs array-sampler-name->uniform-names strip-resource-binding-namespace-regex-str 0))
  (^ShaderRequestData [shader-type+source-pairs location+attribute-name-pairs array-sampler-name->uniform-names strip-resource-binding-namespace-regex-str preview-light-capacity]
   {:pre [(every? shader-type+source-pair? shader-type+source-pairs)
          (every? location+attribute-name-pair? location+attribute-name-pairs)
          (map? array-sampler-name->uniform-names)
          (or (nil? strip-resource-binding-namespace-regex-str)
              (and (string? strip-resource-binding-namespace-regex-str)
                   (pos? (count strip-resource-binding-namespace-regex-str))))
          (nat-int? preview-light-capacity)]}
   (->ShaderRequestData
     (vec shader-type+source-pairs)
     (vec location+attribute-name-pairs)
     array-sampler-name->uniform-names
     strip-resource-binding-namespace-regex-str
     preview-light-capacity)))

(defn with-preview-light-capacity
  ^ShaderRequestData [^ShaderRequestData request-data preview-light-capacity]
  {:pre [(instance? ShaderRequestData request-data)
         (nat-int? preview-light-capacity)]}
  (assoc request-data :preview-light-capacity preview-light-capacity))

(defn make-shader-lifecycle
  ^ShaderLifecycle [request-id request-data attribute-reflection-infos uniform-values-by-name]
  {:pre [(graphics.types/request-id? request-id)
         (instance? ShaderRequestData request-data)
         (vector? attribute-reflection-infos)
         (every? graphics.types/attribute-reflection-info? attribute-reflection-infos)
         (map? uniform-values-by-name)
         (every? string? (keys uniform-values-by-name))]}
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

(defn read-combined-shader-info [shader-paths opts shader-path->source]
  (let [max-page-count (long (or (:max-page-count opts) 0))
        augmented-shader-infos
        (coll/into-> shader-paths []
          (map (fn [^String shader-path]
                 (let [shader-source (shader-path->source shader-path)]
                   (shader-gen/transpile-shader-source shader-path shader-source max-page-count "mediump" "highp" :language-glsl-sm330)))))]

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
                preview-light-capacity
                strip-resource-binding-namespace-regex-str]}
        (read-combined-shader-info shader-paths opts shader-path->source)

        shader-request-data
        (-> (make-shader-request-data
              shader-type+source-pairs
              location+attribute-name-pairs
              array-sampler-name->slice-sampler-names
              strip-resource-binding-namespace-regex-str)
            (with-preview-light-capacity preview-light-capacity))

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

(defn uses-preview-light-buffer? [^ShaderLifecycle shader-lifecycle]
  (let [^ShaderRequestData request-data (.-request-data shader-lifecycle)]
    (pos? (long (.-preview-light-capacity request-data)))))

(defn preview-light-capacity
  ^long [^ShaderLifecycle shader-lifecycle]
  (let [^ShaderRequestData request-data (.-request-data shader-lifecycle)]
    (long (.-preview-light-capacity request-data))))

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

(defn page-count-mismatch-error-message-raw [is-paged-material texture-page-count material-max-page-count exclude-gles-sm100 image-property-message]
  (when (and (some? texture-page-count)
             (some? material-max-page-count))
    (let [texture-page-count (int texture-page-count)
          material-max-page-count (int material-max-page-count)]
      (cond
        (and is-paged-material
             (zero? texture-page-count))
        (localization/message "error.material-expects-paged-atlas" {"property" image-property-message})

        (and (not is-paged-material)
             (pos? texture-page-count))
        (localization/message "error.material-does-not-support-paged-atlases" {"property" image-property-message})

        (and (< material-max-page-count texture-page-count)
             (not exclude-gles-sm100))
        (localization/message "error.material-max-page-count-insufficient" {"property" image-property-message})))))

(def page-count-mismatch-error-message (memoize page-count-mismatch-error-message-raw))

(def ^:private gl-shader-parameter
  (let [out-param-value (int-array 1)]
    (fn gl-shader-parameter
      ^long [^GL3 gl ^long program param]
      (.glGetProgramiv gl program param out-param-value 0)
      (aget out-param-value 0))))

(def ^:private uniform-info
  (let [name-array-suffix-pattern #"\[\d+\]$"
        name-buffer-size 128
        out-name-length (int-array 1)
        out-array-size (int-array 1)
        out-type (int-array 1)
        out-name (byte-array name-buffer-size)]
    (fn uniform-info [^GL3 gl program uniform-index]
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

(defn- strip-resource-namespace [uniform-info strip-resource-binding-namespace-regex]
  (if strip-resource-binding-namespace-regex
    (assoc uniform-info :name (string/replace (:name uniform-info) strip-resource-binding-namespace-regex ""))
    uniform-info))

(defn- make-shader-program [^GL3 gl ^ShaderRequestData request-data]
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

        uniform-infos
        (into {}
              (map (fn [^long uniform-index]
                     (let [uniform-info (strip-resource-namespace (uniform-info gl gl-program uniform-index) strip-resource-binding-namespace-regex)]
                       (pair (:name uniform-info) uniform-info))))
              (range (gl-shader-parameter gl gl-program GL3/GL_ACTIVE_UNIFORMS)))

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

    {:program gl-program
     :uniform-infos uniform-infos
     :sampler-name->uniform-names sampler-name->uniform-names
     :sampler-index->sampler-name sampler-index->sampler-name}))

(defn- update-shader-program [^GL3 gl {:keys [program]} request-data]
  (delete-program gl program)
  (try
    (make-shader-program gl request-data)
    (catch Exception _
      {:program 0
       :uniform-infos {}
       :sampler-name->uniform-names {}
       :sampler-index->sampler-name {}})))

(defn- destroy-shader-programs [^GL3 gl shader-infos _]
  (doseq [{:keys [program]} shader-infos]
    (delete-program gl program)))

(scene-cache/register-object-cache! ::shader make-shader-program update-shader-program destroy-shader-programs)
