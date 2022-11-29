;; Copyright 2020-2022 The Defold Foundation
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

(ns editor.code.preprocessors
  (:require [clojure.string :as string]
            [dynamo.graph :as g]
            [editor.code.data :as data]
            [editor.dialogs :as dialogs]
            [editor.resource :as resource]
            [editor.ui :as ui]
            [internal.java :as java]
            [internal.util :as util]
            [service.log :as log])
  (:import [com.dynamo.bob ClassLoaderScanner]
           [com.dynamo.bob.pipeline LuaBuilderPlugin]))

(set! *warn-on-reflection* true)

(def ^String smoke-test-build-variant-string (name :debug))

(def ^String smoke-test-lua-source-proj-path "/__lua_preprocessor_smoke_test__.lua")

(def ^String smoke-test-lua-source "
-- This is the Lua preprocessor smoke test.
local m = {}
m.f = function () return nil end
return m
")

(g/defnode CodePreprocessorsNode
  (property lua-preprocessors g/Any
            (default [])
            (dynamic visible (g/constantly false))))

(defn- report-error! [error-message faulty-class-names]
  (ui/run-later
    (dialogs/make-info-dialog
      {:title "Unable to Load Plugin"
       :size :large
       :icon :icon/triangle-error
       :always-on-top true
       :header error-message
       :content (string/join
                  "\n"
                  (concat
                    ["The following classes from editor plugins are not compatible with this version of the editor:"
                     ""]
                    (map dialogs/indent-with-bullet
                         (sort faulty-class-names))
                    [""
                     "The project might not build without them."
                     "Please edit your project dependencies to refer to a suitable version."]))})))

(defn- success-values [coll]
  (into []
        (keep (fn [item]
                (when (= :success (:type item))
                  (:value item))))
        coll))

(defn- run-lua-preprocessor
  ^String [^LuaBuilderPlugin lua-preprocessor ^String lua-source ^String lua-source-proj-path ^String build-variant-string]
  (.create lua-preprocessor lua-source-proj-path lua-source build-variant-string))

(defn- try-initialize-lua-preprocessors [^ClassLoader class-loader]
  (into []
        (keep (fn [^String class-name]
                (try
                  (let [uninitialized-class (Class/forName class-name false class-loader)]
                    (when (java/public-implementation? uninitialized-class LuaBuilderPlugin)
                      (let [initialized-class (Class/forName class-name true class-loader)]
                        {:type :success
                         :value initialized-class})))
                  (catch Exception e
                    (log/error :msg (str "Exception in static initializer of LuaBuilderPlugin subclass " class-name ": " (.getMessage e))
                               :exception e)
                    {:type :error
                     :class-name class-name}))))
        (ClassLoaderScanner/scanClassLoader class-loader LuaBuilderPlugin/SCANNED_PACKAGE_NAME)))

(defn- try-create-lua-preprocessors [lua-builder-plugin-subclasses]
  (into []
        (map (fn [^Class lua-builder-plugin-subclass]
               (try
                 (let [lua-preprocessor (java/invoke-no-arg-constructor lua-builder-plugin-subclass)]
                   ;; Perform a smoke test on the Lua preprocessor before
                   ;; registering it with our system. This will help catch
                   ;; incompatibilities caused by interface changes early.
                   (try
                     (let [result (run-lua-preprocessor lua-preprocessor smoke-test-lua-source smoke-test-lua-source-proj-path smoke-test-build-variant-string)]
                       (if (string? result)
                         {:type :success
                          :value lua-preprocessor}
                         (let [class-name (.getName lua-builder-plugin-subclass)]
                           (log/error :msg (str "Error when testing LuaBuilderPlugin subclass " class-name " - failed to return string value."))
                           {:type :error
                            :class-name class-name})))
                     (catch Exception e
                       (let [class-name (.getName lua-builder-plugin-subclass)]
                         (log/error :msg (str "Exception when testing LuaBuilderPlugin subclass " class-name ": " (.getMessage e))
                                    :exception e)
                         {:type :error
                          :class-name class-name}))))
                 (catch Exception e
                   (let [class-name (.getName lua-builder-plugin-subclass)]
                     (log/error :msg (str "Exception when constructing LuaBuilderPlugin subclass " class-name ": " (.getMessage e))
                                :exception e)
                     {:type :error
                      :class-name class-name})))))
        lua-builder-plugin-subclasses))

(defn- set-lua-builder-plugin-subclasses! [code-preprocessors lua-builder-plugin-subclasses]
  {:pre [(every? #(and (class? %) (isa? % LuaBuilderPlugin)) lua-builder-plugin-subclasses)]}
  (let [new-lua-builder-plugin-subclasses (set lua-builder-plugin-subclasses)
        old-lua-builder-plugin-subclasses (into #{}
                                                (map class)
                                                (g/node-value code-preprocessors :lua-preprocessors))]
    (when (not= old-lua-builder-plugin-subclasses new-lua-builder-plugin-subclasses)
      (let [create-infos (try-create-lua-preprocessors lua-builder-plugin-subclasses)
            errors (filter #(= :error (:type %)) create-infos)]
        (if (seq errors)
          (report-error! "Failed to construct Lua preprocessors" (map :class-name errors))
          (let [new-lua-preprocessors (success-values create-infos)]
            (g/set-property! code-preprocessors :lua-preprocessors new-lua-preprocessors)))))))

(defn reload-lua-preprocessors! [code-preprocessors ^ClassLoader class-loader]
  (let [initialize-infos (try-initialize-lua-preprocessors class-loader)
        errors (filter #(= :error (:type %)) initialize-infos)]
    (if (seq errors)
      (report-error! "Failed to initialize Lua preprocessors" (map :class-name errors))
      (let [lua-builder-plugin-subclasses (success-values initialize-infos)]
        (set-lua-builder-plugin-subclasses! code-preprocessors lua-builder-plugin-subclasses)))))

(defn preprocess-lua [lua-preprocessors ^String lua-source lua-resource build-variant]
  {:pre [(case build-variant (:debug :headless :release) true false)]}
  (let [lua-source-proj-path (resource/proj-path lua-resource)
        build-variant-string (name build-variant)]
    (reduce (fn [^String lua-source ^LuaBuilderPlugin lua-preprocessor]
              (run-lua-preprocessor lua-preprocessor lua-source lua-source-proj-path build-variant-string))
            lua-source
            lua-preprocessors)))

(defn preprocess-lua-lines [lua-preprocessors lua-lines lua-resource build-variant]
  (let [dedupe-fn (util/make-dedupe-fn lua-lines)
        original-lua-source (data/lines->string lua-lines)
        preprocessed-lua-source (preprocess-lua lua-preprocessors original-lua-source lua-resource build-variant)]
    (mapv dedupe-fn
          (data/string->lines preprocessed-lua-source))))
