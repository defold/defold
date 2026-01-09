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

(ns editor.code.preprocessors
  (:require [clojure.string :as string]
            [dynamo.graph :as g]
            [editor.code.data :as data]
            [editor.dialogs :as dialogs]
            [editor.localization :as localization]
            [editor.resource :as resource]
            [editor.ui :as ui]
            [internal.java :as java]
            [internal.util :as util]
            [service.log :as log]
            [util.coll :refer [pair]])
  (:import [com.defold.extension.pipeline ILuaPreprocessor]
           [com.dynamo.bob ClassLoaderScanner]))

(set! *warn-on-reflection* true)

(defonce ^:private ^String scanned-package-name "com.defold.extension.pipeline")

(defonce ^:private ^String smoke-test-build-variant-string (name :debug))

(defonce ^:private ^String smoke-test-lua-source-proj-path "/__lua_preprocessor_smoke_test__.lua")

(defonce ^:private ^String smoke-test-lua-source "
-- This is the Lua source transformer smoke test.
-- We instantiate Lua source transformers from project dependencies and register
-- them with our system. To ensure they confirm to our expectations, we feed
-- them this smoke-test and check that they return a string without issues. This
-- enables us to catch problems early and notify the user about potentially
-- incompatible plugins.

local m = {}

m.b = true
m.i = 1
m.s = ''
m.t = {}

function m.f()
    return nil
end

return m
")

(g/defnode CodePreprocessorsNode
  (property lua-preprocessors g/Any
            (default [])
            (dynamic visible (g/constantly false))))

(defn- report-error! [header-key faulty-class-names localization]
  (ui/run-later
    (dialogs/make-info-dialog
      localization
      {:title (localization/message "dialog.lua-preprocessors-error.title")
       :size :large
       :icon :icon/triangle-error
       :always-on-top true
       :header (localization/message header-key)
       :content (localization/message "dialog.lua-preprocessors-error.content"
                                      {"classes" (->> faulty-class-names
                                                      sort
                                                      (map dialogs/indent-with-bullet)
                                                      (string/join "\n"))})})))

(defn- run-lua-preprocessor
  ^String [^ILuaPreprocessor lua-preprocessor ^String lua-source ^String lua-source-proj-path ^String build-variant-string]
  (.preprocess lua-preprocessor lua-source lua-source-proj-path build-variant-string))

(defn- try-initialize-lua-preprocessors [^ClassLoader class-loader]
  (util/group-into
    {} []
    key val
    (keep (fn [^String class-name]
            (try
              (let [uninitialized-class (Class/forName class-name false class-loader)]
                (when (java/public-implementation? uninitialized-class ILuaPreprocessor)
                  (let [initialized-class (Class/forName class-name true class-loader)]
                    (pair :lua-preprocessor-classes initialized-class))))
              (catch Exception e
                (log/error :msg (str "Exception in static initializer of ILuaPreprocessor implementation " class-name ": " (.getMessage e))
                           :exception e)
                (pair :faulty-class-names class-name))))
          (ClassLoaderScanner/scanClassLoader class-loader scanned-package-name))))

(defn- try-create-lua-preprocessors [lua-preprocessor-classes]
  (util/group-into
    {} []
    key val
    (map (fn [^Class lua-preprocessor-class]
           (try
             (let [lua-preprocessor (java/invoke-no-arg-constructor lua-preprocessor-class)]
               ;; Perform a smoke test on the Lua preprocessor before
               ;; registering it with our system. This will help catch
               ;; incompatibilities caused by interface changes early.
               (try
                 (let [result (run-lua-preprocessor lua-preprocessor smoke-test-lua-source smoke-test-lua-source-proj-path smoke-test-build-variant-string)]
                   (if (string? result)
                     (pair :created-lua-preprocessors lua-preprocessor)
                     (let [class-name (.getName lua-preprocessor-class)]
                       (log/error :msg (str "Error when testing ILuaPreprocessor implementation " class-name " - failed to return string value."))
                       (pair :faulty-class-names class-name))))
                 (catch Exception e
                   (let [class-name (.getName lua-preprocessor-class)]
                     (log/error :msg (str "Exception when testing ILuaPreprocessor implementation " class-name ": " (.getMessage e))
                                :exception e)
                     (pair :faulty-class-names class-name)))))
             (catch Exception e
               (let [class-name (.getName lua-preprocessor-class)]
                 (log/error :msg (str "Exception when constructing ILuaPreprocessor implementation " class-name ": " (.getMessage e))
                            :exception e)
                 (pair :faulty-class-names class-name)))))
         lua-preprocessor-classes)))

(defn- set-lua-preprocessor-classes! [code-preprocessors lua-preprocessor-classes localization]
  {:pre [(every? #(and (class? %) (isa? % ILuaPreprocessor)) lua-preprocessor-classes)]}
  (let [new-lua-preprocessor-classes (set lua-preprocessor-classes)
        old-lua-preprocessor-classes (into #{}
                                           (map class)
                                           (g/node-value code-preprocessors :lua-preprocessors))]
    (when (not= old-lua-preprocessor-classes new-lua-preprocessor-classes)
      (let [{:keys [created-lua-preprocessors faulty-class-names]}
            (try-create-lua-preprocessors lua-preprocessor-classes)]
        (if (seq faulty-class-names)
          (report-error! "dialog.lua-preprocessors-error.construct.header" faulty-class-names localization)
          (g/set-property! code-preprocessors :lua-preprocessors created-lua-preprocessors))))))

(defn reload-lua-preprocessors! [code-preprocessors ^ClassLoader class-loader localization]
  (let [{:keys [lua-preprocessor-classes faulty-class-names]}
        (try-initialize-lua-preprocessors class-loader)]
    (if (seq faulty-class-names)
      (report-error! "dialog.lua-preprocessors-error.initialize.header" faulty-class-names localization)
      (set-lua-preprocessor-classes! code-preprocessors lua-preprocessor-classes localization))))

(defn preprocess-lua [lua-preprocessors ^String lua-source lua-resource build-variant]
  {:pre [(case build-variant (:debug :headless :release) true false)]}
  (if (empty? lua-preprocessors)
    lua-source
    (let [lua-source-proj-path (resource/proj-path lua-resource)
          build-variant-string (name build-variant)]
      (reduce (fn [^String lua-source ^ILuaPreprocessor lua-preprocessor]
                (run-lua-preprocessor lua-preprocessor lua-source lua-source-proj-path build-variant-string))
              lua-source
              lua-preprocessors))))

(defn preprocess-lua-lines [lua-preprocessors lua-lines lua-resource build-variant]
  (if (empty? lua-preprocessors)
    lua-lines
    (let [original-lua-source (data/lines->string lua-lines)
          preprocessed-lua-source (preprocess-lua lua-preprocessors original-lua-source lua-resource build-variant)]
      (if (identical? original-lua-source preprocessed-lua-source)
        lua-lines
        (mapv (util/make-dedupe-fn lua-lines) ; Reuse original lines where possible to avoid duplicates in the graph.
              (data/string->lines preprocessed-lua-source))))))
