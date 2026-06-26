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
  (:import [javafx.scene.input KeyCode KeyCombination]))

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

(deftest mouse-shortcut-parsing-test
  (testing "thumb buttons parse into mouse shortcuts"
    (is (keymap/mouse-shortcut? (keymap/parse-shortcut "MouseBack")))
    (is (keymap/mouse-shortcut? (keymap/parse-shortcut "MouseForward")))
    (is (keymap/mouse-shortcut? (keymap/parse-shortcut "Shift+Ctrl+Alt+Meta+MouseForward")))
    (is (not (keymap/mouse-shortcut? (keymap/parse-shortcut "Ctrl+A")))))
  (testing "round-trip through canonical name"
    (is (= "MouseBack" (str (keymap/parse-shortcut "MouseBack"))))
    (is (= "Ctrl+MouseBack" (str (keymap/parse-shortcut "Ctrl+MouseBack"))))
    (is (= "Shift+Ctrl+MouseForward" (str (keymap/parse-shortcut "ctrl+shift+mouseforward"))))
    (let [shortcut (keymap/parse-shortcut "Alt+MouseBack")]
      (is (= shortcut (keymap/parse-shortcut (str shortcut))))))
  (testing "non-thumb buttons and unsupported modifiers do not parse into mouse shortcuts"
    ;; Like other unknown key names, these fall back to KeyCombination parsing.
    (is (not (keymap/mouse-shortcut? (keymap/parse-shortcut "MouseMiddle"))))
    (is (not (keymap/mouse-shortcut? (keymap/parse-shortcut "Shortcut+MouseBack"))))
    (is (thrown? IllegalArgumentException (keymap/parse-shortcut "Ctrl+Ctrl+MouseBack")))))

(deftest mouse-shortcut-keymap-test
  (let [m1 (keymap/from keymap/empty {:a {:add #{"Ctrl+MouseBack"}}})]
    (is (coll/empty? (keymap/warnings m1)))
    (is (= #{:a} (keymap/commands m1 "Ctrl+MouseBack")))
    (is (= #{(keymap/parse-shortcut "Ctrl+MouseBack")} (keymap/shortcuts m1 :a)))
    (testing "conflicts are detected"
      (let [m2 (keymap/from m1 {:b {:add #{"Ctrl+MouseBack"}}})]
        (is (= #{{:type :conflict :command :b}} (keymap/warnings m2 :a "Ctrl+MouseBack")))
        (is (= #{{:type :conflict :command :a}} (keymap/warnings m2 :b "Ctrl+MouseBack")))))
    (testing "removing a mouse shortcut"
      (let [m2 (keymap/from m1 {:a {:remove #{"Ctrl+MouseBack"}}})]
        (is (nil? (keymap/commands m2 "Ctrl+MouseBack")))
        (is (nil? (keymap/shortcuts m2 :a)))))
    (testing "non-canonical names are flagged"
      (let [m2 (keymap/from keymap/empty {:c {:add #{"mouseback"}}})]
        (is (= #{{:type :non-canonical-name}} (keymap/warnings m2 :c "MouseBack")))))
    (testing "mouse shortcuts are never typable"
      (doseq [os [:macos :win32 :linux]]
        (is (not (keymap/typable? (keymap/parse-shortcut "MouseBack") os)))))))

(deftest mouse-shortcut-text-test
  (let [shortcut (keymap/parse-shortcut "Ctrl+MouseBack")]
    (is (= "⌃Mouse Back" (keymap/shortcut-distinct-display-text shortcut :macos)))
    (is (= "Ctrl+Mouse Back" (keymap/shortcut-distinct-display-text shortcut :win32)))
    (is (= "Ctrl+Mouse Back" (keymap/shortcut-distinct-display-text shortcut :linux)))
    (is (= "Ctrl+Mouse Back" (keymap/shortcut-filterable-text shortcut :macos))))
  (let [shortcut (keymap/parse-shortcut "Meta+MouseForward")]
    (is (= "⌘Mouse Forward" (keymap/shortcut-distinct-display-text shortcut :macos)))
    (is (= "Cmd+Mouse Forward" (keymap/shortcut-filterable-text shortcut :macos)))
    (is (= "Win+Mouse Forward" (keymap/shortcut-filterable-text shortcut :win32)))
    (is (= "Meta+Mouse Forward" (keymap/shortcut-filterable-text shortcut :linux)))))

(deftest mouse-shortcut-key-codes-test
  (let [m (keymap/from keymap/empty {:a {:add #{"MouseBack" "Ctrl+A"}}})]
    (is (= #{KeyCode/A} (keymap/shortcut-key-codes m (keymap/shortcuts m :a))))))

(deftest display-text-prefers-key-combinations-test
  (let [m (keymap/from keymap/empty {:a {:add #{"MouseBack" "Ctrl+A"}}})]
    (is (= (.getDisplayText (KeyCombination/valueOf "Ctrl+A"))
           (keymap/display-text m :a nil))))
  (let [m (keymap/from keymap/empty {:a {:add #{"MouseBack"}}})]
    (is (= "Mouse Back" (keymap/display-text m :a nil)))))
