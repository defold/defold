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

(ns editor.lua
  (:require [clojure.edn :as edn]
            [clojure.java.io :as io]
            [clojure.set :as set]
            [clojure.spec.alpha :as s]
            [clojure.string :as string]
            [editor.code-completion :as code-completion]
            [editor.editor-extensions.docs :as ext-docs]
            [editor.lua-completion :as lua-completion]
            [editor.protobuf :as protobuf]
            [internal.util :as util]
            [util.eduction :as e])
  (:import [com.dynamo.scriptdoc.proto ScriptDoc$Document]
           [java.net URI]
           [org.apache.commons.io FilenameUtils]))

(set! *warn-on-reflection* true)

(def ^:private docs
  ["gameobject_script.cpp"
   "gui_script.cpp"
   "lua_base.doc_h"
   "lua_coroutine.doc_h"
   "lua_debug.doc_h"
   "lua_io.doc_h"
   "lua_math.doc_h"
   "lua_os.doc_h"
   "lua_package.doc_h"
   "lua_string.doc_h"
   "lua_table.doc_h"
   "luasocket-luasocket.doc_h"
   "profiler.cpp"
   "proto-gameobject-gameobject_ddf.proto"
   "proto-gamesys-camera_ddf.proto"
   "proto-gamesys-gui_ddf.proto"
   "proto-gamesys-label_ddf.proto"
   "proto-gamesys-model_ddf.proto"
   "proto-gamesys-physics_ddf.proto"
   "proto-gamesys-sprite_ddf.proto"
   "proto-render-render_ddf.proto"
   "render-render_script.cpp"
   "render-render_script_camera.cpp"
   "script-sys_ddf.proto"
   "script.cpp"
   "script_bitop.cpp"
   "script_crash.cpp"
   "script_graphics.cpp"
   "script_hash.cpp"
   "script_html5_js.cpp"
   "script_json.cpp"
   "script_liveupdate.h"
   "script_msg.cpp"
   "script_sys.cpp"
   "script_timer.cpp"
   "script_types.cpp"
   "script_vmath.cpp"
   "script_zlib.cpp"
   "scripts-box2d-script_box2d.cpp"
   "scripts-box2d-script_box2d_body.cpp"
   "scripts-script_buffer.cpp"
   "scripts-script_camera.cpp"
   "scripts-script_collection_factory.cpp"
   "scripts-script_collectionproxy.cpp"
   "scripts-script_factory.cpp"
   "scripts-script_font.cpp"
   "scripts-script_http.cpp"
   "scripts-script_image.cpp"
   "scripts-script_label.cpp"
   "scripts-script_model.cpp"
   "scripts-script_particlefx.cpp"
   "scripts-script_physics.cpp"
   "scripts-script_resource.cpp"
   "scripts-script_sound.cpp"
   "scripts-script_sprite.cpp"
   "scripts-script_sys_gamesys.cpp"
   "scripts-script_tilemap.cpp"
   "scripts-script_window.cpp"])

(defn- sdoc-path [doc]
  (format "doc/%s_doc.sdoc" doc))

(defn- load-sdoc [doc-name]
  (protobuf/read-map-with-defaults ScriptDoc$Document (io/resource (sdoc-path doc-name))))

