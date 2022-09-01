;; Copyright 2020-2022 The Defold Foundation
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
  (:require [clojure.test :refer [are deftest is testing]]
            [editor.keymap :as keymap]))

(deftest default-bindings-are-valid
  (doseq [[platform key-bindings] keymap/platform->default-key-bindings]
    (is (keymap/make-keymap key-bindings {:valid-command? (constantly true)
                                          :platform platform
                                          :throw-on-error? true}))))

(defn- make-keymap-errors [key-bindings platform]
  (try
    (keymap/make-keymap key-bindings {:throw-on-error? true :platform platform})
    (catch Exception ex (:errors (ex-data ex)))))

(deftest disallow-typable-shortcuts
  ;; We don't test keymap/allowed-typable-shortcuts
  (is (= (make-keymap-errors [["S" :s]
                              ["Alt+T" :t]
                              ["Ctrl+Alt+U" :u]
                              ["Shift+Alt+X" :x]]
                             :macos)
         #{{:type :typable-shortcut :command :s :shortcut "S"}
           {:type :typable-shortcut :command :t :shortcut "Alt+T"}
           {:type :typable-shortcut :command :u :shortcut "Ctrl+Alt+U"}
           {:type :typable-shortcut :command :x :shortcut "Shift+Alt+X"}}))
  (doseq [platform [:linux :win32]]
    (is (= (make-keymap-errors [["S" :s] ["Ctrl+Alt+U" :u]] platform)
           #{{:type :typable-shortcut, :command :u, :shortcut "Ctrl+Alt+U"}
             {:type :typable-shortcut, :command :s, :shortcut "S"}}))))

(deftest keymap-does-not-allow-shortcut-key
  (doseq [platform (keys keymap/platform->default-key-bindings)]
    (is (= (make-keymap-errors [["Shortcut+A" :a]] platform)
           #{{:type :shortcut-key :command :a :shortcut "Shortcut+A"}}))))
