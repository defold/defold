(ns editor.keymap-test
  (:require [clojure.test :refer [are deftest is testing]]
            [editor.keymap :as keymap]))

(deftest default-bindings-are-valid
  (doseq [[_ key-bindings] keymap/platform->default-key-bindings]
    (is (keymap/make-keymap key-bindings {:valid-command? (constantly true)
                                          :throw-on-error? true}))))

(defn- make-keymap-errors [key-bindings]
  (try
    (keymap/make-keymap key-bindings {:throw-on-error? true})
    (catch Exception ex (:errors (ex-data ex)))))

(deftest disallow-typable-shortcuts
  ;; We don't test keymap/allowed-typable-shortcuts
  (doseq [platform (keys keymap/platform->default-key-bindings)]
    (testing platform
      (let [errors (make-keymap-errors [["S" :s]
                                        ["Alt+T" :t]
                                        ["Ctrl+Alt+U" :u]
                                        ["Ctrl+Alt+V" :v]
                                        ["Shift+Alt+X" :x]])]
        (is (= errors #{{:type :typable-shortcut
                         :command :s
                         :shortcut "S"}
                        {:type :typable-shortcut
                         :command :t
                         :shortcut "Alt+T"}
                        {:type :typable-shortcut
                         :command :u
                         :shortcut "Ctrl+Alt+U"}
                        {:type :typable-shortcut
                         :command :v
                         :shortcut "Ctrl+Alt+V"}
                        {:type :typable-shortcut
                         :command :x
                         :shortcut "Shift+Alt+X"}}))))))

(deftest keymap-does-not-allow-shortcut-key
  (is (= (make-keymap-errors [["Shortcut+A" :a]])
         #{{:type :shortcut-key, :command :a, :shortcut "Shortcut+A"}})))
