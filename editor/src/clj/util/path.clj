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

(ns util.path
  (:refer-clojure :exclude [compare resolve])
  (:require [clojure.java.io :as io]
            [util.defonce :as defonce])
  (:import [clojure.lang IReduceInit]
           [java.io BufferedInputStream BufferedOutputStream File]
           [java.net URI URL]
           [java.nio.charset Charset StandardCharsets]
           [java.nio.file CopyOption FileVisitOption FileVisitResult FileVisitor Files LinkOption NoSuchFileException OpenOption Path StandardOpenOption]
           [java.nio.file.attribute BasicFileAttributes FileAttribute FileTime]
           [java.util Comparator Set]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(defonce ^:private ^CopyOption/1 empty-copy-option-array (make-array CopyOption 0))
(defonce ^:private ^FileAttribute/1 empty-file-attribute-array (make-array FileAttribute 0))
(defonce ^:private ^FileVisitOption/1 empty-file-visit-option-array (make-array FileVisitOption 0))
(defonce ^:private ^LinkOption/1 empty-link-option-array (make-array LinkOption 0))
(defonce ^:private ^String/1 empty-string-array (make-array String 0))

(defonce ^:private ^OpenOption/1 append-open-options (into-array OpenOption [StandardOpenOption/WRITE StandardOpenOption/CREATE StandardOpenOption/APPEND]))
(defonce ^:private ^OpenOption/1 overwrite-open-options (into-array OpenOption [StandardOpenOption/WRITE StandardOpenOption/CREATE StandardOpenOption/TRUNCATE_EXISTING]))

(defonce/protocol Coercions
  "Convert to Path objects."
  (^Path as-path [this] "Coerce argument to a Path."))

(extend-protocol Coercions
  nil
  (as-path [_this] nil)

  File
  (as-path [this] (.toPath this))

  Path
  (as-path [this] this)

  URI
  (as-path [this] (Path/of this))

  URL
  (as-path [this] (Path/of (.toURI this)))

  String
  (as-path [this] (Path/of this empty-string-array)))

(extend-protocol io/Coercions
  Path
  (as-file [this] (.toFile this))
  (as-url [this] (.toURL (.toUri this))))

(extend-protocol io/IOFactory
  Path
  (make-reader [this opts]
    (Files/newBufferedReader
      this
      (or (some-> (:encoding opts) Charset/forName)
          StandardCharsets/UTF_8)))
  (make-writer [this opts]
    (Files/newBufferedWriter
      this
      (or (some-> (:encoding opts) Charset/forName)
          StandardCharsets/UTF_8)
      (if (:append opts)
        append-open-options
        overwrite-open-options)))
  (make-input-stream [this _opts]
    (BufferedInputStream.
      (Files/newInputStream this empty-link-option-array)))
  (make-output-stream [this opts]
    (BufferedOutputStream.
      (Files/newOutputStream
        this
        (if (:append opts)
          append-open-options
          overwrite-open-options)))))

(defn path?
  "Returns true if the supplied value is a java.nio.file.Path."
  [value]
  (instance? Path value))

(defn of
  "Returns a java.nio.file.Path, passing each argument to as-path. When called
  with multiple arguments, the first argument is treated as the parent and
  later arguments as children relative to the parent."
  (^Path [x]
   (as-path x))
  (^Path [parent child]
   (let [child-path ^Path (as-path child)]
     (if (.isAbsolute child-path)
       (throw (IllegalArgumentException. (str child " is not a relative path")))
       (.resolve ^Path (as-path parent) child-path))))
  (^Path [parent child & children]
   (reduce of (of parent child) children)))

(defn normalized
  "Returns a normalized java.nio.file.Path, passing each argument to as-path.
  When called with multiple arguments, the first argument is treated as the
  parent and later arguments as children relative to the parent."
  (^Path [x]
   (.normalize (as-path x)))
  (^Path [x & xs]
   (.normalize ^Path (apply of x xs))))

(defn absolute
  "Returns an absolute java.nio.file.Path, passing each argument to as-path.
  When called with multiple arguments, the first argument is treated as the
  parent and later arguments as children relative to the parent."
  (^Path [x]
   (.toAbsolutePath (as-path x)))
  (^Path [x & xs]
   (.toAbsolutePath ^Path (apply of x xs))))

(defn real
  "Returns the canonical, real path to an existing file system entry. Throws an
  IOException if there was no matching entry in the file system. Follows
  symbolic links and queries their target."
  (^Path [x]
   (.toRealPath (as-path x) empty-link-option-array))
  (^Path [x & xs]
   (.toRealPath ^Path (apply of x xs) empty-link-option-array)))

(defn to-file
  "Converts the supplied java.nio.file.Path to a java.io.File. Does not coerce
  the argument into a Path. Returns nil if called with a nil value."
  ^File [^Path path]
  (when path
    (.toFile path)))

(defn to-uri
  "Converts the supplied java.nio.file.Path to a java.net.URI. Does not coerce
  the argument into a Path. Returns nil if called with a nil value."
  ^URI [^Path path]
  (when path
    (.toUri path)))

(defn relativize
  "Coerces both arguments to java.nio.file.Path and returns a relative Path
  that, when resolved against this path, yields a path that locates the same
  file as the other path. For example, on UNIX, if this path is `/a/b` and the
  other path is `/a/b/c/d`, then the resulting relative path would be `c/d`."
  ^Path [this other]
  (.relativize (as-path this) (as-path other)))

(defn resolve
  "Coerces both arguments to java.nio.file.Path and resolves the other path
  against this path. If the other parameter is an absolute path, returns its
  coerced Path. If other is an empty path, returns the Path coerced from the
  first argument. Otherwise, we consider this path to be a directory and
  resolve the other path against this path. For example, on UNIX, if this path
  is `/a/b` and the other path is `../c/d`, then the resulting resolved path would
  be `/a/c/d`."
  ^Path [this other]
  (.resolve (as-path this) (as-path other)))

(defn resolve-sibling
  "Coerces both arguments to java.nio.file.Path and resolves the other path
  against this path's parent path. This is useful when a file name needs to be
  replaced with another file name."
  ^Path [this other]
  (.resolveSibling (as-path this) (as-path other)))

(defn reroot
  "Coerces all arguments to java.nio.file.Path, then replace the start of the
  path specified by this-root with the other-root in this path."
  ^Path [this this-root other-root]
  (->> (relativize this-root this)
       (.resolve (as-path other-root))
       (.normalize)))

(defn root
  "Coerces the argument to a java.nio.file.Path and returns its root component
  as a Path, or nil if the path does not have a root component."
  ^Path [x]
  (.getRoot (as-path x)))

(defn parent
  "Coerces the argument to a java.nio.file.Path and returns its parent Path, or
  nil if the path does not have a parent."
  ^Path [x]
  (.getParent (as-path x)))

(defn leaf
  "Coerces the argument to a java.nio.file.Path and returns its file name as a
  Path, or nil if the path is empty."
  ^Path [x]
  (.getFileName (as-path x)))

(defn byte-size
  "Coerces the argument to a java.nio.file.Path and returns the byte size of the
  corresponding file system entry. Beware that the size of entries that are not
  regular files is implementation-specific."
  ^long [x]
  (Files/size (as-path x)))

(defn last-modified-ms
  "Coerces the argument to a java.nio.file.Path and returns the last modified
  time of the corresponding file system entry in milliseconds since the UNIX
  epoch. Follows symbolic links and queries their target."
  ^long [x]
  (.toMillis (Files/getLastModifiedTime (as-path x) empty-link-option-array)))

(defn set-last-modified-ms!
  "Coerces the argument to a java.nio.file.Path and sets the last modified
  time of the corresponding file system entry. The time is in milliseconds since
  the UNIX epoch. Returns the Path."
  ^Path [x ^long epoch-time]
  (Files/setLastModifiedTime (as-path x) (FileTime/fromMillis epoch-time)))

(defn attributes
  "Coerces the argument to a java.nio.file.Path and reads the attributes of the
  denoted file system entry as a bulk operation. Returns an instance of the
  java.nio.file.attribute.BasicFileAttributes interface. Follows symbolic links
  and queries their target."
  ^BasicFileAttributes [x]
  (^[Path Class LinkOption/1] Files/readAttributes (as-path x) BasicFileAttributes empty-link-option-array))

(defn posix-file-permissions
  "Coerces the argument to a java.nio.file.Path and returns its POSIX file
  permissions as a set of java.nio.file.attribute.PosixFilePermission enum
  values. Follows symbolic links and queries their target."
  ^Set [x]
  (Files/getPosixFilePermissions (as-path x) empty-link-option-array))

(defn exists?
  "Coerces the argument to a java.nio.file.Path and returns true if a
  corresponding file system entry exists. Follows symbolic links and queries
  their target."
  [x]
  (Files/exists (as-path x) empty-link-option-array))

(defn directory?
  "Coerces the argument to a java.nio.file.Path and returns true if a
  corresponding file system entry exists and is a directory. Follows symbolic
  links and queries their target."
  [x]
  (Files/isDirectory (as-path x) empty-link-option-array))

(defn file?
  "Coerces the argument to a java.nio.file.Path and returns true if a
  corresponding file system entry exists and is a regular file. Follows symbolic
  links and queries their target."
  [x]
  (Files/isRegularFile (as-path x) empty-link-option-array))

(defn symbolic-link?
  "Coerces the argument to a java.nio.file.Path and returns true if a
  corresponding file system entry exists and is a symbolic link."
  [x]
  (Files/isSymbolicLink (as-path x)))

(defn absolute?
  "Coerces the argument to a java.nio.file.Path and returns true if the path is
  absolute."
  [x]
  (.isAbsolute (as-path x)))

(defn hidden?
  "Coerces the argument to a java.nio.file.Path and returns true if it denotes a
  hidden file system entry."
  [x]
  (Files/isHidden (as-path x)))

(defn same?
  "Coerces both arguments to java.nio.file.Path and returns true if both paths
  locate the same file system entry. Returns false when one or more of the paths
  do not exist. Follows symbolic links and compares their targets."
  [x y]
  (let [a (as-path x)
        b (as-path y)]
    (try
      (Files/isSameFile a b)
      (catch NoSuchFileException _
        false))))

(defn compare
  "Coerces both arguments to java.nio.file.Path and compares the two paths
  lexicographically. This function does not access the file system."
  ^long [x y]
  (.compareTo (as-path x) (as-path y)))

(defn starts-with?
  "Coerces both arguments to java.nio.file.Path and returns true if this path
  starts with the denoted prefix."
  [this prefix]
  (.startsWith (as-path this) (as-path prefix)))

(defn ends-with?
  "Coerces both arguments to java.nio.file.Path and returns true if this path
  ends with the denoted suffix."
  [this suffix]
  (.endsWith (as-path this) (as-path suffix)))

(defn move!
  "Coerces both arguments to java.nio.file.Path and moves this file to the
  new path specified as the destination. If this path is a symlink, the symlink
  itself will be moved. Can be used to move a non-empty directory to a new
  destination on the same FileStore. However, if the FileStore differs, and it
  is determined that the operation requires copying the directory contents, this
  function will throw an IOException. Returns the destination Path."
  [this destination]
  (Files/move (as-path this) (as-path destination) empty-copy-option-array))

(defn delete!
  "Coerces the argument to a java.nio.file.Path and deletes the corresponding
  entry from the file system. If the entry is a symbolic link, then the symbolic
  link itself, not the final target of the link, is deleted. If the path refers
  to a directory, then the directory must be empty. Returns nil."
  [x]
  (Files/delete (as-path x)))

(defn delete-tree!
  "Coerces the argument to a java.nio.file.Path and deletes all the entries in
  the corresponding directory tree from the file system. If the path refers to a
  symbolic link, then the symbolic link itself, not the final target of the
  link, is deleted. If the path refers to a regular file, it will be deleted. If
  the path refers to a directory, the directory and all files within it will be
  deleted. Returns nil."
  [x]
  (-> (as-path x)
      (Files/walk empty-file-visit-option-array)
      (.sorted (Comparator/reverseOrder))
      (.forEach Files/delete)))

(defn create-directory!
  "Coerces the argument to a java.nio.file.Path and creates a directory
  in the file system assuming all parent directories are in place. Returns the
  Path to the directory."
  ^Path [x]
  (Files/createDirectory (as-path x) empty-file-attribute-array))

(defn create-directories!
  "Coerces the argument to a java.nio.file.Path and creates missing directories
  in the file system for the entire path. Throws an exception if the path refers
  to an already-existing file. Returns the resulting Path."
  ^Path [x]
  (Files/createDirectories (as-path x) empty-file-attribute-array))

(defn create-parent-directories!
  "Coerces the argument to a java.nio.file.Path and creates missing parent
  directories in the file system leading up to the path. Throws an exception if
  the parent of the path refers to an already-existing file. Returns the Path
  the argument was coerced into."
  ^Path [x]
  (let [path (as-path x)]
    (when-let [parent (.getParent path)]
      (create-directories! parent))
    path))

(defn create-symbolic-link!
  "Coerces both arguments to java.nio.file.Path and creates a symbolic link in
  the file system at the path denoted by the link argument referring to the path
  denoted by the target argument. Returns the Path to the symbolic link."
  ^Path [link target]
  (let [link-path (as-path link)
        target-path (as-path target)]
    (Files/createSymbolicLink link-path target-path empty-file-attribute-array)))

(defn tree-walker
  "Coerces the argument to a java.nio.file.Path and returns a reducible that
  walks over all file Paths in the denoted directory, recursively. The optional
  dir-path-pred should take a subdirectory Path and return true if the walk
  should recurse into the subdirectory."
  ([root-dir]
   (tree-walker root-dir nil))
  ([root-dir dir-path-pred]
   {:pre [(or (nil? dir-path-pred) (ifn? dir-path-pred))]}
   (let [root-dir-path (as-path root-dir)]
     (reify IReduceInit
       (reduce [_ f init]
         (let [acc-vol (volatile! init)]
           (Files/walkFileTree
             root-dir-path
             (reify FileVisitor
               (preVisitDirectory [_ path _]
                 (cond
                   (nil? dir-path-pred)
                   FileVisitResult/CONTINUE

                   (dir-path-pred path)
                   FileVisitResult/CONTINUE

                   :else
                   FileVisitResult/SKIP_SUBTREE))

               (visitFile [_ path _]
                 (let [acc (vswap! acc-vol f path)]
                   (if (reduced? acc)
                     FileVisitResult/TERMINATE
                     FileVisitResult/CONTINUE)))

               (visitFileFailed [_ _ exception]
                 (throw exception))

               (postVisitDirectory [_ _ exception]
                 (if exception
                   (throw exception)
                   FileVisitResult/CONTINUE))))

           (unreduced @acc-vol)))))))
