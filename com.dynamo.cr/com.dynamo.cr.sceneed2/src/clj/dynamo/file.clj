(ns dynamo.file
  (:refer-clojure :exclude [load])
  (:require [clojure.java.io :as io]
            [clojure.osgi.core :refer [get-bundle]]
            [internal.java :as j]
            [service.log :as log])
  (:import [java.io PipedOutputStream PipedInputStream]
           [com.google.protobuf TextFormat]
           [org.eclipse.core.internal.resources File]
           [org.eclipse.core.resources IResource IFile]))

(defprotocol ProjectRelative
  (eclipse-path [this])
  (eclipse-file [this]))


(defprotocol PathManipulation
  (extension         [this])
  (replace-extension [this new-ext])
  (local-path        [this])
  (alter-path        [this f]
                     [this f args]))

(defrecord ProjectPath [project path ext]
  PathManipulation
  (extension         [this]         ext)
  (replace-extension [this new-ext] (ProjectPath. project path new-ext))
  (local-path        [this]         (str path "." ext))
  (alter-path        [this f]       (ProjectPath. project (f path) ext))
  (alter-path        [this f args]  (ProjectPath. project (apply f path args) ext))

  ProjectRelative
  (eclipse-path      [this] (.addFileExtension (.getFullPath (.getFile (:eclipse-project project) path)) ext))
  (eclipse-file      [this] (.getFile (:eclipse-project project) (local-path this)))

  io/IOFactory
  (io/make-input-stream  [this opts] (io/make-input-stream (eclipse-file this) opts))
  (io/make-reader        [this opts] (io/make-reader (io/make-input-stream this opts) opts))
  (io/make-output-stream [this opts] (io/make-output-stream (eclipse-file this) opts))
  (io/make-writer        [this opts] (io/make-writer (io/make-output-stream this opts) opts))

  Object
  (toString [this] (local-path this)))

(defmethod print-method ProjectPath
  [v ^java.io.Writer w]
  (.write w (str "<ProjectPath \"" (.path v) "." (.ext v) "\">")))

