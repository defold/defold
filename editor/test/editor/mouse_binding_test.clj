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

(ns editor.mouse-binding-test
  (:require [clojure.test :refer [deftest is testing]]
            [editor.mouse-binding :as mouse-binding]))

(defmacro with-mouse-bindings [& forms]
  `(let [old-bindings# @mouse-binding/bindings-atom]
     (try
       (reset! mouse-binding/bindings-atom {:contexts {}})
       ~@forms
       (finally
         (reset! mouse-binding/bindings-atom old-bindings#)))))

(deftest default-bound-command-override-workflow
  (with-mouse-bindings
    (mouse-binding/register! ::camera "Scene 2D Camera"
                             [{:command :scene.camera.pan
                               :action ["Pan"]
                               :binding {:button :primary :modifiers [:shift]}}])
    (testing "registered default is active"
      (is (= :scene.camera.pan
             (mouse-binding/command-for-action ::camera {:button :primary :shift true})))
      (is (mouse-binding/command-active?
            ::camera
            :scene.camera.pan
            {:mouse-buttons #{:primary} :modifiers #{:shift}}))
      (is (= {:kind :mouse-binding
              :context ::camera
              :command :scene.camera.pan
              :action ["Pan"]
              :context-path "Scene 2D Camera"
              :binding-source :default
              :fallback-context-path nil
              :bindings [{:button :primary :modifiers [:shift]}]}
             (mouse-binding/command-row {} ::camera :scene.camera.pan))))
    (let [overrides-1 (mouse-binding/update-command-bindings
                        {}
                        ::camera
                        :scene.camera.pan
                        [{:button :secondary :modifiers [:control]}])]
      (testing "single custom override replaces default binding"
        (is (= {::camera
                {:scene.camera.pan
                 {:bindings [{:button :secondary :modifiers [:control]}]}}}
               overrides-1))
        (mouse-binding/set-user-overrides! overrides-1)
        (is (= :scene.camera.pan
               (mouse-binding/command-for-action ::camera {:button :secondary :control true})))
        (is (nil?
              (mouse-binding/command-for-action ::camera {:button :primary :shift true})))
        (is (= {:kind :mouse-binding
                :context ::camera
                :command :scene.camera.pan
                :action ["Pan"]
                :context-path "Scene 2D Camera"
                :binding-source :custom
                :fallback-context-path nil
                :bindings [{:button :secondary :modifiers [:control]}]}
               (mouse-binding/command-row overrides-1 ::camera :scene.camera.pan))))
      (let [overrides-2 (mouse-binding/update-command-bindings
                          overrides-1
                          ::camera
                          :scene.camera.pan
                          [{:button :secondary :modifiers [:control]}
                           {:button :middle :modifiers [:alt]}])]
        (testing "appended custom bindings are effective"
          (mouse-binding/set-user-overrides! overrides-2)
          (is (= :scene.camera.pan
                 (mouse-binding/command-for-action ::camera {:button :middle :alt true})))
          (is (= [{:command :scene.camera.pan
                   :action ["Pan"]
                   :context ::camera
                   :context-path "Scene 2D Camera"
                   :bindings [{:button :secondary :modifiers [:control]}
                              {:button :middle :modifiers [:alt]}]}]
                 (mouse-binding/all-bindings))))
        (let [overrides-3 (mouse-binding/remove-command-binding overrides-2 ::camera :scene.camera.pan 0)]
          (testing "removing one binding keeps the remaining custom binding"
            (is (= {::camera
                    {:scene.camera.pan
                     {:bindings [{:button :middle :modifiers [:alt]}]}}}
                   overrides-3))
            (mouse-binding/set-user-overrides! overrides-3)
            (is (= :scene.camera.pan
                   (mouse-binding/command-for-action ::camera {:button :middle :alt true}))))
          (let [overrides-4 (mouse-binding/remove-command-binding overrides-3 ::camera :scene.camera.pan 0)]
            (testing "removing the final binding preserves an explicit empty override"
              (is (= {::camera
                      {:scene.camera.pan
                       {:bindings []}}}
                     overrides-4))
              (mouse-binding/set-user-overrides! overrides-4)
              (is (nil?
                    (mouse-binding/command-for-action ::camera {:button :middle :alt true})))
              (is (not
                    (mouse-binding/command-active?
                      ::camera
                      :scene.camera.pan
                      {:mouse-buttons #{:primary} :modifiers #{:shift}})))
              (is (= {:kind :mouse-binding
                      :context ::camera
                      :command :scene.camera.pan
                      :action ["Pan"]
                      :context-path "Scene 2D Camera"
                      :binding-source :custom
                      :fallback-context-path nil
                      :bindings []}
                     (mouse-binding/command-row overrides-4 ::camera :scene.camera.pan)))
              (is (= [{:command :scene.camera.pan
                       :action ["Pan"]
                       :context ::camera
                       :context-path "Scene 2D Camera"
                       :bindings []}]
                     (mouse-binding/all-bindings))))
            (let [overrides-5 (mouse-binding/reset-command-bindings overrides-4 ::camera :scene.camera.pan)]
              (testing "reset removes the override and restores defaults"
                (is (= {} overrides-5))
                (mouse-binding/set-user-overrides! overrides-5)
                (is (= :scene.camera.pan
                       (mouse-binding/command-for-action ::camera {:button :primary :shift true})))
                (is (= {:kind :mouse-binding
                        :context ::camera
                        :command :scene.camera.pan
                        :action ["Pan"]
                        :context-path "Scene 2D Camera"
                        :binding-source :default
                        :fallback-context-path nil
                        :bindings [{:button :primary :modifiers [:shift]}]}
                       (mouse-binding/command-row overrides-5 ::camera :scene.camera.pan)))))))))))

(deftest fallback-context-workflow
  (with-mouse-bindings
    (mouse-binding/register! ::base "Scene 2D Camera"
                             [{:command :scene.camera.pan
                               :action ["Pan"]
                               :binding {:button :primary :modifiers [:shift]}}])
    (mouse-binding/register! ::derived "Tile Map Editor"
                             [{:command :scene.camera.pan
                               :action ["Pan"]}]
                             {:fallback-context ::base})
    (testing "derived context inherits from its fallback"
      (is (= ::base (mouse-binding/fallback-context ::derived)))
      (is (= :scene.camera.pan
             (mouse-binding/command-for-action ::derived {:button :primary :shift true})))
      (is (mouse-binding/command-active?
            ::derived
            :scene.camera.pan
            {:mouse-buttons #{:primary} :modifiers #{:shift}}))
      (is (= {:kind :mouse-binding
              :context ::derived
              :command :scene.camera.pan
              :action ["Pan"]
              :context-path "Tile Map Editor"
              :binding-source :inherited
              :fallback-context-path "Scene 2D Camera"
              :bindings [{:button :primary :modifiers [:shift]}]}
             (mouse-binding/command-row {} ::derived :scene.camera.pan))))
    (let [overrides-1 (mouse-binding/update-command-bindings
                        {}
                        ::derived
                        :scene.camera.pan
                        [{:button :secondary :modifiers []}])]
      (testing "derived override takes precedence over inherited binding"
        (mouse-binding/set-user-overrides! overrides-1)
        (is (= :scene.camera.pan
               (mouse-binding/command-for-action ::derived {:button :secondary})))
        (is (= {:kind :mouse-binding
                :context ::derived
                :command :scene.camera.pan
                :action ["Pan"]
                :context-path "Tile Map Editor"
                :binding-source :custom
                :fallback-context-path nil
                :bindings [{:button :secondary :modifiers []}]}
               (mouse-binding/command-row overrides-1 ::derived :scene.camera.pan))))
      (let [overrides-2 (mouse-binding/reset-command-bindings overrides-1 ::derived :scene.camera.pan)]
        (testing "reset returns the row to inherited fallback behavior"
          (is (= {} overrides-2))
          (mouse-binding/set-user-overrides! overrides-2)
          (is (= :scene.camera.pan
                 (mouse-binding/command-for-action ::derived {:button :primary :shift true})))
          (is (= {:kind :mouse-binding
                  :context ::derived
                  :command :scene.camera.pan
                  :action ["Pan"]
                  :context-path "Tile Map Editor"
                  :binding-source :inherited
                  :fallback-context-path "Scene 2D Camera"
                  :bindings [{:button :primary :modifiers [:shift]}]}
                 (mouse-binding/command-row overrides-2 ::derived :scene.camera.pan)))))
      (mouse-binding/register! ::derived "Tile Map Editor"
                               [{:command :scene.camera.pan
                                 :action ["Pan"]}])
      (testing "re-registering without fallback clears inherited behavior"
        (is (nil? (mouse-binding/fallback-context ::derived)))
        (is (nil? (mouse-binding/command-for-action ::derived {:button :primary :shift true})))
        (is (not (mouse-binding/command-active?
                   ::derived
                   :scene.camera.pan
                   {:mouse-buttons #{:primary} :modifiers #{:shift}})))
        (is (= {:kind :mouse-binding
                :context ::derived
                :command :scene.camera.pan
                :action ["Pan"]
                :context-path "Tile Map Editor"
                :binding-source :default
                :fallback-context-path nil
                :bindings [nil]}
               (mouse-binding/command-row {} ::derived :scene.camera.pan)))))))

(deftest modifier-command-workflow
  (with-mouse-bindings
    (mouse-binding/register! ::camera "Scene 3D Camera"
                             [{:command :scene.camera.free-look
                               :action ["Free Look"]
                               :binding {:button :secondary :modifiers []}}
                              {:command :scene.camera.free-look.speed-boost
                               :action ["Free Look" "Speed Boost"]
                               :modifier :shift}
                              {:command :scene.camera.free-look.speed-precision
                               :action ["Free Look" "Speed Precision"]
                               :modifier :alt}])
    (testing "registered modifier commands are active by default"
      (is (mouse-binding/command-active?
            ::camera
            :scene.camera.free-look.speed-boost
            {:modifiers #{:shift}}))
      (is (not
            (mouse-binding/command-active?
              ::camera
              :scene.camera.free-look.speed-boost
              {:modifiers #{:control}})))
      (is (= {:kind :mouse-modifier
              :context ::camera
              :command :scene.camera.free-look.speed-boost
              :action ["Free Look" "Speed Boost"]
              :context-path "Scene 3D Camera"
              :modifier :shift
              :default-modifier :shift}
             (mouse-binding/command-row {} ::camera :scene.camera.free-look.speed-boost))))
    (let [overrides-1 (mouse-binding/update-modifier-command
                        {}
                        ::camera
                        :scene.camera.free-look.speed-boost
                        :control)]
      (testing "modifier override is applied"
        (is (= {::camera
                {:scene.camera.free-look.speed-boost
                 {:modifier :control}}}
               overrides-1))
        (mouse-binding/set-user-overrides! overrides-1)
        (is (mouse-binding/command-active?
              ::camera
              :scene.camera.free-look.speed-boost
              {:modifiers #{:control}}))
        (is (not
              (mouse-binding/command-active?
                ::camera
                :scene.camera.free-look.speed-boost
                {:modifiers #{:shift}})))
        (is (= {:kind :mouse-modifier
                :context ::camera
                :command :scene.camera.free-look.speed-boost
                :action ["Free Look" "Speed Boost"]
                :context-path "Scene 3D Camera"
                :modifier :control
                :default-modifier :shift}
               (mouse-binding/command-row overrides-1 ::camera :scene.camera.free-look.speed-boost))))
      (let [overrides-2 (mouse-binding/reset-modifier-command
                          overrides-1
                          ::camera
                          :scene.camera.free-look.speed-boost)]
        (testing "reset restores the default modifier"
          (is (= {} overrides-2))
          (mouse-binding/set-user-overrides! overrides-2)
          (is (mouse-binding/command-active?
                ::camera
                :scene.camera.free-look.speed-boost
                {:modifiers #{:shift}}))
          (is (not
                (mouse-binding/command-active?
                  ::camera
                  :scene.camera.free-look.speed-boost
                  {:modifiers #{:control}}))))))))