(defn make-completion-map
  "Make a completion map from reducible of ns path + completion tuples

  Args:
    ns-path       a vector of strings that defines a nested context for the
                  completion, e.g. [\"socket\" \"dns\"] with the completion
                  \"toip\" defines a completion socket.dns.toip
    completion    a completion map

  Returns a map from completion context string (e.g. \"socket.dns\") to
  available completions for that context."
  [ns-path+completions]
  (letfn [(ensure-modules [acc ns-path]
            (reduce (fn [acc module-path]
                      (let [parent-path (pop module-path)
                            module-name (peek module-path)
                            k [:module module-name]]
                        (update acc (string/join "." parent-path)
                                (fn [m]
                                  (if (contains? m k)
                                    m
                                    (assoc m k (code-completion/make module-name :type :module)))))))
                    acc
                    (drop 1 (reductions conj [] ns-path))))]
    (let [context->key->completion
          (reduce
            (fn [acc [ns-path completion]]
              (-> acc
                  (ensure-modules ns-path)
                  (update (string/join "." ns-path) assoc [(:type completion) (:display-string completion)] completion)))
            {}
            ns-path+completions)]
      (into {}
            (map (fn [[context key->completion]]
                   [context (vec (sort-by :display-string (vals key->completion)))]))
            context->key->completion))))

(s/def ::documentation
  (s/map-of string? (s/coll-of ::code-completion/completion)))

(defn- defold-documentation []
  {:post [(s/assert ::documentation %)]}
  (make-completion-map
    (eduction
      (mapcat (fn [doc-name]
                (let [sdoc (load-sdoc doc-name)
                      raw-info (:info sdoc)
                      raw-elements (:elements sdoc)]
                  (eduction
                    (remove #(= :typedef (:type %)))
                    (map #(vector doc-name raw-info %))
                    raw-elements))))
      (map (fn [[doc-name raw-info raw-element]]
             (let [raw-name (:name raw-element)
                   name-parts (string/split raw-name #"\.")
                   ns-path (pop name-parts)
                   {:keys [type parameters] :as el} (assoc raw-element :name (peek name-parts))
                   raw-namespace (:namespace raw-info)
                   base-url (URI. (str "https://defold.com/ref/" raw-namespace "-lua"))
                   site-url (str "#"
                                 (string/replace
                                   (str
                                     raw-name
                                     (when (= :function type)
                                       (str ":" (string/join "-" (mapv :name parameters)))))
                                   #"[\"#*]" ""))]
               [ns-path (lua-completion/make el :base-url base-url :url site-url)])))
      docs)))

(def logic-keywords #{"and" "or" "not"})

(def self-keyword #{"self"})

(def control-flow-keywords #{"break" "do" "else" "for" "if" "elseif" "return" "then" "repeat" "while" "until" "end" "function"
                             "local" "goto" "in"})

(def defold-keywords #{"final" "init" "on_input" "on_message" "on_reload" "update" "acquire_input_focus" "disable" "enable"
                       "release_input_focus" "request_transform" "set_parent" "transform_response"})

(def lua-constants #{"nil" "false" "true"})

(def all-keywords
  (set/union logic-keywords
             self-keyword
             control-flow-keywords
             defold-keywords))

(def defold-docs (defold-documentation))
(def preinstalled-modules (into #{} (remove #{""} (keys defold-docs))))

(def base-globals
  (into #{"coroutine" "package" "string" "table" "math" "io" "file" "os" "debug"}
        (map :name)
        (:elements (load-sdoc "lua_base.doc_h"))))

(defn extract-globals-from-completions [completions]
  (into #{}
        (comp
          (remove #(#{:message :property} (:type %)))
          (map :name)
          (remove #(or (= "" %)
                       (string/includes? % ":")
                       (contains? base-globals %))))
        (get completions "")))

(def defined-globals
  (extract-globals-from-completions defold-docs))

(defn- lua-base-documentation []
  {:post [(s/assert ::documentation %)]}
  {"" (into []
            (util/distinct-by :display-string)
            (concat (map #(assoc % :type :snippet)
                         (-> (io/resource "lua-base-snippets.edn")
                             slurp
                             edn/read-string))
                    (map #(code-completion/make % :type :constant)
                         lua-constants)
                    (map #(code-completion/make % :type :keyword)
                         all-keywords)))})

(def std-libs-docs (lua-base-documentation))

(defn lua-module->path [module]
  (str "/" (string/replace module #"\." "/") ".lua"))

(defn path->lua-module [path]
  (-> (if (string/starts-with? path "/") (subs path 1) path)
      (string/replace #"/" ".")
      (FilenameUtils/removeExtension)))

(defn lua-module->build-path [module]
  (str (lua-module->path module) "c"))

(defn combine-completions
  [script-intelligence-completions]
  (merge-with (fn [dest new]
                (let [taken-display-strings (into #{} (map :display-string) dest)]
                  (into dest
                        (comp (remove #(contains? taken-display-strings (:display-string %)))
                              (util/distinct-by :display-string))
                        new)))
              defold-docs
              std-libs-docs
              script-intelligence-completions))

(def editor-completions
  (->> (ext-docs/editor-script-docs)
       (e/map
         (fn [doc]
           (let [name-parts (string/split (:name doc) #"\.")]
             [(pop name-parts) (lua-completion/make (assoc doc :name (peek name-parts)))])))
       (make-completion-map)))
