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

(ns util.text-util-test
  (:require [clojure.java.io :as io]
            [clojure.test :refer :all]
            [util.text-util :as text-util])
  (:import [org.apache.commons.io IOUtils]))

(deftest guess-line-endings-test
  (let [make-reader (fn [^String content]
                      (some-> content IOUtils/toInputStream (io/make-reader nil)))]
    (are [expected content]
      (= expected (text-util/guess-line-endings (make-reader content)))

      ; Indeterminate. Used for nil or binary-looking input.
      nil   nil
      nil   (apply str (map char [0x89 0x50 0x4E 0x47 0x0D 0x0A 0x1A 0x0A]))

      ; Empty or no line endings.
      nil   ""
      nil   "eol"

      ; Uniform line endings.
      :lf   "\n"
      :crlf "\r\n"
      :lf   "eol\n"
      :crlf "eol\r\n"
      :lf   (str "id: \"sprite\"\n"
                 "type: \"sprite\"\n"
                 "position: {\n"
                 "  x: 0.0\n"
                 "  y: 0.0\n"
                 "  z: 0.0\n"
                 "}\n")
      :crlf (str "id: \"sprite\"\r\n"
                 "type: \"sprite\"\r\n"
                 "position: {\r\n"
                 "  x: 0.0\r\n"
                 "  y: 0.0\r\n"
                 "  z: 0.0\r\n"
                 "}\r\n")

      ; Mixed line endings.
      :lf (str "\n"
               "\n"
               "\n"
               "\r\n"
               "\r\n"
               "\r\n"
               "\n")
      :crlf (str "\r\n"
                 "\r\n"
                 "\r\n"
                 "\n"
                 "\n"
                 "\n"
                 "\r\n"))))

(deftest guess-line-separator-test
  (are [expected content]
    (= expected (text-util/guess-line-separator content))

    ; Indeterminate. Used for nil or binary-looking input.
    "\n"   nil
    "\n"   (apply str (map char [0x89 0x50 0x4E 0x47 0x0D 0x0A 0x1A 0x0A]))

    ; Empty or no line endings.
    "\n"   ""
    "\n"   "eol"

    ; Uniform line endings.
    "\n"   "\n"
    "\r\n" "\r\n"
    "\n"   "eol\n"
    "\r\n" "eol\r\n"
    "\n"   (str "id: \"sprite\"\n"
                "type: \"sprite\"\n"
                "position: {\n"
                "  x: 0.0\n"
                "  y: 0.0\n"
                "  z: 0.0\n"
                "}\n")
    "\r\n" (str "id: \"sprite\"\r\n"
                "type: \"sprite\"\r\n"
                "position: {\r\n"
                "  x: 0.0\r\n"
                "  y: 0.0\r\n"
                "  z: 0.0\r\n"
                "}\r\n")

    ; Mixed line endings.
    "\n" (str "\n"
              "\n"
              "\n"
              "\r\n"
              "\r\n"
              "\r\n"
              "\n")
    "\r\n" (str "\r\n"
                "\r\n"
                "\r\n"
                "\n"
                "\n"
                "\n"
                "\r\n")))

(deftest scan-line-endings-test
  (let [make-reader (fn [^String content]
                      (some-> content IOUtils/toInputStream (io/make-reader nil)))]
    (are [expected content]
      (= expected (text-util/scan-line-endings (make-reader content)))

      ; Indeterminate. Used for nil or binary-looking input.
      nil   nil
      nil   (apply str (map char [0x89 0x50 0x4E 0x47 0x0D 0x0A 0x1A 0x0A]))

      ; Empty or no line endings.
      nil   ""
      nil   "eol"

      ; Uniform line endings.
      :lf   "\n"
      :crlf "\r\n"
      :lf   "eol\n"
      :crlf "eol\r\n"
      :lf   (str "id: \"sprite\"\n"
                 "type: \"sprite\"\n"
                 "position: {\n"
                 "  x: 0.0\n"
                 "  y: 0.0\n"
                 "  z: 0.0\n"
                 "}\n")
      :crlf (str "id: \"sprite\"\r\n"
                 "type: \"sprite\"\r\n"
                 "position: {\r\n"
                 "  x: 0.0\r\n"
                 "  y: 0.0\r\n"
                 "  z: 0.0\r\n"
                 "}\r\n")

      ; Mixed line endings.
      :mixed (str "\n"
                  "\n"
                  "\n"
                  "\r\n"
                  "\r\n"
                  "\r\n"
                  "\n")
      :mixed (str "\r\n"
                  "\r\n"
                  "\r\n"
                  "\n"
                  "\n"
                  "\n"
                  "\r\n"))))

