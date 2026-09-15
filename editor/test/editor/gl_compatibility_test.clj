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

(ns editor.gl-compatibility-test
  (:require [clojure.java.io :as io]
            [clojure.string :as string]
            [clojure.test :refer :all]))

(set! *warn-on-reflection* true)

(def ^:private source-file-pattern #"\.(?:clj|cljc|java|vp|fp|glsl)$")

(def ^:private forbidden-api-rules
  [{:label "immediate-mode drawing"
    :pattern #"\bgl(?:Begin|End)\b"}
   {:label "immediate-mode vertex attributes"
    :pattern #"\bgl(?:Vertex|Normal|TexCoord|MultiTexCoord|SecondaryColor|EdgeFlag|Index|Rect)[234]?[bdfis]?(?:v)?\b"}
   {:label "fixed-function matrix state"
    :pattern #"\bgl(?:MatrixMode|LoadIdentity|LoadMatrix[fd]?|LoadTransposeMatrix[fd]?|MultMatrix[fd]?|MultTransposeMatrix[fd]?|PushMatrix|PopMatrix|Translate[fd]|Rotate[fd]|Scale[fd]|Ortho|Frustum)\b"}
   {:label "fixed-function matrix constants"
    :pattern #"\bGL_(?:MODELVIEW|PROJECTION)(?:_MATRIX)?\b"}
   {:label "GLSL fixed-function matrix built-ins"
    :pattern #"\bgl_(?:ModelViewProjection|ModelView|Projection|Normal|Texture)Matrix\w*\b"
    ;; The code editor must continue to recognize these names in user-authored shaders.
    :excluded-paths #{"src/clj/editor/code/shader.clj"}}
   {:label "fixed-function current color"
    :pattern #"\b(?:glColor[34][bdfis](?:v)?|GL_CURRENT_COLOR)\b"}
   {:label "fixed-function fog"
    :pattern #"\b(?:glFog[fi](?:v)?|GL_FOG(?:_[A-Z0-9_]+)?)\b"}
   {:label "line stipple"
    :pattern #"\b(?:glLineStipple|GL_LINE_STIPPLE)\b"}
   {:label "removed primitive topology"
    :pattern #"\bGL_(?:QUADS|POLYGON)\b"}
   {:label "JOGL TextRenderer"
    :pattern #"\bTextRenderer\b"}
   {:label "GLU"
    :pattern #"\bGLU\b"}])

(defn- source-files [project-directory]
  (into []
        (comp
          (map #(io/file project-directory %))
          (mapcat file-seq)
          (filter #(.isFile ^java.io.File %))
          (filter #(re-find source-file-pattern (.getName ^java.io.File %))))
        ["src" "resources/shaders"]))

(defn- project-relative-path [project-directory file]
  (-> (.relativize (.toPath (io/file project-directory))
                   (.toPath ^java.io.File file))
      str
      (string/replace "\\" "/")))

(defn- forbidden-api-violations [project-directory]
  (into []
        (mapcat
          (fn [file]
            (let [path (project-relative-path project-directory file)
                  lines (string/split-lines (slurp file))]
              (for [{:keys [label pattern excluded-paths]} forbidden-api-rules
                    :when (not (contains? excluded-paths path))
                    [line-index line] (map-indexed vector lines)
                    :when (re-find pattern line)]
                (format "%s:%d: %s: %s" path (inc line-index) label (string/trim line))))))
        (source-files project-directory)))

(defn- project-directory []
  (let [working-directory (io/file (System/getProperty "user.dir"))]
    (or (some (fn [candidate]
                (when (.isFile (io/file candidate "src/clj/editor/gl.clj"))
                  candidate))
              [working-directory (io/file working-directory "editor")])
        (throw (ex-info "Could not locate the editor project directory."
                        {:working-directory working-directory})))))

(deftest production-editor-does-not-use-compatibility-only-opengl-apis-test
  (let [project-directory (project-directory)
        violations (forbidden-api-violations project-directory)]
    (is (empty? violations)
        (str "Compatibility-only OpenGL APIs found in production editor sources:\n"
             (string/join "\n" violations)))))
