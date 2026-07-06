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
                               :binding {:button :primary :modifiers #{:shift}}}])
    (testing "registered default is active"
      (is (= :scene.camera.pan
             (mouse-binding/command-for-action ::camera {:button :primary :modifiers #{:shift}})))
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
              :inherited-context-path nil
              :bindings [{:button :primary :modifiers #{:shift}}]}
             (mouse-binding/resolve-command-binding {} ::camera :scene.camera.pan))))
    (let [overrides-1 (mouse-binding/update-command
                        {}
                        ::camera
                        :scene.camera.pan
                        [{:button :secondary :modifiers #{:control}}])]
      (testing "single custom override replaces default binding"
        (is (= {::camera
                {:scene.camera.pan
                 {:bindings [{:button :secondary :modifiers #{:control}}]}}}
               overrides-1))
        (mouse-binding/set-user-overrides! overrides-1)
        (is (= :scene.camera.pan
               (mouse-binding/command-for-action ::camera {:button :secondary :modifiers #{:control}})))
        (is (nil?
              (mouse-binding/command-for-action ::camera {:button :primary :modifiers #{:shift}})))
        (is (= {:kind :mouse-binding
                :context ::camera
                :command :scene.camera.pan
                :action ["Pan"]
                :context-path "Scene 2D Camera"
                :binding-source :custom
                :inherited-context-path nil
                :bindings [{:button :secondary :modifiers #{:control}}]}
               (mouse-binding/resolve-command-binding overrides-1 ::camera :scene.camera.pan))))
      (let [overrides-2 (mouse-binding/update-command
                          overrides-1
                          ::camera
                          :scene.camera.pan
                          [{:button :secondary :modifiers #{:control}}
                           {:button :middle :modifiers #{:alt}}])]
        (testing "appended custom bindings are effective"
          (mouse-binding/set-user-overrides! overrides-2)
          (is (= :scene.camera.pan
                 (mouse-binding/command-for-action ::camera {:button :middle :modifiers #{:alt}})))
          (is (= [{:command :scene.camera.pan
                   :action ["Pan"]
                   :context ::camera
                   :context-path "Scene 2D Camera"
                   :bindings [{:button :secondary :modifiers #{:control}}
                              {:button :middle :modifiers #{:alt}}]}]
                 (mouse-binding/all-bindings))))
        (let [overrides-3 (mouse-binding/remove-command-binding overrides-2 ::camera :scene.camera.pan 0)]
          (testing "removing one binding keeps the remaining custom binding"
            (is (= {::camera
                    {:scene.camera.pan
                     {:bindings [{:button :middle :modifiers #{:alt}}]}}}
                   overrides-3))
            (mouse-binding/set-user-overrides! overrides-3)
            (is (= :scene.camera.pan
                   (mouse-binding/command-for-action ::camera {:button :middle :modifiers #{:alt}}))))
          (let [overrides-4 (mouse-binding/remove-command-binding overrides-3 ::camera :scene.camera.pan 0)]
            (testing "removing the final binding preserves an explicit empty override"
              (is (= {::camera
                      {:scene.camera.pan
                       {:bindings []}}}
                     overrides-4))
              (mouse-binding/set-user-overrides! overrides-4)
              (is (nil?
                    (mouse-binding/command-for-action ::camera {:button :middle :modifiers #{:alt}})))
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
                      :inherited-context-path nil
                      :bindings []}
                     (mouse-binding/resolve-command-binding overrides-4 ::camera :scene.camera.pan)))
              (is (= [{:command :scene.camera.pan
                       :action ["Pan"]
                       :context ::camera
                       :context-path "Scene 2D Camera"
                       :bindings []}]
                     (mouse-binding/all-bindings))))
            (let [overrides-5 (mouse-binding/reset-command overrides-4 ::camera :scene.camera.pan)]
              (testing "reset removes the override and restores defaults"
                (is (= {} overrides-5))
                (mouse-binding/set-user-overrides! overrides-5)
                (is (= :scene.camera.pan
                       (mouse-binding/command-for-action ::camera {:button :primary :modifiers #{:shift}})))
                (is (= {:kind :mouse-binding
                        :context ::camera
                        :command :scene.camera.pan
                        :action ["Pan"]
                        :context-path "Scene 2D Camera"
                        :binding-source :default
                        :inherited-context-path nil
                        :bindings [{:button :primary :modifiers #{:shift}}]}
                       (mouse-binding/resolve-command-binding overrides-5 ::camera :scene.camera.pan)))))))))))

(deftest inherited-context-workflow
  (with-mouse-bindings
    (mouse-binding/register! ::base "Scene 2D Camera"
                             [{:command :scene.camera.pan
                               :action ["Pan"]
                               :binding {:button :primary :modifiers #{:shift}}}])
    (mouse-binding/register! ::derived "Tile Map Editor"
                             [{:command :scene.camera.pan
                               :action ["Pan"]}]
                             {:inherited-context ::base})
    (testing "prefs UI shows inheritance, but runtime does not resolve inherited commands"
      (is (= ::base (mouse-binding/inherited-context ::derived)))
      ;; Runtime resolution never traverses the prefs inheritance: an unbound command
      ;; in the derived context resolves to nothing, leaving it for the camera to
      ;; resolve against its live projection.
      (is (nil? (mouse-binding/command-for-action ::derived {:button :primary :modifiers #{:shift}})))
      (is (not (mouse-binding/command-active?
                 ::derived
                 :scene.camera.pan
                 {:mouse-buttons #{:primary} :modifiers #{:shift}})))
      ;; The prefs UI still displays the inherited binding.
      (is (= {:kind :mouse-binding
              :context ::derived
              :command :scene.camera.pan
              :action ["Pan"]
              :context-path "Tile Map Editor"
              :binding-source :inherited
              :inherited-context-path "Scene 2D Camera"
              :bindings [{:button :primary :modifiers #{:shift}}]}
             (mouse-binding/resolve-command-binding {} ::derived :scene.camera.pan))))
    (let [overrides-1 (mouse-binding/update-command
                        {}
                        ::derived
                        :scene.camera.pan
                        [{:button :secondary :modifiers #{}}])]
      (testing "a derived override resolves at the derived context (no inheritance needed)"
        (mouse-binding/set-user-overrides! overrides-1)
        (is (= :scene.camera.pan
               (mouse-binding/command-for-action ::derived {:button :secondary :modifiers #{}})))
        (is (= {:kind :mouse-binding
                :context ::derived
                :command :scene.camera.pan
                :action ["Pan"]
                :context-path "Tile Map Editor"
                :binding-source :custom
                :inherited-context-path nil
                :bindings [{:button :secondary :modifiers #{}}]}
               (mouse-binding/resolve-command-binding overrides-1 ::derived :scene.camera.pan))))
      (let [overrides-2 (mouse-binding/reset-command overrides-1 ::derived :scene.camera.pan)]
        (testing "reset returns the row to display-inherited behavior"
          (is (= {} overrides-2))
          (mouse-binding/set-user-overrides! overrides-2)
          ;; Runtime no longer resolves the inherited command...
          (is (nil? (mouse-binding/command-for-action ::derived {:button :primary :modifiers #{:shift}})))
          ;; ...while the prefs UI shows it as inherited again.
          (is (= {:kind :mouse-binding
                  :context ::derived
                  :command :scene.camera.pan
                  :action ["Pan"]
                  :context-path "Tile Map Editor"
                  :binding-source :inherited
                  :inherited-context-path "Scene 2D Camera"
                  :bindings [{:button :primary :modifiers #{:shift}}]}
                 (mouse-binding/resolve-command-binding overrides-2 ::derived :scene.camera.pan)))))
      (let [overrides-3 (mouse-binding/update-command
                          {}
                          ::base
                          :scene.camera.pan
                          [])]
        (testing "inherited rows show an empty binding list when the inherited-from bindings are explicitly cleared"
          (is (= {:kind :mouse-binding
                  :context ::derived
                  :command :scene.camera.pan
                  :action ["Pan"]
                  :context-path "Tile Map Editor"
                  :binding-source :default
                  :inherited-context-path nil
                  :bindings []}
                 (mouse-binding/resolve-command-binding overrides-3 ::derived :scene.camera.pan)))))
      (let [overrides-4 (mouse-binding/update-command-binding
                          {}
                          ::derived
                          :scene.camera.pan
                          nil
                          {:button :secondary :modifiers #{}})
            overrides-5 (mouse-binding/update-command-binding
                          overrides-4
                          ::derived
                          :scene.camera.pan
                          0
                          {:modifiers #{}})]
        (testing "clearing the last custom binding on an inherited-only row resets to display-inherited behavior"
          (is (= {::derived
                  {:scene.camera.pan
                   {:bindings [{:button :secondary :modifiers #{}}]}}}
                 overrides-4))
          (is (= {} overrides-5))
          (is (= {:kind :mouse-binding
                  :context ::derived
                  :command :scene.camera.pan
                  :action ["Pan"]
                  :context-path "Tile Map Editor"
                  :binding-source :inherited
                  :inherited-context-path "Scene 2D Camera"
                  :bindings [{:button :primary :modifiers #{:shift}}]}
                 (mouse-binding/resolve-command-binding overrides-5 ::derived :scene.camera.pan)))))
      (mouse-binding/register! ::derived "Tile Map Editor"
                               [{:command :scene.camera.pan
                                 :action ["Pan"]}])
      (testing "re-registering without :inherited-context clears the display inheritance"
        (is (nil? (mouse-binding/inherited-context ::derived)))
        (is (nil? (mouse-binding/command-for-action ::derived {:button :primary :modifiers #{:shift}})))
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
                :inherited-context-path nil
                :bindings []}
               (mouse-binding/resolve-command-binding {} ::derived :scene.camera.pan)))))))

(deftest modifier-command-workflow
  (with-mouse-bindings
    (mouse-binding/register! ::camera "Scene 3D Camera"
                             [{:command :scene.camera.free-look
                               :action ["Free Look"]
                               :binding {:button :secondary :modifiers #{}}}
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
             (mouse-binding/resolve-command-binding {} ::camera :scene.camera.free-look.speed-boost))))
    (let [overrides-1 (mouse-binding/update-command
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
               (mouse-binding/resolve-command-binding overrides-1 ::camera :scene.camera.free-look.speed-boost))))
      (let [overrides-2 (mouse-binding/reset-command
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

(deftest binding-edit-workflow
  (with-mouse-bindings
    (mouse-binding/register! ::camera "Scene 2D Camera"
                             [{:command :scene.camera.pan
                               :action ["Pan"]
                               :binding {:button :primary :modifiers #{:shift}}}])
    (testing "adding a binding through the edit workflow appends to editable bindings"
      (is (= {::camera
              {:scene.camera.pan
               {:bindings [{:button :primary :modifiers #{:shift}}
                           {:button :secondary :modifiers #{:control}}]}}}
             (mouse-binding/update-command-binding
               {}
               ::camera
               :scene.camera.pan
               nil
               {:button :secondary :modifiers #{:control}}))))
    (testing "duplicate binding edits are ignored"
      (is (= {}
             (mouse-binding/update-command-binding
               {}
               ::camera
               :scene.camera.pan
               nil
               {:button :primary :modifiers #{:shift}}))))
    (testing "editing a binding can remove it by clearing the button"
      (is (= {::camera
              {:scene.camera.pan
               {:bindings []}}}
             (mouse-binding/update-command-binding
               {::camera
                {:scene.camera.pan
                 {:bindings [{:button :primary :modifiers #{:shift}}]}}}
               ::camera
               :scene.camera.pan
               0
               {:modifiers #{:shift}}))))))