(deftest crlf->lf-test
  (testing "Base cases"
    (are [expected content]
      (= expected (text-util/crlf->lf content))
      nil nil
      "" ""
      "\n" "\r\n"
      "\n\n" "\r\n\r\n"
      "\n" "\n"
      "eol" "eol"
      "eol\n" "eol\r\n"
      "eol\n" "eol\n"))

  (testing "Broken line endings"
    (are [expected content]
      (= expected (text-util/crlf->lf content))
      "" "\r"
      "" "\r\r"
      "\n" "\r\r\n"
      "\n" "\r\n\r"
      "\n" "\r\r\r\n"
      "\n" "\r\r\n\r"
      "\n\n" "\r\r\n\n"))

  (testing "Mixed line endings"
    (is (= (str "id: \"sprite\"\n"
                "type: \"sprite\"\n"
                "position: {\n"
                "  x: 0.0\n"
                "  y: 0.0\n"
                "  z: 0.0\n"
                "}\n")
           (text-util/crlf->lf (str "id: \"sprite\"\n"
                                    "type: \"sprite\"\n"
                                    "position: {\n"
                                    "  x: 0.0\r\n"
                                    "  y: 0.0\r\n"
                                    "  z: 0.0\r\n"
                                    "}\n"))))))

(deftest lf->crlf-test
  (testing "Base cases"
    (are [expected content]
      (= expected (text-util/lf->crlf content))
      nil nil
      "" ""
      "\r\n" "\n"
      "\r\n\r\n" "\n\n"
      "\r\n" "\r\n"
      "eol" "eol"
      "eol\r\n" "eol\n"
      "eol\r\n" "eol\r\n"))

  (testing "Broken line endings"
    (are [expected content]
      (= expected (text-util/lf->crlf content))
      "" "\r"
      "" "\r\r"
      "\r\n" "\r\r\n"
      "\r\n" "\r\n\r"
      "\r\n" "\r\r\r\n"
      "\r\n" "\r\r\n\r"
      "\r\n\r\n" "\r\r\n\n"))

  (testing "Mixed line endings"
    (is (= (str "id: \"sprite\"\r\n"
                "type: \"sprite\"\r\n"
                "position: {\r\n"
                "  x: 0.0\r\n"
                "  y: 0.0\r\n"
                "  z: 0.0\r\n"
                "}\r\n")
           (text-util/lf->crlf (str "id: \"sprite\"\r\n"
                                    "type: \"sprite\"\r\n"
                                    "position: {\r\n"
                                    "  x: 0.0\n"
                                    "  y: 0.0\n"
                                    "  z: 0.0\n"
                                    "}\r\n"))))))

(deftest parse-comma-separated-string-test
  (is (= []
         (text-util/parse-comma-separated-string "")))
  (is (= []
         (text-util/parse-comma-separated-string nil)))
  (is (= []
         (text-util/parse-comma-separated-string ",")))
  (is (= ["abc"]
         (text-util/parse-comma-separated-string "abc")))
  (is (= ["abc" "de"]
         (text-util/parse-comma-separated-string "abc,de")))
  (is (= ["abc" "de"]
         (text-util/parse-comma-separated-string "abc,   de")))
  (is (= ["abc"]
         (text-util/parse-comma-separated-string "\"abc\"")))
  (is (= ["234" "aaa, bbbb" "galaxy" "iPhone10,6"]
         (text-util/parse-comma-separated-string "234,\"aaa, bbbb\", galaxy, \"iPhone10,6\""))))

(deftest join-comma-separated-string-test
  (is (= ""
         (text-util/join-comma-separated-string [])))
  (is (= ""
         (text-util/join-comma-separated-string [""])))
  (is (= ""
         (text-util/join-comma-separated-string nil)))
  (is (= "\"a\""
         (text-util/join-comma-separated-string ["a"])))
  (is (= "\"aa\", \"bbb\""
         (text-util/join-comma-separated-string ["aa" "bbb"])))
  (is (= "\"234\", \"aaa, bbbb\", \"galaxy\", \"iPhone10,6\""
         (text-util/join-comma-separated-string ["234" "aaa, bbbb" "galaxy" "iPhone10,6"]))))

(deftest character-count-test
  (is (= 0 (text-util/character-count "" \a)))
  (is (= 0 (text-util/character-count "a" \b)))
  (is (= 2 (text-util/character-count "ababb" \a)))
  (is (= 3 (text-util/character-count "ababb" \b)))
  (is (= 2 (text-util/character-count (String. (int-array [0x100000 0x10FFFF 0x100000 0x10FFFF 0x10FFFF]) 0 5) 0x100000)))
  (is (= 3 (text-util/character-count (String. (int-array [0x100000 0x10FFFF 0x100000 0x10FFFF 0x10FFFF]) 0 5) 0x10FFFF))))

