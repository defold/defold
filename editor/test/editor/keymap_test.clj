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
  (:require [cljfx.api :as fx]
            [clojure.test :refer :all]
            [editor.keymap :as keymap]
            [util.coll :as coll])
  (:import [javafx.event EventHandler]
           [javafx.scene Scene]
           [javafx.scene.input KeyCode KeyCombination KeyEvent]
           [javafx.scene.layout Pane]))

(defn- key-event
  [event-type character key-code & {:keys [shift control alt meta]}]
  (KeyEvent. event-type
             character
             (if (= KeyEvent/KEY_TYPED event-type) "" character)
             key-code
             (boolean shift)
             (boolean control)
             (boolean alt)
             (boolean meta)))

(defn- fire-key-event! [target event]
  (.fireEvent target event)
  event)

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

(deftest installed-keymap-suppresses-typed-shortcut-characters-test
  @(fx/on-fx-thread
     (let [root (Pane.)
           scene (Scene. root)
           executed (atom [])
           typed (atom [])
           keymap (keymap/from keymap/empty :macos {:format {:add #{"Shift+Alt+F"}}})]
       (.addEventHandler root KeyEvent/KEY_TYPED
                         (reify EventHandler
                           (handle [_ event]
                             (swap! typed conj (.getCharacter ^KeyEvent event)))))
       (keymap/install! keymap scene #(swap! executed conj %) :macos)

       (fire-key-event! root (key-event KeyEvent/KEY_PRESSED KeyEvent/CHAR_UNDEFINED KeyCode/F
                                        :shift true :alt true))
       (fire-key-event! root (key-event KeyEvent/KEY_TYPED "Ï" KeyCode/UNDEFINED
                                        :shift true :alt true))
       (fire-key-event! root (key-event KeyEvent/KEY_TYPED "unexpected-second-event" KeyCode/UNDEFINED
                                        :shift true :alt true))
       (is (= [#{:format}] @executed))
       (is (coll/empty? @typed))

       (fire-key-event! root (key-event KeyEvent/KEY_PRESSED KeyEvent/CHAR_UNDEFINED KeyCode/X))
       (fire-key-event! root (key-event KeyEvent/KEY_TYPED "x" KeyCode/UNDEFINED))
       (is (= ["x"] @typed)))))

(deftest installed-keymap-does-not-suppress-non-typable-shortcut-characters-test
  @(fx/on-fx-thread
     (let [root (Pane.)
           scene (Scene. root)
           typed (atom [])
           keymap (keymap/from keymap/empty :macos {:find {:add #{"Meta+F"}}})]
       (.addEventHandler root KeyEvent/KEY_TYPED
                         (reify EventHandler
                           (handle [_ event]
                             (swap! typed conj (.getCharacter ^KeyEvent event)))))
       (keymap/install! keymap scene (constantly nil) :macos)
       (fire-key-event! root (key-event KeyEvent/KEY_PRESSED KeyEvent/CHAR_UNDEFINED KeyCode/F :meta true))
       (fire-key-event! root (key-event KeyEvent/KEY_TYPED "f" KeyCode/UNDEFINED :meta true))
       (is (= ["f"] @typed)))))

(deftest installed-keymap-does-not-suppress-inactive-shortcut-characters-test
  @(fx/on-fx-thread
     (let [root (Pane.)
           scene (Scene. root)
           typed (atom [])
           keymap (keymap/from keymap/empty :macos {:format {:add #{"Shift+Alt+F"}}})]
       (.addEventHandler root KeyEvent/KEY_TYPED
                         (reify EventHandler
                           (handle [_ event]
                             (swap! typed conj (.getCharacter ^KeyEvent event)))))
       (keymap/install! keymap scene (constantly false) :macos)
       (fire-key-event! root (key-event KeyEvent/KEY_PRESSED KeyEvent/CHAR_UNDEFINED KeyCode/F
                                        :shift true :alt true))
       (fire-key-event! root (key-event KeyEvent/KEY_TYPED "Ï" KeyCode/UNDEFINED
                                        :shift true :alt true))
       (is (= ["Ï"] @typed)))))

(deftest installed-keymap-does-not-suppress-bare-shortcut-characters-test
  @(fx/on-fx-thread
     (let [root (Pane.)
           scene (Scene. root)
           executed (atom [])
           typed (atom [])
           keymap (keymap/from keymap/empty :macos {:d {:add #{"D"}}
                                                    :e {:add #{"E"}}
                                                    :r {:add #{"R"}}})]
       (.addEventHandler root KeyEvent/KEY_TYPED
                         (reify EventHandler
                           (handle [_ event]
                             (swap! typed conj (.getCharacter ^KeyEvent event)))))
       (keymap/install! keymap scene #(swap! executed conj %) :macos)

       (doseq [[code character] [[KeyCode/D "d"]
                                 [KeyCode/E "e"]
                                 [KeyCode/R "r"]]]
         (fire-key-event! root (key-event KeyEvent/KEY_PRESSED KeyEvent/CHAR_UNDEFINED code))
         (fire-key-event! root (key-event KeyEvent/KEY_TYPED character KeyCode/UNDEFINED)))

       (is (= [#{:d} #{:e} #{:r}] @executed))
       (is (= ["d" "e" "r"] @typed)))))

(deftest reinstall-keymap-removes-old-handlers-test
  @(fx/on-fx-thread
     (let [root (Pane.)
           scene (Scene. root)
           executed (atom [])
           first-keymap (keymap/from keymap/empty :macos {:first {:add #{"Meta+F"}}})
           second-keymap (keymap/from keymap/empty :macos {:second {:add #{"Meta+F"}}})]
       (keymap/install! first-keymap scene #(swap! executed conj %) :macos)
       (keymap/install! second-keymap scene #(swap! executed conj %) :macos)
       (fire-key-event! root (key-event KeyEvent/KEY_PRESSED KeyEvent/CHAR_UNDEFINED KeyCode/F :meta true))
       (is (= [#{:second}] @executed)))))
