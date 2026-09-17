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

(ns editor.menu-items
  (:require [editor.localization :as localization]
            [util.fn :as fn]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

;; -----------------------------------------------------------------------------
;; Common menu definitions shared across files.
;; Keep sorted alphabetically!
;; -----------------------------------------------------------------------------

(def open-as
  {:label (localization/message "command.file.open-as")
   :icon "icons/32/Icons_S_14_linkarrow.png"
   :command :file.open-as})

(def open-selected
  {:label (localization/message "command.file.open-selected")
   :icon "icons/32/Icons_S_14_linkarrow.png"
   :command :file.open-selected})

(def pull-up-overrides-message (localization/message "command.edit.pull-up-overrides"))
(def pull-up-overrides
  {:label pull-up-overrides-message
   :icon "icons/32/Icons_62-Pull-Up-Override.png"
   :command :edit.pull-up-overrides})

(def push-down-overrides-message (localization/message "command.edit.push-down-overrides"))
(def push-down-overrides
  {:label push-down-overrides-message
   :icon "icons/32/Icons_63-Push-Down-Override.png"
   :command :edit.push-down-overrides})

(def separator {:label :separator})
(def separator-with-id
  (fn/memoize
    (fn separator-with-id [id]
      {:pre [(keyword? id)]}
      (assoc separator :id id))))

(def show-overrides
  {:label (localization/message "command.edit.show-overrides")
   :command :edit.show-overrides})
