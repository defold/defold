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

(ns preflight.test
  (:require  [clojure.test :as t]
             [clojure.java.io :as io]
             [clojure.tools.namespace.find :as ns-find])
  (:import [java.io ByteArrayOutputStream PrintStream PrintWriter]
           [java.nio.charset StandardCharsets]))

(defn- with-output-harness
  [f]
  ;; Have to redirect the standard Java streams as well as the Clojure vars
  ;; because some code will print directly to the Java streams.
  (let [orig-out ^PrintStream System/out
        orig-err ^PrintStream System/err
        baos (ByteArrayOutputStream.)
        out (PrintStream. baos true "UTF-8")
        outwriter (PrintWriter. out)]
    (try
      (System/setOut out)
      (try
        (System/setErr out)
        (binding [*ns* *ns* ;; Needed for a test to work
                  *out* outwriter
                  *err* outwriter
                  t/*test-out* outwriter]
          [(f)
           (String. (.toByteArray baos) StandardCharsets/UTF_8)])
        (finally
          (System/setErr orig-err)))
      (finally
        (System/setOut orig-out)))))

(defn run
  [_files]
  (let [[test-summary report]
        (with-output-harness
          (fn []
            (let [namespaces (ns-find/find-namespaces-in-dir (io/file "test"))]
              (doall (map #(require %) namespaces))
              (apply t/run-tests namespaces))))]
    {:counts {:incorrect (:fail test-summary) :error (:error test-summary)}
     :report report}))
