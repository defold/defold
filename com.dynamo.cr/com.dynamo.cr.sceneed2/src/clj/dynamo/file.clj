(ns dynamo.file
  "Contains functions for navigating paths, loading, and saving files.

Two record types are defined, `ProjectPath` and `NativePath`.

*ProjectPath*: represents a project-relative path to a resource. The [[make-project-path]] function
creates and returns a ProjectPath, which implements [[PathManipulation]] and [[ProjectRelative]].

*NativePath*: represents a path, typically to a resource, as represented in
the native file system. Overrides `.toString()`. Implements [[PathManipulation]].

A project path can be converted into a native path, but the reverse is not true.

Both `ProjectPath` and `NativePath` satisfy the `clojure.java.io/IOFactory` protocol,
and have the corresponding `make-reader`, `make-writer`, `make-input-stream` and
`make-output-stream` functions."
  (:refer-clojure :exclude [load])
  (:require [clojure.string :as str]
            [clojure.java.io :as io]
            [clojure.osgi.core :refer [get-bundle]]
            [dynamo.types :as t]
            [internal.java :as j]
            [service.log :as log])
  (:import [java.io OutputStream PipedOutputStream PipedInputStream Reader]
           [com.google.protobuf TextFormat GeneratedMessage$Builder]
           [org.osgi.framework Bundle]
           [org.eclipse.core.internal.resources File]
           [org.eclipse.core.resources IProject IResource IFile]
           [org.eclipse.core.runtime IPath Path]))

(defn- eproj ^IProject [p] (:eclipse-project p))

(defprotocol ProjectRelative
  (project-file [this] "Returns the file (as Eclipse's IFile) relative to a project container."))

(defprotocol FileWriter
  (write-file [this ^bytes contents] "Given a path and contents, writes the contents to file. This will overwrite any existing contents."))

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

(defrecord ProjectPath [project-ref ^String path ^String ext]
  t/PathManipulation
  (extension         [this]         ext)
  (replace-extension [this new-ext] (ProjectPath. project-ref path new-ext))
  (local-path        [this]         (if ext (str path "." ext) path))
  (local-name        [this]         (str (last-component path) "." ext))

  FileWriter
  (write-file        [path contents]
    (ensure-parents path)
    (with-open [out ^OutputStream (io/output-stream path)]
      (.write out ^bytes contents)
      (.flush out)))

  ProjectRelative
  (project-file          [this]      (.getFile (eproj @project-ref) (str "content/" (t/local-path this))))

  io/IOFactory
  (io/make-input-stream  [this opts] (io/make-input-stream (project-file this) opts))
  (io/make-reader        [this opts] (io/make-reader (io/make-input-stream this opts) opts))
  (io/make-output-stream [this opts] (io/make-output-stream (project-file this) opts))
  (io/make-writer        [this opts] (io/make-writer (io/make-output-stream this opts) opts))

  Object
  (toString [this] (t/local-path this)))

(defmethod print-method ProjectPath
  [^ProjectPath v ^java.io.Writer w]
  (.write w (str "<ProjectPath \"" (t/local-path v) "\">")))

(alter-meta! #'->ProjectPath update-in [:doc] str "\n\n Takes a project, a string path, and a file extension.")

(alter-meta! #'map->ProjectPath update-in [:doc] str "\n\n See [[->ProjectPath.]]")

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

(defn make-project-path
  "Given a project-scope node, returns a ProjectPath containing the path to the project's files.
   The resource can be a string or an IFile."
  ([project-scope]
    (let [ep (eproj project-scope)]
      (ProjectPath. project-scope (.toString (.removeFirstSegments (.getFullPath ep) 1)) nil)))
  ([project-scope resource]
    (let [ep    (eproj project-scope)
          file  (cond
                  (instance? IFile resource) resource
                  (instance? IPath resource) (.getFile ep (.append (Path. "content") resource))
                  :else                      (.getFile ep (str "content/" resource)))
          pr    (.removeFirstSegments (.getFullPath ^IFile file) 2)
          ext   (.getFileExtension pr)
          p     (str "/" (.toString pr))
          p     (if-not ext p (subs p 0 (- (count p) (count ext) 1)))]
      (ProjectPath. (t/node-ref project-scope) p ext))))

(defn in-build-directory
  "Given a ProjectPath, translates that path into a NativePath containing the
   corresponding build location"
  [^ProjectPath p]
  (let [project               @(:project-ref p)
        eclipse-project       (eproj project)
        branch                (:branch project)
        relative-to-build-dir (str branch "/build/default/" (:path p))
        build-dir-native      (.removeLastSegments (.getLocation (.getFile eclipse-project "content")) 1)]
    (NativePath. (.toOSString (.append build-dir-native relative-to-build-dir)) (:ext p))))

(extend File
  io/IOFactory
  (assoc io/default-streams-impl
         :make-input-stream
         (fn [^File x opts]
           (.getContents x))
         :make-output-stream
         (fn [^File x opts]
           (let [pipe (PipedOutputStream.)
                 sink (PipedInputStream. pipe)]
             (future
               (try
                 (.setContents x sink true false nil)
                 (catch Throwable t
                   (log/error :exception t :message (str "Cannot write output to " x)))))
             pipe))))
