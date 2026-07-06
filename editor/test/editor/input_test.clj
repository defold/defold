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

(ns editor.input-test
  (:require [clojure.test :refer :all]
            [editor.input :as i])
  (:import [javafx.scene.input KeyCode]))

(deftest update-input-state
  (testing "Mouse press adds button and sets modifiers"
    (let [state (i/make-input-state)
          action {:type :mouse-pressed :button :primary :modifiers #{}}
          result (i/update-input-state state action)]
      (is (contains? (:mouse-buttons result) :primary))
      (is (empty? (:modifiers result)))))

  (testing "Mouse press with modifiers"
    (let [state (i/make-input-state)
          action {:type :mouse-pressed :button :secondary :modifiers #{:alt :shift}}
          result (i/update-input-state state action)]
      (is (contains? (:mouse-buttons result) :secondary))
      (is (= #{:alt :shift} (:modifiers result)))))

  (testing "Multiple button presses accumulate"
    (let [state (i/make-input-state)
          state (i/update-input-state state {:type :mouse-pressed :button :primary})
          state (i/update-input-state state {:type :mouse-pressed :button :secondary})]
      (is (= #{:primary :secondary} (:mouse-buttons state)))))

  (testing "Mouse release removes button"
    (let [state (-> (i/make-input-state)
                    (i/update-input-state {:type :mouse-pressed :button :secondary}))
          result (i/update-input-state state {:type :mouse-released :button :primary})]
      (is (not (contains? (:mouse-buttons result) :primary)))
      (is (contains? (:mouse-buttons result) :secondary))))

  (testing "Releasing unpressed button is no-op"
    (let [state (i/make-input-state)
          result (i/update-input-state state {:type :mouse-released :button :primary})]
      (is (empty? (:mouse-buttons result)))))

  (testing "Key press adds to pressed-keys"
    (let [state (i/make-input-state)
          action-a  {:type :key-pressed :key-code KeyCode/A :modifiers #{}}
          action-1  {:type :key-pressed :key-code KeyCode/DIGIT1 :modifiers #{}}
          action-up {:type :key-pressed :key-code KeyCode/UP :modifiers #{}}
          result (-> state
                     (i/update-input-state action-a)
                     (i/update-input-state action-1)
                     (i/update-input-state action-up))]
      (is (contains? (:pressed-keys result) KeyCode/A))
      (is (contains? (:pressed-keys result) KeyCode/DIGIT1))
      (is (contains? (:pressed-keys result) KeyCode/UP))))

  (testing "Key release updates modifiers"
    (let [state (i/make-input-state)
          action {:type :key-released :key-code KeyCode/A :modifiers #{:alt :control}}
          result (i/update-input-state state action)]
      (is (= #{:alt :control} (:modifiers result))))))

(deftest mouse-binding-action?
  (testing "button and modifier set must match exactly"
    (is (i/mouse-binding-action? {:button :primary :modifiers #{:shift}}
                                 {:button :primary :modifiers #{:shift}}))
    (is (i/mouse-binding-action? {:button :primary :modifiers #{}}
                                 {:button :primary :modifiers #{}}))
    (is (not (i/mouse-binding-action? {:button :primary :modifiers #{:shift}}
                                      {:button :primary :modifiers #{:shift :alt}})))
    (is (not (i/mouse-binding-action? {:button :primary :modifiers #{:shift}}
                                      {:button :secondary :modifiers #{:shift}}))))
  (testing "meta held disqualifies a match (meta is never part of a binding)"
    (is (not (i/mouse-binding-action? {:button :primary :modifiers #{}}
                                      {:button :primary :modifiers #{:meta}})))
    (is (not (i/mouse-binding-action? {:button :primary :modifiers #{:shift}}
                                      {:button :primary :modifiers #{:shift :meta}})))))
