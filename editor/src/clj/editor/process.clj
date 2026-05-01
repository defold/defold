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

(ns editor.process
  (:require [clojure.java.io :as io]
            [clojure.string :as string])
  (:import [java.lang Process ProcessBuilder ProcessBuilder$Redirect]
           [java.util List]
           [java.io InputStream StringWriter OutputStream]))

(set! *warn-on-reflection* true)

(defn- ->redirect
  ^ProcessBuilder$Redirect [x]
  (case x
    :pipe ProcessBuilder$Redirect/PIPE
    :inherit ProcessBuilder$Redirect/INHERIT
    :discard ProcessBuilder$Redirect/DISCARD
    x))

(defn ^{:arglists '([& command+args] [opts & command+args])} start!
  "Start a process

  Optional leading opts map:
    :in     either:
              :pipe - default, available as [[in]]
              :inherit - connects to this process's stdin
    :out    either:
              :pipe - default, available as [[out]]
              :discard
              :inherit - connects to this process's stdout
    :err    either:
              :pipe - default, available as [[err]]
              :discard
              :inherit - connects to this process's stderr
              :stdout - redirects to spawned process's out
    :dir    the process working directory
    :env    process env map, keys are env var names, vars are either strings
            (values) or nil (removes the var from the process env)"
  ^Process [& opts+args]
  (let [has-opts (map? (first opts+args))
        opts (if has-opts (first opts+args) {})
        command (if has-opts (rest opts+args) opts+args)
        {:keys [in out err dir env]} opts
        pb (ProcessBuilder. ^List command)]
    (when dir (.directory pb (io/file dir)))
    (when in (.redirectInput pb (->redirect in)))
    (when out (.redirectOutput pb (->redirect out)))
    (when err
      (if (= :stdout err)
        (.redirectErrorStream pb true)
        (.redirectError pb (->redirect err))))
    (when env
      (let [m (.environment pb)]
        (doseq [[k v] env]
          (if v
            (.put m k v)
            (.remove m k)))))
    (.start pb)))

(defn out
  "Get the process out, an InputStream that can be read from"
  ^InputStream [^Process process]
  (.getInputStream process))

(defn in
  "Get the process in, an OutputStream that can be written to"
  ^OutputStream [^Process process]
  (.getOutputStream process))

(defn err
  "Get the process err, an InputStream that can be read from"
  ^InputStream [^Process process]
  (.getErrorStream process))

(defn ^{:arglists '([^InputStream in & {:keys [buffer-size encoding] :or {buffer-size 1024 encoding "UTF-8"}}])}
  capture!
  "Given an InputStream such as process's out or err, consume it into a string

  Returns a non-empty string with trimmed new lines or nil if it's empty

  Optional kv-args:
    :buffer-size    read buffer size, default 1024
    :encoding       text encoding, default UTF-8"
  [^InputStream input-stream & opts]
  (let [w (StringWriter.)]
    (apply io/copy input-stream w opts)
    (let [s (string/trim-newline (.toString w))]
      (when (pos? (.length s))
        s))))

(defn pipe!
  "Pipe data from the InputStream (e.g. [[out]] or [[err]]) to OutputStream

  Similar to io/copy and InputStream::transferTo, but flushes on write"
  [^InputStream in ^OutputStream out]
  (let [buf (byte-array 1024)]
    (loop []
      (let [size (.read in buf)]
        (when-not (neg? size)
          (do (.write out buf 0 size)
              (.flush out)
              (recur)))))))

(defn await-exit-code
  "Blocks until process exits and returns the exit code

  Zero exit code indicates success, any other number indicates failure"
  ^long [^Process process]
  (.waitFor process))

(defn exec!
  "Execute command and return the output on successful exit or throw otherwise

  Returns a non-empty string with trimmed new lines or nil if it's empty

  Optional leading opts map:
    :in     either:
              :pipe - default, available as [[in]]
              :inherit - connects to this process's stdin
    :out    either:
              :pipe - default, available as [[out]]
              :discard
              :inherit - connects to this process's stdout
    :err    either:
              :pipe - default, available as [[err]]
              :discard
              :inherit - connects to this process's stderr
              :stdout - redirects to spawned process's out
    :dir    the process working directory
    :env    process env map, keys are env var names, vars are either strings
            (values) or nil (removes the var from the process env)"
  [& opts+args]
  (let [has-opts (map? (first opts+args))
        opts (if has-opts (first opts+args) {})
        command (if has-opts (rest opts+args) opts+args)
        ^Process process (apply start! (into {:err :inherit} opts) command)
        output (future (capture! (out process)))
        exit-code (await-exit-code process)]
    (if (zero? exit-code)
      @output
      (throw (ex-info (format "Non-zero exit code for command '%s': %s"
                              (string/join " " command)
                              exit-code)
                      {:command command
                       :exit-code exit-code
                       :opts opts
                       :process process})))))

(defn on-exit!
  "Invoke a Runnable (e.g. a 0-arg function) when process exits"
  [^Process process on-exit]
  (.thenRun (.onExit process) ^Runnable on-exit)
  nil)