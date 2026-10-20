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

(ns editor.process-test
  (:require [clojure.java.io :as io]
            [clojure.string :as string]
            [clojure.test :refer :all]
            [editor.fs :as fs]
            [editor.future :as future]
            [editor.os :as os]
            [editor.process :as process]
            [support.process-util :as process-util]
            [util.coll :as coll])
  (:import [com.sun.jna Function NativeLibrary]
           [java.io BufferedReader ByteArrayOutputStream IOException]
           [java.lang ProcessBuilder$Redirect]
           [java.nio.charset StandardCharsets]
           [java.util.concurrent TimeUnit]))

(set! *warn-on-reflection* true)

;; Starting a child must expose its output and successful exit on every host.
(deftest start-test
  (process-util/with-child-process {} '(print "output") []
    (is (= "output" (process/capture! (process/out process))))
    (is (zero? (process/await-exit-code process)))))

;; exec! must return a successful child's output without requiring shell tools.
(deftest exec-test
  (is (= "output"
         (process-util/child-process process/exec! {} '(print "output") []))))

;; Environment additions must reach the child on every host.
(deftest env-test
  (is (= "UHH"
         (process-util/child-process process/exec!
                                     {:env {"CUSTOM_VAR" "UHH"}}
                                     '(print (System/getenv "CUSTOM_VAR")) []))))

;; Removing an inherited variable must affect the child and expose a failed
;; exit.
(deftest remove-env-test
  (is (some? (process-util/child-process process/exec!
                                         {} '(print (System/getenv "PATH")) [])))
  (is (thrown? Exception
               (process-util/child-process process/exec!
                                           {:env {"PATH" nil}}
                                           '(System/exit (if (System/getenv "PATH") 0 1)) []))))

;; stderr redirection must discard or merge the actual child's output.
(deftest redirect-test
  (doseq [[redirection expected] [[:discard nil] [:stdout "stderr"]]]
    (is (= expected
           (process-util/child-process process/exec!
                                       {:err redirection}
                                       '(binding [*out* *err*] (print "stderr") (flush)) [])))))

;; Piping must copy the child's output through EOF without a shell dependency.
(deftest pipe-test
  (process-util/with-child-process {} '(print "output") []
    (let [target (ByteArrayOutputStream.)]
      (process/pipe! (process/out process) target)
      (is (= "output" (.toString target StandardCharsets/UTF_8))))))

;; Register before releasing a ready child so exit notification needs no sleeps.
(deftest on-exit-test
  (process-util/with-child-process {}
    '(do (println "ready") (flush) (.read System/in)) []
    (with-open [reader (io/reader (process/out process) :encoding "UTF-8")]
      (is (= "ready" (.readLine ^BufferedReader reader))))
    (let [done (promise)]
      (process/on-exit! process (fn on-exit [] (deliver done true)))
      (with-open [stdin (process/in process)]
        (.write stdin (int 1)))
      (is (true? (deref done 10000 ::timeout))))))

(defn- capture-child! [options form arguments]
  (process-util/with-child-process options form arguments
    {:out (process/capture! (process/out process))
     :err (process/capture! (process/err process))
     :exit-code (process/await-exit-code process)}))

(def ^:private default-error-modes
  (if (os/is-win32?) [false true] [false]))

;; Standard streams must stay separate unless redirected, for both launch
;; paths.
(deftest streams-test
  (doseq [default-error-mode default-error-modes
          [redirection expected-out expected-err]
          [[nil "stdout" "stderr"]
           [:stdout "stdoutstderr" nil]
           [:discard "stdout" nil]]]
    (let [{:keys [out err exit-code]}
          (capture-child! {:windows-default-error-mode default-error-mode
                           :err redirection}
                          '(do
                             (print "stdout")
                             (flush)
                             (binding [*out* *err*]
                               (print "stderr")
                               (flush)))
                          [])]
      (is (= expected-out out))
      (is (= expected-err err))
      (is (zero? exit-code))))
  (doseq [default-error-mode default-error-modes]
    (is (= {:out nil :err nil :exit-code 0}
           (capture-child! {:windows-default-error-mode default-error-mode
                            :in :inherit
                            :out :inherit
                            :err :inherit}
                           nil [])))))

;; Drain a payload larger than a pipe buffer, exercising writes with an array
;; offset and EOF delivery when stdin closes without timing assumptions.
(deftest stdin-test
  (doseq [default-error-mode default-error-modes]
    (process-util/with-child-process {:windows-default-error-mode default-error-mode}
      '(.transferTo System/in System/out) []
      (let [text (.repeat "data\"\\\n" 10000)
            bytes (.getBytes (str "pre" text "post") StandardCharsets/UTF_8)
            output (future/io (slurp (process/out process) :encoding "UTF-8"))]
        (with-open [stdin (process/in process)]
          (.write stdin bytes 3 (- (alength bytes) 7)))
        (is (= text @output))
        (is (zero? (process/await-exit-code process)))))))

;; Environment additions/removals and working directories must be honored,
;; including values and directories containing spaces.
(deftest environment-and-directory-test
  (let [directory (fs/create-temp-directory! "process directory ")]
    (try
      (doseq [default-error-mode default-error-modes]
        (let [options {:windows-default-error-mode default-error-mode}
              variable "DEFOLD_PROCESS_TEST_VALUE"]
          (is (= "value with spaces 你好"
                 (:out (capture-child! (assoc options :env {variable "value with spaces 你好"})
                                       `(print (System/getenv ~variable)) []))))
          (is (= "<unset>"
                 (:out (capture-child! (assoc options :env {"PATH" nil})
                                       '(print (or (System/getenv "PATH") "<unset>")) []))))
          (is (= (.getCanonicalFile directory)
                 (.getCanonicalFile
                   (io/file (:out (capture-child! (assoc options :dir directory)
                                                  '(print (.getCanonicalPath (java.io.File. "."))) []))))))))
      (finally
        (fs/delete-directory! directory)))))

;; Redirected files must replace pipes, and attempts to write redirected stdin
;; must fail instead of silently discarding data.
(deftest file-redirection-test
  (let [directory (fs/create-temp-directory! "process redirects ")]
    (try
      (doseq [default-error-mode default-error-modes]
        (let [input (io/file directory "input.txt")
              output (io/file directory "output.txt")]
          (spit input "redirected input" :encoding "UTF-8")
          (process-util/with-child-process {:windows-default-error-mode default-error-mode
                                            :in (ProcessBuilder$Redirect/from input)
                                            :out (ProcessBuilder$Redirect/to output)}
            '(.transferTo System/in System/out) []
            (is (thrown? IOException (.write (process/in process) (int 65))))
            (is (nil? (process/capture! (process/out process))))
            (is (zero? (process/await-exit-code process))))
          (is (= "redirected input" (slurp output :encoding "UTF-8")))
          (is (zero? (:exit-code (capture-child! {:windows-default-error-mode default-error-mode
                                                  :out (ProcessBuilder$Redirect/appendTo output)
                                                  :err :discard}
                                                 '(print "stdout") []))))
          (is (= "redirected inputstdout" (slurp output :encoding "UTF-8")))))
      (finally
        (fs/delete-directory! directory)))))

;; Readiness comes from the child, so timeout, termination, ProcessHandle, and
;; exit callbacks are checked while the process is known to be running.
(deftest lifecycle-test
  (doseq [default-error-mode default-error-modes]
    (process-util/with-child-process {:windows-default-error-mode default-error-mode}
      '(do (println "ready") (flush) (.read System/in)) []
      (with-open [^BufferedReader reader (io/reader (process/out process) :encoding "UTF-8")]
        (is (= "ready" (.readLine reader)))
        (is (.isAlive process))
        (is (= (.pid process) (.pid (.toHandle process))))
        (is (not (.waitFor process 0 TimeUnit/MILLISECONDS)))
        (is (thrown? IllegalThreadStateException (.exitValue process)))
        (let [exited (promise)]
          (process/on-exit! process (fn on-process-exit [] (deliver exited true)))
          (.destroyForcibly process)
          (is (.waitFor process 10 TimeUnit/SECONDS))
          (is (true? (deref exited 10000 ::timeout)))
          (is (not (.isAlive process)))
          (is (integer? (.exitValue process))))))))

;; Missing executables must fail at launch, rather than return a process that
;; hangs while callers wait for streams or exit notification.
(deftest missing-executable-test
  (doseq [default-error-mode default-error-modes]
    (is (thrown? IOException
                 (process/start! {:windows-default-error-mode default-error-mode}
                                 (.getAbsolutePath (io/file "missing-process.exe")))))))

(when (os/is-win32?)
  ;; The native launcher must preserve CRT arguments, including empty strings,
  ;; embedded quotes, newlines, and backslashes before quotes or the end.
  (deftest arguments-test
    (let [arguments ["" "plain" "has spaces" "a\tb" "quote\"" "\\" "tail\\\\"
                     "a\\\"b" "line\nnext" "\r" "C:\\Program Files\\" ""]
          expected (mapv (fn [argument]
                           (str "arg:" (coll/join-to-string
                                         (eduction (map #(format "%04x" (int %))) argument))))
                         arguments)
          {:keys [out err exit-code]}
          (capture-child! {:windows-default-error-mode true}
                          '(doseq [argument *command-line-args*]
                             (print "arg:")
                             (doseq [character argument]
                               (printf "%04x" (int character)))
                             (println))
                          arguments)]
      (is (= expected (string/split-lines out)))
      (is (nil? err))
      (is (zero? exit-code))))

  ;; Java's Windows launcher converts argv to ANSI. Inspect the original Unicode
  ;; command line to verify CreateProcessW without that lossy conversion.
  (deftest unicode-arguments-test
    (let [{:keys [out err exit-code]}
          (capture-child! {:windows-default-error-mode true}
                          '(let [command-line (.getWideString
                                                (.invokePointer
                                                  (.getFunction (com.sun.jna.NativeLibrary/getInstance "kernel32")
                                                                "GetCommandLineW" com.sun.jna.Function/ALT_CONVENTION)
                                                  (object-array 0))
                                                0)]
                             (print (.endsWith command-line "\"你好😀\"")))
                          ["你好😀"])]
      (is (= "true" out))
      (is (nil? err))
      (is (zero? exit-code))))

  ;; Windows exit codes are 32-bit values; 259 is also a valid final exit code,
  ;; even though GetExitCodeProcess uses it to report a running process.
  (deftest exit-code-test
    (doseq [default-error-mode [false true]
            code [0 23 259 -1073741515]]
      (is (= code
             (:exit-code (capture-child! {:windows-default-error-mode default-error-mode}
                                         `(System/exit ~code) []))))))

  ;; Set a non-default parent mode so omitting CREATE_DEFAULT_ERROR_MODE cannot
  ;; accidentally pass; restore it after checking both launch paths.
  (deftest error-mode-test
    (let [kernel (NativeLibrary/getInstance "kernel32")
          set-error-mode (.getFunction kernel "SetErrorMode" Function/ALT_CONVENTION)
          previous-mode (.invokeInt set-error-mode (object-array [(int 1)]))
          form '(print (.invokeInt
                         (.getFunction (com.sun.jna.NativeLibrary/getInstance "kernel32")
                                       "GetErrorMode" com.sun.jna.Function/ALT_CONVENTION)
                         (object-array 0)))]
      (try
        (is (= "1" (:out (capture-child! {} form []))))
        (is (= "0" (:out (capture-child! {:windows-default-error-mode true} form []))))
        (finally
          (.invokeInt set-error-mode (object-array [previous-mode])))))))
