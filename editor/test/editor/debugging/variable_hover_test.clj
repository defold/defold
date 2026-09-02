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

(ns editor.debugging.variable-hover-test
  (:require [clojure.string :as string]
            [clojure.test :refer :all]
            [editor.code.data :as data :refer [->Cursor ->CursorRange]]
            [editor.debugging.variable-hover :as variable-hover]))

(defn- cr [[from-row from-col] [to-row to-col]]
  (->CursorRange (->Cursor from-row from-col) (->Cursor to-row to-col)))

;; -----------------------------------------------------------------------------
;; resolve-value
;; -----------------------------------------------------------------------------

(deftest resolve-value-test
  (testing "Local lookup"
    (is (= {:value 100}
           (variable-hover/resolve-value {:locals {"health" 100} :upvalues {}} "health"))))

  (testing "Upvalue fallback"
    (is (= {:value "hi"}
           (variable-hover/resolve-value {:locals {} :upvalues {"outer" "hi"}} "outer"))))

  (testing "Locals shadow upvalues"
    (is (= {:value "local-val"}
           (variable-hover/resolve-value {:locals {"x" "local-val"} :upvalues {"x" "upvalue-val"}} "x"))))

  (testing "Unknown name"
    (is (nil? (variable-hover/resolve-value {:locals {} :upvalues {}} "unknown"))))

  (testing "Nil-valued local is distinct from a missing local"
    (let [suspension-variables {:locals {"x" nil} :upvalues {}}]
      (is (= {:value nil} (variable-hover/resolve-value suspension-variables "x"))
          "existing key mapped to nil")
      (is (nil? (variable-hover/resolve-value suspension-variables "y"))
          "key never present")))

  (testing "Dotted chain through nested values"
    (let [suspension-variables {:locals {"self" {"weapon_popup" {"scale_default" 1}}} :upvalues {}}]
      (is (= {:value 1}
             (variable-hover/resolve-value suspension-variables "self.weapon_popup.scale_default")))
      (is (nil? (variable-hover/resolve-value suspension-variables "self.missing"))
          "last segment missing")))

  (testing "Chain stops at a nil intermediate"
    (let [suspension-variables {:locals {"self" {"weapon_popup" nil}} :upvalues {}}]
      (is (nil? (variable-hover/resolve-value suspension-variables "self.weapon_popup.scale_default"))))))

;; -----------------------------------------------------------------------------
;; render-value
;; -----------------------------------------------------------------------------

(deftest render-value-test
  (testing "Number value"
    (is (= "health = 100" (variable-hover/render-value "health" 100))))

  (testing "String value is quoted"
    (is (= "name = \"hi\"" (variable-hover/render-value "name" "hi"))))

  (testing "Nil value"
    (is (= "x = nil" (variable-hover/render-value "x" nil))))

  (testing "Table value"
    (is (= "self = {\"weapon_popup\" {\"scale_default\" 1}}"
           (variable-hover/render-value "self" {"weapon_popup" {"scale_default" 1}})))))

;; -----------------------------------------------------------------------------
;; truncate-hover-text
;; -----------------------------------------------------------------------------

(deftest truncate-hover-text-test
  (testing "Empty string"
    (is (= "" (variable-hover/truncate-hover-text ""))))

  (testing "Within both caps"
    (is (= "a\nb\nc" (variable-hover/truncate-hover-text "a\nb\nc"))))

  (testing "At the line cap"
    (let [s (string/join "\n" (repeat 40 "x"))]
      (is (= s (variable-hover/truncate-hover-text s)))))

  (testing "Past the line cap"
    (let [s (string/join "\n" (repeat 41 "x"))
          expected (str (string/join "\n" (repeat 40 "x")) "\n...")]
      (is (= expected (variable-hover/truncate-hover-text s)))))

  (testing "At the line-length cap"
    (let [line (apply str (repeat 200 "a"))]
      (is (= line (variable-hover/truncate-hover-text line)))))

  (testing "Past the line-length cap"
    (let [line (apply str (repeat 201 "a"))
          expected (str (apply str (repeat 200 "a")) "...")]
      (is (= expected (variable-hover/truncate-hover-text line)))))

  (testing "Both caps together"
    (let [long-line (apply str (repeat 250 "y"))
          lines (assoc (vec (repeat 44 "short")) 10 long-line)
          s (string/join "\n" lines)
          truncated-long-line (str (apply str (repeat 200 "y")) "...")
          expected (str (string/join "\n" (assoc (vec (repeat 40 "short")) 10 truncated-long-line))
                        "\n...")]
      (is (= expected (variable-hover/truncate-hover-text s))))))

;; -----------------------------------------------------------------------------
;; variable-hover-region
;; -----------------------------------------------------------------------------

(def ^:private proj-path "/main/player.script")

(def ^:private suspension-variables
  {:file proj-path
   :locals {"health" 100
            "self" {"weapon_popup" {"scale_default" 1}}}
   :upvalues {"outer" "hi"}})

(deftest variable-hover-region-test
  (testing "Not suspended"
    (is (nil? (variable-hover/variable-hover-region nil proj-path (data/identifier-expression-at-cursor ["health"] (->Cursor 0 0))))))

  (testing "Different file"
    (is (nil? (variable-hover/variable-hover-region suspension-variables "/main/other.script" (data/identifier-expression-at-cursor ["health"] (->Cursor 0 0))))))

  (testing "No identifier at cursor"
    (is (nil? (variable-hover/variable-hover-region suspension-variables proj-path (data/identifier-expression-at-cursor ["  "] (->Cursor 0 0))))))

  (testing "Unresolved identifier"
    (is (nil? (variable-hover/variable-hover-region suspension-variables proj-path (data/identifier-expression-at-cursor ["unknown_var"] (->Cursor 0 0))))))

  (testing "Region shape"
    (is (= (data/map->CursorRange
             {:from (->Cursor 0 0)
              :to (->Cursor 0 6)
              :type :debug-variable
              :hoverable true
              :expr-text "health"
              :value 100
              :content {:type :plaintext :value "health = 100"}})
           (variable-hover/variable-hover-region suspension-variables proj-path (data/identifier-expression-at-cursor ["health"] (->Cursor 0 0))))))

  (testing "Nil-valued local produces a region"
    (let [suspension-variables (assoc-in suspension-variables [:locals "nil_var"] nil)]
      (is (= "nil_var = nil"
             (get-in (variable-hover/variable-hover-region suspension-variables proj-path (data/identifier-expression-at-cursor ["nil_var"] (->Cursor 0 0)))
                     [:content :value])))))

  (testing "Dotted chain region"
    (let [line "self.weapon_popup.scale_default"]
      (is (= (data/map->CursorRange
               {:from (->Cursor 0 0)
                :to (->Cursor 0 31)
                :type :debug-variable
                :hoverable true
                :expr-text line
                :value 1
                :content {:type :plaintext :value "self.weapon_popup.scale_default = 1"}})
             (variable-hover/variable-hover-region suspension-variables proj-path (data/identifier-expression-at-cursor [line] (->Cursor 0 20))))))))
