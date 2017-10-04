(ns editor.keymap-test
  (:require
   [clojure.test :as test :refer [deftest is are testing]]
   [editor.keymap :as keymap])
  (:import
   (javafx.scene.input KeyCombination)))

(def shortcut-keys {:control-down? "Ctrl"
                    :meta-down?    "Meta"})

(deftest default-bindings-are-valid
  (doseq [[shortcut-key _] shortcut-keys]
    (is (keymap/make-keymap keymap/default-key-bindings {:valid-command?        (constantly true)
                                                         :platform-shortcut-key shortcut-key
                                                         :throw-on-error?       true}))))

(deftest make-keymap-test
  (testing "canonicalizes shortcut keys correctly"
    (doseq [[shortcut-key _] shortcut-keys]
      (are [shortcut command expected-key expected-modifiers]
          (= [{:key           expected-key
               :alt-down?     (boolean (expected-modifiers :alt-down?))
               :control-down? (boolean (expected-modifiers :control-down?))
               :meta-down?    (boolean (expected-modifiers :meta-down?))
               :shift-down?   (boolean (expected-modifiers :shift-down?))}
              [{:command command
                :shortcut shortcut
                :key-combo (KeyCombination/keyCombination shortcut)}]]
             (first (keymap/make-keymap [[shortcut command]]
                                        {:platform-shortcut-key shortcut-key
                                         :throw-on-error? true
                                         :valid-command? (constantly true)})))

        "A"               :a "A" #{}
        "Shortcut+A"      :a "A" #{shortcut-key}
        "Ctrl+A"          :a "A" #{:control-down?}
        "Meta+A"          :a "A" #{:meta-down?}
        "Shift+A"         :a "A" #{:shift-down?}
        "Shortcut+Ctrl+A" :a "A" (hash-set :control-down? shortcut-key)
        "Shortcut+Meta+A" :a "A" (hash-set :meta-down? shortcut-key))))

  (testing "prefers shortcut key to the corresponding platform modifier key"
    (doseq [[shortcut-key shortcut-name] shortcut-keys]
      (let [km (keymap/make-keymap [["Shortcut+A" :shortcut]
                                    [(str shortcut-name "+A") :platform-modifier]]
                                   {:platform-shortcut-key shortcut-key})]
        (is (= 1 (count km)))
        (is (= :shortcut (-> km first val first :command)))))))
