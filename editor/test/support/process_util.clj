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

(ns support.process-util
  (:require [clojure.java.io :as io]
            [clojure.string :as string]
            [editor.os :as os]
            [editor.process :as process]
            [util.coll :as coll]
            [util.task :as task])
  (:import [java.io File]
           [java.lang AutoCloseable]
           [java.util.concurrent Callable Executors TimeUnit TimeoutException]
           [java.util.regex Pattern]))

(set! *warn-on-reflection* true)

(defmacro child-process [operation options form arguments]
  ;; ProcessBuilder's Windows legacy quoting strips embedded quotes, so
  ;; pass the child form through the environment instead of the command.
  `(apply ~operation
          (assoc-in ~options [:env "DEFOLD_PROCESS_TEST_FORM"]
                    (pr-str (list 'do ~form '(flush) '(System/exit 0))))
          (.getAbsolutePath (io/file (System/getProperty "java.home") "bin" (if (os/is-win32?) "java.exe" "java")))
          "--enable-native-access=ALL-UNNAMED"
          "--sun-misc-unsafe-memory-access=allow"
          "-Dfile.encoding=UTF-8"
          "-Dstdout.encoding=UTF-8"
          "-Dstderr.encoding=UTF-8"
          ;; Keep the JVM from changing the error mode under test.
          "-Dsun.io.allowCriticalErrorMessageBox=true"
          "-cp" (coll/join-to-string
                  File/pathSeparator
                  (eduction
                    (map #(.getAbsolutePath (io/file %)))
                    (string/split (System/getProperty "java.class.path")
                                  (re-pattern (Pattern/quote File/pathSeparator)))))
          "clojure.main" "-e"
          "(load-string (System/getenv (name :DEFOLD_PROCESS_TEST_FORM)))"
          "-" ~arguments))

(defmacro with-child-process [options form arguments & body]
  `(task/with-open [executor# (Executors/newVirtualThreadPerTaskExecutor)]
     (let [~(with-meta 'process {:tag 'Process}) (child-process process/start! ~options ~form ~arguments)
           ^Callable interaction-fn# (bound-fn interact-with-child# []
                                       (with-open [stdin# (process/in ~'process)
                                                   stdout# (process/out ~'process)
                                                   stderr# (process/err ~'process)]
                                         ~@body))
           interaction# (.submit executor# interaction-fn#)]
       ;; Close in reverse order: destroy, cancel, await child, join worker.
       (task/with-open [await-child# (reify AutoCloseable
                                       (close [_#]
                                         (when-not (.waitFor ~'process 10 TimeUnit/SECONDS)
                                           (throw (TimeoutException.
                                                    (format "Child process %s did not terminate within 10 seconds after forced destruction."
                                                            (.pid ~'process)))))))
                        cancel-interaction# (reify AutoCloseable
                                              (close [_#]
                                                (.cancel interaction# true)))
                        destroy-child# (reify AutoCloseable
                                         (close [_#]
                                           (.destroyForcibly ~'process)))]
         (try
           (.get interaction# 60 TimeUnit/SECONDS)
           (catch TimeoutException cause#
             (throw (doto (TimeoutException.
                            (format "Child process %s interaction timed out after 60 seconds."
                                    (.pid ~'process)))
                      (.initCause cause#)))))))))
