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

(ns editor.editor-extensions.localization
  (:require [editor.editor-extensions.coerce :as coerce]
            [editor.editor-extensions.runtime :as rt]
            [editor.editor-extensions.ui-docs :as ui-docs]
            [editor.localization :as localization])
  (:import [com.defold.editor.luart DynamicToStringUserdata]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(def ^:private localizable-value-coercer
  (coerce/one-of coerce/null coerce/boolean coerce/number coerce/string ui-docs/message-pattern-coercer))

(def ^:private message-args-coercer
  (coerce/regex :key coerce/string
                :vars :? (coerce/map-of coerce/string localizable-value-coercer)))

(defn- make-ext-message-fn [localization]
  (rt/varargs-lua-fn ext-message [{:keys [rt]} varargs]
    (let [{:keys [key vars] :or {vars {}}} (rt/->clj rt message-args-coercer varargs)]
      (DynamicToStringUserdata. localization (localization/message key vars)))))

(def ^:private list-args-coercer
  (coerce/regex :items (coerce/vector-of localizable-value-coercer)))

(defn- make-ext-and-list-fn [localization]
  (rt/varargs-lua-fn ext-and-list [{:keys [rt]} varargs]
    (let [{:keys [items]} (rt/->clj rt list-args-coercer varargs)]
      (DynamicToStringUserdata. localization (localization/and-list items)))))

(defn- make-ext-or-list-fn [localization]
  (rt/varargs-lua-fn ext-or-list [{:keys [rt]} varargs]
    (let [{:keys [items]} (rt/->clj rt list-args-coercer varargs)]
      (DynamicToStringUserdata. localization (localization/or-list items)))))

(def ^:private concat-args-coercer
  (coerce/regex :items (coerce/vector-of localizable-value-coercer)
                :separator :? localizable-value-coercer))

(defn- make-ext-concat-fn [localization]
  (rt/varargs-lua-fn ext-concat [{:keys [rt]} varargs]
    (let [{:keys [items separator] :or {separator ""}} (rt/->clj rt concat-args-coercer varargs)]
      (DynamicToStringUserdata. localization (localization/join separator items)))))

(defn env [localization]
  {"message" (make-ext-message-fn localization)
   "and_list" (make-ext-and-list-fn localization)
   "or_list" (make-ext-or-list-fn localization)
   "concat" (make-ext-concat-fn localization)})