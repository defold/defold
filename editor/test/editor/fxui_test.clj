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

(ns editor.fxui-test
  (:require [cljfx.api :as fx]
            [clojure.test :refer [deftest is]]
            [editor.fxui :as fxui]
            [editor.ui :as ui])
  (:import [javafx.event Event]
           [javafx.scene Node]
           [javafx.scene.control Label Tooltip]
           [javafx.scene.input MouseEvent]))

(set! *warn-on-reflection* true)

(defn- mouse-enter! [^Node node]
  (Event/fireEvent node
                   (MouseEvent. MouseEvent/MOUSE_ENTERED
                                0.0 0.0 0.0 0.0 nil 0
                                false false false false false
                                false false false false false nil)))

;; Verifies that tooltip graphics are deferred, reused on hover, refreshed when
;; their description changes, and disposed with their owner.
(deftest lazy-tooltip-lifecycle-test
  (ui/run-now
    (let [created (atom [])
          deleted (atom [])
          description (fn tooltip-owner-description [text]
                        {:fx/type fxui/menu-button
                         fxui/prop-lazy-tooltip
                         {:fx/type fx/ext-on-instance-lifecycle
                          :on-created (fn on-tooltip-created [tooltip]
                                        (swap! created conj tooltip))
                          :on-deleted (fn on-tooltip-deleted [tooltip]
                                        (swap! deleted conj tooltip))
                          :desc {:fx/type fxui/tooltip
                                 :graphic {:fx/type fx/ext-on-instance-lifecycle
                                           :on-created (fn on-tooltip-graphic-created [graphic]
                                                         (swap! created conj graphic))
                                           :on-deleted (fn on-tooltip-graphic-deleted [graphic]
                                                         (swap! deleted conj graphic))
                                           :desc {:fx/type fxui/label
                                                  :text text}}}}})
          component (volatile! (fx/create-component (description "original")))
          node (fx/instance @component)]
      (try
        (is (= [] @created))
        (mouse-enter! node)
        (is (= 2 (count @created)))
        (let [[graphic ^Tooltip tooltip] @created]
          (is (identical? graphic (.getGraphic tooltip)))
          (is (= "original" (.getText ^Label graphic)))
          (mouse-enter! node)
          (is (= 2 (count @created)))

          (vreset! component
                   (fx/advance-component @component (description "updated")))
          (is (= #{tooltip graphic} (set @deleted)))
          ;; Updating the description while hovered replaces the tooltip now.
          (is (= 4 (count @created)))
          (is (not (.isShowing tooltip))))

        (mouse-enter! node)
        (is (= 4 (count @created)))
        (is (= "updated" (.getText ^Label (@created 2))))
        (finally
          (fx/delete-component @component)))

      (is (= 4 (count @deleted)))
      (mouse-enter! node)
      (is (= 4 (count @created))))))

;; Verifies that disposing an unhovered owner never creates its tooltip content.
(deftest unhovered-tooltip-disposal-test
  (ui/run-now
    (let [created (atom 0)
          component (fx/create-component
                      {:fx/type fxui/menu-button
                       fxui/prop-lazy-tooltip
                       {:fx/type fx/ext-on-instance-lifecycle
                        :on-created (fn on-tooltip-created [_]
                                      (swap! created inc))
                        :desc {:fx/type fxui/tooltip :text "unused"}}})
          node (fx/instance component)]
      (fx/delete-component component)
      (mouse-enter! node)
      (is (zero? @created)))))
