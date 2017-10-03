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
