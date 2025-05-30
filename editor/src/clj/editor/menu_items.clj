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

(ns editor.menu-items
  (:require [util.fn :as fn]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

;; -----------------------------------------------------------------------------
;; Common menu definitions shared across files.
;; Keep sorted alphabetically!
;; -----------------------------------------------------------------------------

(def open-as-text "Open As")
(def open-as-icon "icons/32/Icons_S_14_linkarrow.png")
(def open-as
  {:label open-as-text
   :icon open-as-icon
   :command :file.open-as})

(def open-selected-text "Open")
(def open-selected-icon "icons/32/Icons_S_14_linkarrow.png")
(def open-selected
  {:label open-selected-text
   :icon open-selected-icon
   :command :file.open-selected})

(def push-down-overrides-text "Push Down Overrides")
(def push-down-overrides
  {:label push-down-overrides-text
   :command :edit.push-down-overrides})

(def pull-up-overrides-text "Pull Up Overrides")
(def pull-up-overrides
  {:label pull-up-overrides-text
   :command :edit.pull-up-overrides})

(def separator {:label :separator})
(def separator-with-id
  (fn/memoize
    (fn separator-with-id [id]
      {:pre [(keyword? id)]}
      (assoc separator :id id))))

(def show-overrides-text "Show Overrides...")
(def show-overrides
  {:label show-overrides-text
   :command :edit.show-overrides})
