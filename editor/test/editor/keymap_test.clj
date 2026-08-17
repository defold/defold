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

(ns editor.keymap-test
  (:require [clojure.test :refer :all]
            [editor.keymap :as keymap]
            [util.coll :as coll])
  (:import [javafx.scene.input KeyCombination]))

(deftest default-bindings-are-valid-test
  (doseq [os [:macos :win32 :linux]
          [command shortcut->warnings] (keymap/warnings (keymap/default os))
          [shortcut warnings] shortcut->warnings
          warning warnings]
    (is (contains? #{:typable :conflict} (:type warning))
        (format "Unacceptable default shortcut %s for command %s (%s)" shortcut command (name (:type warning))))))

(deftest disallow-typable-shortcuts-test
  (is (= {:s {(KeyCombination/valueOf "S") #{{:type :typable}}}
          :t {(KeyCombination/valueOf "Alt+T") #{{:type :typable}}}
          :u {(KeyCombination/valueOf "Ctrl+Alt+U") #{{:type :typable}}}
          :x {(KeyCombination/valueOf "Shift+Alt+X") #{{:type :typable}}}}
         (keymap/warnings
           (keymap/from
             keymap/empty
             :macos
             {:s {:add #{"S"}}
              :t {:add #{"Alt+T"}}
              :u {:add #{"Ctrl+Alt+U"}}
              :x {:add #{"Shift+Alt+X"}}}))))
  (doseq [os [:linux :win32]]
    (is (= {:s {(KeyCombination/valueOf "S") #{{:type :typable}}}
            :u {(KeyCombination/valueOf "Ctrl+Alt+U") #{{:type :typable}}}}
           (keymap/warnings
             (keymap/from keymap/empty os {:s {:add #{"S"}}
                                           :u {:add #{"Ctrl+Alt+U"}}}))))))

(deftest keymap-does-not-allow-shortcut-key-test
  (doseq [os (keys keymap/platform->default-key-bindings)]
    (is (contains? (-> (keymap/from keymap/empty os {:a {:add #{"Shortcut+A"}}})
                       (keymap/warnings :a "Shortcut+A"))
                   {:type :shortcut-modifier}))))

(deftest keymap-editing-test
  (let [m1 (keymap/from keymap/empty {:a {:add #{"Meta+A"}}})]
    (is (coll/empty? (keymap/warnings m1)))
    (testing "add a conflicting shortcut"
      (let [m2 (keymap/from m1 {:b {:add #{"Meta+A"}}})]
        (is (= #{{:type :conflict :command :b}} (keymap/warnings m2 :a "Meta+A")))
        (is (= #{{:type :conflict :command :a}} (keymap/warnings m2 :b "Meta+A")))
        (is (= #{:a :b} (keymap/commands m2 "Meta+A")))
        (is (= #{(KeyCombination/valueOf "Meta+A")} (keymap/shortcuts m2 :a)))
        (is (= #{(KeyCombination/valueOf "Meta+A")} (keymap/shortcuts m2 :b)))))
    (testing "avoiding conflicts"
      (let [m2 (keymap/from m1 {:b {:add #{"Meta+A"}}
                                :a {:add #{"Ctrl+A"}
                                    :remove #{"Meta+A"}}})]
        (is (coll/empty? (keymap/warnings m2)))
        (is (= #{(KeyCombination/valueOf "Ctrl+A")} (keymap/shortcuts m2 :a)))
        (is (= #{(KeyCombination/valueOf "Meta+A")} (keymap/shortcuts m2 :b)))
        (is (= #{:a} (keymap/commands m2 "Ctrl+A")))
        (is (= #{:b} (keymap/commands m2 "Meta+A")))))
    (testing "removing a shortcut completely"
      (let [m2 (keymap/from m1 {:a {:remove #{"Meta+A"}}})]
        (is (nil? (keymap/commands m2 "Meta+A")))
        (is (nil? (keymap/shortcuts m2 :a)))
        (is (nil? (keymap/warnings m2 :a)))))))