(alter-meta! #'->ProjectPath update-in [:doc] str "\n\n Takes a project, a string path, and a file extension.")

(alter-meta! #'map->ProjectPath update-in [:doc] str "\n\n See [[->ProjectPath.]]")

(defrecord NativePath [path ext]
  PathManipulation
  (extension         [this]         ext)
  (replace-extension [this new-ext] (NativePath. path new-ext))
  (local-path        [this]         (str path "." ext))
  (alter-path        [this f]       (NativePath. (f path) ext))
  (alter-path        [this f args]  (NativePath. (apply f path args) ext))

  io/IOFactory
  (io/make-input-stream  [this opts] (io/make-input-stream (.toString this) opts))
  (io/make-reader        [this opts] (io/make-reader (io/make-input-stream this opts) opts))
  (io/make-output-stream [this opts] (io/make-output-stream (.toString this) opts))
  (io/make-writer        [this opts] (io/make-writer (io/make-output-stream this opts) opts))

  Object
  (toString [this] (local-path this)))

(defmethod print-method NativePath
  [v ^java.io.Writer w]
  (.write w (str "<NativePath \"" (.path v) "." (.ext v) "\">")))

(alter-meta! #'->NativePath update-in [:doc] str "\n\n Takes a path and extension. See also [[in-build-directory]].")

(alter-meta! #'map->NativePath update-in [:doc] str "\n\n See [[->NativePath.]]")

(defrecord BundlePath [bundle path]
  io/IOFactory
  (io/make-input-stream  [this opts]
    (assert bundle (str "Bundle cannot be nil. <#BundlePath [" this "]>: "))
    (let [ent (.getEntry bundle (.toString this))]
      (when-not ent
        (throw (java.io.FileNotFoundException. (str "Bundle entry <" (.toString this) "> not found."))))
      (try
        (io/make-input-stream ent opts)
        (catch java.lang.NullPointerException npe
          ;;WAT?
          (log/error :exception npe :url ent :url-string (.toString ent) :opts opts)
          (throw npe)))))
  (io/make-reader        [this opts] (io/make-reader (io/make-input-stream this opts) opts))
  (io/make-output-stream [this opts]
    (throw (IllegalArgumentException.
             (str "Cannot open <" (pr-str this) "> as an OutputStream."))))
  (io/make-writer        [this opts] (io/make-writer (io/make-output-stream this opts) opts))

  Object
  (toString [this] path))

(defmethod print-method BundlePath
  [v ^java.io.Writer w]
  (.write w (str "<BundlePath \"" (.getSymbolicName (.bundle v)) ":" (.path v) "\">")))

(defn project-path
  ([project-state]
    (let [eproj (:eclipse-project @project-state)]
      (ProjectPath. @project-state (.toString (.removeFirstSegments (.getFullPath eproj) 1)) nil)))
  ([project-state resource]
    (let [eproj (:eclipse-project @project-state)
          file  (cond
                  (string? resource)         (.getFile eproj (str "content/" resource))
                  (instance? IFile resource) resource)
          pr (.removeFirstSegments (.getFullPath file) 1)]
      (ProjectPath. @project-state (.toString (.removeFileExtension pr)) (.getFileExtension pr)))))

(defn bundle-path [bundle resource]
  (if (string? bundle)
    (BundlePath. (get-bundle bundle) resource)
    (BundlePath. bundle resource)))

(defn in-build-directory
  [^ProjectPath p]
  (let [project               (.project p)
        eclipse-project       (:eclipse-project project)
        branch                (:branch project)
        relative-to-build-dir (clojure.string/replace (.path p) "content" (str branch "/build/default"))
        build-dir-native      (.removeLastSegments (.getLocation (.getFile eclipse-project "content")) 1)]
    (NativePath. (.toOSString (.append build-dir-native relative-to-build-dir)) (.ext p))))

(defn- new-builder
  [class]
  (j/invoke-no-arg-class-method class "newBuilder"))

(defn protocol-buffer-loader
  [^java.lang.Class class f]
  (fn [project-state nm input-reader]
    (let [builder (new-builder class)]
      (TextFormat/merge input-reader builder)
      (f project-state nm (.build builder)))))

(defmulti message->node
  (fn [message & _] (class message)))

(extend File
  io/IOFactory
  (assoc io/default-streams-impl
         :make-input-stream
         (fn [x opts]
           (.getContents x))
         :make-output-stream
         (fn [x opts]
           (let [pipe (PipedOutputStream.)
                 sink (PipedInputStream. pipe)]
             (future
               (try
                 (.create x sink IResource/FORCE nil)
                 (catch Throwable t
                   (log/error :exception t :message (str "Cannot write output to " x)))))
             pipe))))

(defmulti write-native-file (fn [path _] (class path)))

(defmethod write-native-file NativePath
  [path contents]
  (with-open [out (io/output-stream (local-path path))]
    (.write out contents)))

(defmethod write-native-file ProjectPath
  [path contents]
  (with-open [out (io/output-stream (local-path (in-build-directory path)))]
    (.write out contents)
    (.flush out)))

(defn write-native-text-file
  [path text]
  (write-native-file path (.getBytes text)))

(defn write-project-file
  [^ProjectPath path contents]
  (with-open [out (io/output-stream (eclipse-file path))]
    (.write out contents)))

(doseq [[v doc]
       {*ns*
        "Contains functions for loading and saving files. This includes the definitions
for loaders by file type (extension).

Two record types are defined, `ProjectPath` and `NativePath`.

*ProjectPath*: represents a project-relative path to a resource. The [[project-path]] function
creates and returns a ProjectPath. Implements [[PathManipulation]] and [[ProjectRelative]].

*NativePath*: represents a path, typically to a resource, as represented in
the native file system. Overrides `.toString()`. Implements [[PathManipulation]].

A project path can be converted into a native path, but the reverse is not true.

Both `ProjectPath` and `NativePath` satisfy the `clojure.java.io/IOFactory` protocol,
and have the corresponding `make-reader`, `make-writer`, `make-input-stream` and
`make-output-stream` functions."

        #'write-native-file
        "Given a NativePath and contents, writes the contents to the build path"

        #'new-builder
        "Dynamically construct a protocol buffer builder, given a class as a variable."

        #'project-path
        "Given a project-state, returns a ProjectPath containing the path to the project's files.
         The resource can be a string or an IFile."

        #'write-project-file
        "Write the given contents into the file at path."

        #'in-build-directory
        "given a ProjectPath, translates that path into a NativePath containing the
         corresponding build location"

        #'eclipse-path
        "Returns the path relative to a project container."

        #'eclipse-file
        "Returns the file relative to a project container."

        #'extension
        "Returns the extension represented by this path."

        #'replace-extension
        "Returns a new path with the desired extension."

        #'local-path
        "Returns a string representation of the path and extension."

        #'alter-path
        "Apply the function to the path part, without altering the extension, maybe with a collection of extra args."

        #'protocol-buffer-loader
          "Create a new loader that knows how to read protocol buffer files in text format.

class is the Java class generated by the protoc compiler. It will probably be an inner class
of some top-level name. Instead of a '.' for the inner class name separator, use a '$'.

For example, the inner class called AtlasProto.Atlas in Java becomes AtlasProto$Atlas.

f is a function to call with the deserialised protocol buffer message. f must take two arguments, the
resource name and the immutable protocol buffer itself."

        #'message->node
          "This is an extensible function that you implement to help load a specific file
type. Most of the time, these will be created for you by the
dynamo.file.protobuf/protocol-buffer-converter macro.

Create an implementation by adding something like this to your namespace:

    (defmethod message->node message-classname
      [message-instance container [container-target desired-output ...] & {:as overrides}]
      (,,,) ;; implementation
    )

You'll replace _message-classname_ with the Java class that matches the message
type to convert. The _message-instance_ argument will contain an instance of the
class specified in _message-classname_.

If the message instance contains other messages (as in the case of protocol buffers,
for example) then you should call message->node recursively with the same resource-name
and the child message.

When container is set, it means this node should be connected to the container. In that case,
the third argument is a collection of pairs. In each pair, container-target is the label to
connect _to_ and desired-output is the label to connect _from_. It is an error for a container
to ask for a desired-output that doesn't exist on the node being created by this function.

Overrides is a map of additional properties to set on the new node.

Given a resource name and message describing the resource,
create (and return?) a list of nodes."
          }]
  (alter-meta! v assoc :doc doc))

