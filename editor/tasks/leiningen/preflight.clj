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

(ns leiningen.preflight
  (:require [clojure.java.io :as io]
            [leiningen.core.eval :as lein-eval])
  (:import [java.io InputStreamReader]))

(defn preflight
  [project & _rest]
  (lein-eval/eval-in-project
    project
    '(do
       (load-file "tasks/leiningen/preflight/fmt.clj")
       (load-file "tasks/leiningen/preflight/kibit.clj")
       (load-file "tasks/leiningen/preflight/test.clj")
       (load-file "tasks/leiningen/preflight/core.clj")
       (in-ns 'preflight.core)
       (main)
       (shutdown-agents)
       (System/exit 0))))