(deftest search-string-numeric?-test
  (testing "Returns true for patterns that might match a numeric value."
    (is (true? (text-util/search-string-numeric? ".")))
    (is (true? (text-util/search-string-numeric? "-")))
    (is (true? (text-util/search-string-numeric? "*")))
    (is (true? (text-util/search-string-numeric? "**")))
    (is (true? (text-util/search-string-numeric? "-*")))
    (is (true? (text-util/search-string-numeric? "5.")))
    (is (true? (text-util/search-string-numeric? ".5")))
    (is (true? (text-util/search-string-numeric? "*.")))
    (is (true? (text-util/search-string-numeric? ".*")))
    (is (true? (text-util/search-string-numeric? "*.*")))
    (is (true? (text-util/search-string-numeric? "-*.*")))
    (is (true? (text-util/search-string-numeric? "-50.*")))
    (is (true? (text-util/search-string-numeric? "-*.05")))
    (is (true? (text-util/search-string-numeric? "1234567890")))
    (is (true? (text-util/search-string-numeric? "1234.56789")))
    (is (true? (text-util/search-string-numeric? "-1234.56789")))
    (is (true? (text-util/search-string-numeric? "-*12*34*.*56*78*"))))

  (testing "Returns false for patterns that cannot match a numeric value."
    (is (false? (text-util/search-string-numeric? "")))
    (is (false? (text-util/search-string-numeric? "a")))
    (is (false? (text-util/search-string-numeric? "1a")))
    (is (false? (text-util/search-string-numeric? "a1")))
    (is (false? (text-util/search-string-numeric? "..")))
    (is (false? (text-util/search-string-numeric? "--")))
    (is (false? (text-util/search-string-numeric? "*-")))
    (is (false? (text-util/search-string-numeric? "*.*.*")))))

(defn- search-text [text search-string case-sensitivity]
  (let [pattern (text-util/search-string->re-pattern search-string case-sensitivity)
        matcher (re-matcher pattern text)]
    (when (.find matcher)
      (.group matcher))))

(deftest search-string->re-pattern-test
  (testing "Case-sensitivity patterns."
    (is (= "\\Qfoo\\E" (str (text-util/search-string->re-pattern "foo" :case-sensitive))))
    (is (= "(?i)\\Qfoo\\E" (str (text-util/search-string->re-pattern "foo" :case-insensitive)))))

  (testing "Match-anything wildcards are handled correctly."
    (is (= "(?i)\\Qfoo\\E.*\\Qbar\\E" (str (text-util/search-string->re-pattern "foo*bar" :case-insensitive)))))

  (testing "Other wildcard chars are quoted."
    (is (= "(?i)\\Qfoo\\E.*\\Qbar[]().$^\\E" (str (text-util/search-string->re-pattern "foo*bar[]().$^" :case-insensitive)))))

  (testing "Partial matches."
    (is (= "Partial" (search-text "Partial sentence" "Partial" :case-sensitive)))
    (is (= "Partial" (search-text "Partial sentence" "partial" :case-insensitive))))

  (testing "Quote behavior when matching against strings."
    (is (= "'quoted'" (search-text "'quoted'" "'quoted'" :case-sensitive)))
    (is (= "'quot" (search-text "'quoted'" "'quot" :case-sensitive)))
    (is (= "\"quoted\"" (search-text "\"quoted\"" "\"quoted\"" :case-sensitive)))
    (is (= "\"quot" (search-text "\"quoted\"" "\"quot" :case-sensitive)))
    (is (nil? (search-text "quoted" "'quoted'" :case-sensitive)))
    (is (nil? (search-text "quoted" "\"quoted\"" :case-sensitive))))

  (testing "Case-sensitivity when matching against strings."
    (let [case-sensitive-pattern (text-util/search-string->re-pattern "fOoO" :case-sensitive)
          case-insensitive-pattern (text-util/search-string->re-pattern "fOoO" :case-insensitive)]
      (is (= "fOoO" (re-matches case-sensitive-pattern "fOoO")))
      (is (nil? (re-matches case-sensitive-pattern "fooo")))
      (is (= "fOoO" (re-matches case-insensitive-pattern "fOoO")))
      (is (= "fooo" (re-matches case-insensitive-pattern "fooo"))))))

(deftest includes-re-pattern?-test
  (is (true? (text-util/includes-re-pattern? "exact" #"exact")))
  (is (true? (text-util/includes-re-pattern? "two words" #"word")))
  (is (false? (text-util/includes-re-pattern? "two words" #"missing"))))
