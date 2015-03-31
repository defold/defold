(ns dynamo.file
  "Contains functions for navigating paths, loading, and saving files.

*NativePath*: represents a path, typically to a resource, as represented in
the native file system. Overrides `.toString()`. Implements [[PathManipulation]].

`NativePath` satisfies the `clojure.java.io/IOFactory` protocol,
so it has the corresponding `make-reader`, `make-writer`, `make-input-stream` and
`make-output-stream` functions."
  (:refer-clojure :exclude [load])
  (:require [clojure.string :as str]
            [clojure.java.io :as io]
            [dynamo.types :as t]
            [internal.java :as j]
            [service.log :as log])
  (:import [java.io File OutputStream]))

(set! *warn-on-reflection* true)

(defn- split-ext [^File f]
  (let [n ^String (.getPath f)
        i (.lastIndexOf n ".")]
    (if (pos? i)
      [(subs n 0 i) (subs n (inc i))]
      [n nil])))

(defprotocol ProjectRelative
  (project-file [this] "Returns the file (as Eclipse's IFile) relative to a project container."))

(defprotocol FileWriter
  (write-file [this ^bytes contents] "Given a path and contents, writes the contents to file. This will overwrite any existing contents."))

(defrecord ProjectPath [project-ref ^String path ^String ext]
  t/PathManipulation
  (extension         [this]         ext)
  (replace-extension [this new-ext] (ProjectPath. project-ref path new-ext))
  (local-path        [this]         (if ext (str path "." ext) path))
  (local-name        [this]         (str (last (clojure.string/split path (java.util.regex.Pattern/compile java.io.File/separator))) "." ext))

  ProjectRelative
  (project-file          [this]      (io/file (str (:content-root project-ref) "/" (t/local-path this))))

  io/IOFactory
  (io/make-input-stream  [this opts] (io/make-input-stream (project-file this) opts))
  (io/make-reader        [this opts] (io/make-reader (io/make-input-stream this opts) opts))
  (io/make-output-stream [this opts] (io/make-output-stream (project-file this) opts))
  (io/make-writer        [this opts] (io/make-writer (io/make-output-stream this opts) opts)))

(defn make-project-path [game-project name]
  (let [[path ext] (split-ext (io/file name))]
    (ProjectPath. game-project path ext)))

(defmethod print-method dynamo.file.ProjectPath
  [^ProjectPath v ^java.io.Writer w]
  (.write w (str "P<" (t/local-name v) ">")))

(defn- ensure-parents
  [path]
  (-> path
    t/local-path
    java.io.File.
    (.getParentFile)
    (.mkdirs)))

(defn- last-component
  [p]
  (last (str/split p (java.util.regex.Pattern/compile java.io.File/separator))))

(defrecord NativePath [^String path ^String ext]
  t/PathManipulation
  (extension         [this]         ext)
  (replace-extension [this new-ext] (NativePath. path new-ext))
  (local-path        [this]         (str path "." ext))
  (local-name        [this]         (str (last-component path) "." ext))

  FileWriter
  (write-file        [path contents]
    (ensure-parents path)
    (with-open [out ^OutputStream (io/output-stream (t/local-path path))]
      (.write out ^bytes contents)
      (.flush out)))

  io/IOFactory
  (io/make-input-stream  [this opts] (io/make-input-stream (.toString this) opts))
  (io/make-reader        [this opts] (io/make-reader (io/make-input-stream this opts) opts))
  (io/make-output-stream [this opts] (io/make-output-stream (.toString this) opts))
  (io/make-writer        [this opts] (io/make-writer (io/make-output-stream this opts) opts))

  Object
  (toString [this] (t/local-path this)))

(defmethod print-method NativePath
  [^NativePath v ^java.io.Writer w]
  (if (.ext v)
    (.write w (str "<NativePath \"" (.path v) "." (.ext v) "\">"))
    (.write w (str "<NativePath \"" (.path v) "\">"))))

(defn native-path [p]
  (let [[with-dot ext]  (re-find #"\.([^\./]*)$" p)
        path            (subs p 0 (- (count p) (count with-dot)))]
    (NativePath. path ext)))

(alter-meta! #'->NativePath update-in [:doc] str "\n\n Takes a path and extension. See also [[in-build-directory]].")

(alter-meta! #'map->NativePath update-in [:doc] str "\n\n See [[->NativePath.]]")
