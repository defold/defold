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

(ns editor.fs
  (:require [clojure.java.io :as io]
            [clojure.string :as string]
            [util.coll :as coll])
  (:import [clojure.lang IReduceInit]
           [java.awt Desktop Desktop$Action]
           [java.io BufferedInputStream BufferedOutputStream File FileNotFoundException IOException RandomAccessFile]
           [java.net URI URL URLDecoder]
           [java.nio.channels OverlappingFileLockException]
           [java.nio.charset Charset StandardCharsets]
           [java.nio.file AccessDeniedException CopyOption FileAlreadyExistsException FileSystems FileVisitResult FileVisitor Files LinkOption NoSuchFileException NotDirectoryException OpenOption Path SimpleFileVisitor StandardCopyOption StandardOpenOption]
           [java.nio.file.attribute BasicFileAttributes FileAttribute FileTime]
           [java.util Map UUID]))

(set! *warn-on-reflection* true)

;; util

(defonce empty-link-option-array (make-array LinkOption 0))
(defonce ^:private empty-string-array (make-array String 0))
(defonce ^:private ^"[Ljava.nio.file.OpenOption;" append-open-options (into-array OpenOption [StandardOpenOption/WRITE StandardOpenOption/CREATE StandardOpenOption/APPEND]))
(defonce ^:private ^"[Ljava.nio.file.OpenOption;" overwrite-open-options (into-array OpenOption [StandardOpenOption/WRITE StandardOpenOption/CREATE StandardOpenOption/TRUNCATE_EXISTING]))

(defprotocol PathCoercions
  "Convert to Path objects."
  (^Path as-path [this] "Coerce argument to a Path."))

(extend-protocol PathCoercions
  nil
  (as-path [_this] nil)

  File
  (as-path [this] (.toPath this))

  Path
  (as-path [this] this)

  URI
  (as-path [this] (Path/of this))

  URL
  (as-path [this] (as-path (.toURI this)))

  String
  (as-path [this] (Path/of this empty-string-array)))

(extend-protocol io/IOFactory
  Path
  (make-reader [x opts]
    (Files/newBufferedReader x (or (some-> (:encoding opts) Charset/forName)
                                   StandardCharsets/UTF_8)))
  (make-writer [x opts]
    (Files/newBufferedWriter
      x
      (or (some-> (:encoding opts) Charset/forName)
          StandardCharsets/UTF_8)
      (if (:append opts)
        append-open-options
        overwrite-open-options)))
  (make-input-stream [x _]
    (BufferedInputStream.
      (Files/newInputStream x empty-link-option-array)))
  (make-output-stream [x opts]
    (BufferedOutputStream.
      (Files/newOutputStream
        x
        (if (:append opts)
          append-open-options
          overwrite-open-options)))))

(defn path
  (^Path [x]
   (as-path x))
  (^Path [parent child]
   (let [child-path ^Path (as-path child)]
     (if (.isAbsolute child-path)
       (throw (IllegalArgumentException. (str child " is not a relative path")))
       (.resolve ^Path (as-path parent) child-path))))
  (^Path [parent child & children]
   (reduce path (path parent child) children)))

(defn real-path
  "Returns the canonical, real path to an existing file system entry. Throws an
  IOException if there was no matching entry in the file system."
  ^Path [p & ps]
  (.toRealPath ^Path (apply path p ps) empty-link-option-array))

(defn with-leading-slash
  ^String [^String path]
  (if (.startsWith path "/")
    path
    (str "/" path)))

(defn without-leading-slash
  ^String [^String path]
  (if (.startsWith path "/")
    (subs path 1)
    path))

(defn to-folder ^File [^File file]
  (if (.isFile file) (.getParentFile file) file))

(defn- same-path? [^Path src ^Path tgt]
  (try (Files/isSameFile src tgt)
       (catch NoSuchFileException _
         false)))

(defn same-file? [^File file1 ^File file2]
  (same-path? (.toPath file1) (.toPath file2)))

(defn set-executable! [^File target executable]
  (.setExecutable target executable))

(defn set-writable! [^File target writable]
  (.setWritable target writable))

(defn locked-file?
  "Returns true if we are unable to read from or write to the target location.
  Includes a check to see if a conflicting file lock exists on the file."
  [^File file]
  (try
    (cond
      (not (.exists file))
      false

      (or (not (.canRead file)) (not (.canWrite file)))
      true

      :else
      ;; If we fail to acquire an exclusive lock, we consider it locked.
      (with-open [random-access-file (RandomAccessFile. file "rw")
                  file-channel (.getChannel random-access-file)]
        (if-some [file-lock (.tryLock file-channel)]
          (do (.release file-lock)
              false)
          true)))
    (catch OverlappingFileLockException _
      true)
    (catch FileNotFoundException _
      ;; On some platforms, the RandomAccessFile constructor throws a
      ;; FileNotFoundException if it is being used by another process.
      ;; Since we already verified the file exists, we consider it locked.
      true)
    (catch SecurityException _
      true)))

(defn existing-file?
  "Returns true if the argument refers to an existing file."
  [^File file]
  (if-not (instance? File file)
    false
    (try
      (.isFile file)
      (catch Exception _
        false))))

(defn existing-directory?
  "Returns true if the argument refers to an existing directory."
  [^File directory]
  (if-not (instance? File directory)
    false
    (try
      (.isDirectory directory)
      (catch Exception _
        false))))

(defn empty-directory?
  "Returns true if the argument refers to an existing empty directory."
  [^File directory]
  (and (existing-directory? directory)
       (try
         (with-open [directory-stream (Files/newDirectoryStream (.toPath directory))]
           (not (.hasNext (.iterator directory-stream))))
         (catch Exception _
           false))))

(defmacro maybe-silently
  "If silently, returns replacement when body throws exception."
  [silently replacement & body]
  `(if ~silently
     (try ~@body (catch Throwable ~'_ ~replacement))
     ~@body))

(defn- fail-silently? [opts]
  (= (:fail opts) :silently))

(defmacro retry-writable
  "If body throws exception, retries with target set writable."
  [target & body]
  `(try ~@body
        (catch AccessDeniedException ~'_
          (set-writable! ~target true)
          ~@body)))

;; delete

(def ^:private delete-defaults {:missing :ignore})

(defn- do-delete-file! ^File [^File file {:keys [missing]}]
  (if (= missing :fail)
    (Files/delete (.toPath file))
    (Files/deleteIfExists (.toPath file)))
  file)

(defn delete-file!
  "Deletes a file. Returns the deleted file if successful.
  Options:
  :fail :silently will not throw an exception on failure, and instead return nil.
  :missing :fail will fail if the file is missing.
  :missing :ignore (default) will treat a missing file as success."
  (^File [^File file]
   (delete-file! file {}))
  (^File [^File file opts]
   (let [opts (merge delete-defaults opts)]
     (maybe-silently (fail-silently? opts) nil (retry-writable file (do-delete-file! file opts))))))

(def ^:private delete-directory-visitor
  (proxy [SimpleFileVisitor] []
    (postVisitDirectory [^Path dir ^BasicFileAttributes _attrs]
      (retry-writable (.toFile dir) (Files/delete dir))
      FileVisitResult/CONTINUE)
    (visitFile [^Path file ^BasicFileAttributes _attrs]
      (retry-writable (.toFile file) (Files/delete file))
      FileVisitResult/CONTINUE)))

(defn- do-delete-directory! ^File [^File directory {:keys [missing]}]
  (when (or (= missing :fail) (.exists directory))
    (Files/walkFileTree (.toPath directory) delete-directory-visitor))
  directory)

(defn delete-directory!
  "Deletes a directory tree. Returns the deleted directory \"file\" if successful.
  Options:
  :fail :silently will not throw an exception on failure, and instead return nil.
  :missing :fail will fail if the directory is missing.
  :missing :ignore (default) will treat a missing directory as success."
  (^File [^File directory]
   (delete-directory! directory {}))
  (^File [^File directory opts]
   (let [opts (merge delete-defaults opts)]
     (maybe-silently (fail-silently? opts) nil (do-delete-directory! directory opts)))))

(defn delete-path-directory!
  "Deletes a directory tree. Returns true if the directory was deleted by us.
  Throws NotDirectoryException if the path does not refer to a directory."
  [^Path path]
  (let [file (.toFile path)]
    (cond
      (not (.exists file))
      false

      (.isDirectory file)
      (some? (delete-directory! file))

      :else
      (throw (NotDirectoryException. (str path))))))

(defn delete!
  "Deletes a file or directory tree. Returns the deleted directory or file if successful.
  Options:
  :fail :silently will not throw an exception on failure, and instead return nil.
  :missing :fail will fail if the file or directory is missing.
  :missing :ignore will treat a missing file or directory as success."
  (^File [^File file]
   (delete! file {}))
  (^File [^File file opts]
   (let [opts (merge delete-defaults opts)]
     ;; .isDirectory will return false also if the file does not exist
     ;; thus trying to delete-file! on a "missing" file.
     (if (.isDirectory file)
       (delete-directory! file opts)
       (delete-file! file opts)))))

(defn delete-on-exit!
  "Flags a file or directory for delete when the application exits. Returns the file or directory."
  [^File file]
  (if (.isDirectory file)
    (.. Runtime getRuntime (addShutdownHook (Thread. #(delete-directory! file {:fail :silently}))))
    (.deleteOnExit file)))

(defn move-to-trash!
  "Moves a file or directory to the system recycle bin or deletes if unsupported

  Returns the trashed file if successful

  Options:
  :fail :silently will not throw an exception on failure, and instead return nil.
  :missing :fail will fail if the file or directory is missing.
  :missing :ignore will treat a missing file or directory as success."
  (^File [^File file]
   (move-to-trash! file {}))
  (^File [^File file opts]
   (maybe-silently (fail-silently? opts) nil
     (if (and (Desktop/isDesktopSupported)
              (.isSupported (Desktop/getDesktop) Desktop$Action/MOVE_TO_TRASH)
              (.moveToTrash (Desktop/getDesktop) file))
       file
       (delete! file opts)))))

;; temp

(def ^:private empty-file-attrs (into-array FileAttribute []))

(defn create-temp-file!
  "Creates a temporary file with optional prefix and suffix. Returns the file.
  The file will be deleted on exit."
  (^File [] (create-temp-file! nil nil))
  (^File [prefix] (create-temp-file! prefix nil))
  (^File [prefix suffix]
   (doto (.toFile (Files/createTempFile prefix suffix empty-file-attrs))
     (delete-on-exit!))))

(defn create-temp-directory!
  "Creates a temporary directory with optional name hint. Returns the directory.
  The directory will be deleted on exit."
  (^File [] (create-temp-directory! nil))
  (^File [hint]
   (doto (.toFile (Files/createTempDirectory hint empty-file-attrs))
     (delete-on-exit!))))

;; create directories, files

(defn create-path-directories!
  "Creates the directory path up to and including directory. Returns the directory as Path."
  ^Path [^Path path]
  (Files/createDirectories path empty-file-attrs))

(defn create-directories!
  "Creates the directory path up to and including directory. Returns the directory as File."
  ^File [^File directory]
  (.toFile (create-path-directories! (.toPath directory))))

(defn create-parent-directories!
  "Creates the directory path (if any) up to the parent directory of file. Returns the parent directory."
  ^File [^File file]
  (when-let [parent (.getParentFile file)]
    (create-directories! parent)))

(defn create-directory!
  "Creates a directory assuming all parent directories are in place. Returns the directory."
  ^File [^File directory]
  (.toFile (Files/createDirectory (.toPath directory) empty-file-attrs)))

(def ^:private ^"[Ljava.nio.file.LinkOption;" no-follow-link-options (into-array LinkOption [LinkOption/NOFOLLOW_LINKS]))

(defn create-file!
  "Creates a file if it does not already exist and fills it with optional content.
  If the supplied content is a byte array, write the bytes as-is to the file,
  otherwise, write a UTF-8 string representation of the content.
  Missing parent directories are created. Returns the file."
  (^File [^File target]
   (if (not (Files/exists (.toPath target) no-follow-link-options))
     (do (create-parent-directories! target)
         (.toFile (Files/createFile (.toPath target) empty-file-attrs)))
     target))
  (^File [^File target content]
   (let [path (.toPath target)]
     (when (not (Files/exists path no-follow-link-options))
       (create-parent-directories! target))
     (if (bytes? content)
       (Files/write path ^bytes content overwrite-open-options)
       (spit target content))
     target)))

(defn touch-file!
  "Creates a file if it does not exist and updates its last modified time."
  ^File [^File target]
  (doto (create-file! target)
    (.setLastModified (System/currentTimeMillis))))

;; move

(def ^:private ^"[Ljava.nio.file.CopyOption;" replace-copy-options (into-array CopyOption [StandardCopyOption/REPLACE_EXISTING StandardCopyOption/COPY_ATTRIBUTES]))
(def ^:private ^"[Ljava.nio.file.CopyOption;" keep-copy-options (into-array CopyOption [StandardCopyOption/COPY_ATTRIBUTES]))

(def ^:private move-defaults {:target :replace})

(def ^:private fs-temp-dir (create-temp-directory! "moved"))

(def case-sensitive?
  "Whether we're running on a case sensitive file system or not."
  (let [lower-file (io/file (io/file fs-temp-dir "casetest"))
        upper-file (io/file (io/file fs-temp-dir "CASETEST"))]
    (touch-file! lower-file)
    (not (same-path? (.toPath lower-file) (.toPath upper-file)))))

(defn- comparable-path
  ^String [entry]
  (cond-> (-> entry io/as-file .toPath .normalize .toAbsolutePath .toString)
          (not case-sensitive?) .toLowerCase))

(def separator-char File/separatorChar)

(defn below-directory?
  "Returns true if the file system entry is located below the directory."
  [^File entry ^File directory]
  (let [entry-path (comparable-path entry)
        directory-path (comparable-path directory)]
    ;; Note: can't inline File/separatorChar here, otherwise during AOT this
    ;; code will throw exception that no overload of clojure.lang.Util/equiv is
    ;; found for char and Object
    (and (= separator-char (get entry-path (count directory-path)))
         (string/starts-with? entry-path directory-path))))

(declare copy-directory!)

(defn- do-move-directory! [^File src ^File tgt opts]
  (let [halfway (io/file fs-temp-dir (str (UUID/randomUUID)))]
    (case (:target opts)
      :keep
      (do
        (copy-directory! src halfway)
        (delete-directory! src {:fail :silently})
        (if (.exists tgt)
          (do
            (copy-directory! halfway src)
            (throw (FileAlreadyExistsException. (str tgt))))
          (copy-directory! halfway tgt)))

      :replace
      (do
        (copy-directory! src halfway)
        (delete-directory! src {:fail :silently})
        (delete-directory! tgt {:fail :silently})
        (copy-directory! halfway tgt))

      :merge
      (do
        (copy-directory! src halfway)
        (delete-directory! src {:fail :silently})
        (copy-directory! halfway tgt {:target :merge})))
    (delete-directory! halfway {:fail :silently})
    [[src tgt]]))

(defn move-directory!
  "Moves a directory. Returns the source and target as a vector pair.
  Options:
  :fail :silently will not throw an exception on failure and instead return an empty vector.
  :target :keep will not replace the target directory if it exists.
  :target :replace (default) will delete the target directory and overwrite it with the source.
  :target :merge will keep the target directory but source files will overwrite their targets if any."
  ([^File src ^File tgt]
   (move-directory! src tgt {}))
  ([^File src ^File tgt opts]
   (let [opts (merge move-defaults opts)]
     (maybe-silently (fail-silently? opts) [] (do-move-directory! src tgt opts)))))
    
(def ^:private ^"[Ljava.nio.file.CopyOption;" replace-move-options (into-array CopyOption [StandardCopyOption/REPLACE_EXISTING]))
(def ^:private ^"[Ljava.nio.file.CopyOption;" keep-move-options (into-array CopyOption []))

(defn- do-move-file! [^File src ^File tgt {:keys [target] :as _opts}]
  ;; So. Files/move with src & tgt paths that refer to the same underlying file but with different
  ;; case  will ignore the move => no rename. This mostly happens on mac os and windows where the fs
  ;; is case insensitive by default.
  ;; For example: Files/move "a.txt" "A.txt" will not cause a rename.
  ;; As a workaround we temporarily move the src to a fresh file, then to tgt.
  ;; We attempt to restore the src if the final copy fails.
  (let [src-path (.toPath src)
        tgt-path (.toPath tgt)
        copy-options (if (= target :replace) replace-move-options keep-move-options)]
    (if-not (same-path? src-path tgt-path)
      (do (when-not (= target :keep)
            (delete! tgt))
          (create-parent-directories! tgt)
          (Files/move src-path tgt-path copy-options)
          [[src tgt]])
      (let [halfway-path (.toPath (io/file (.getParentFile src) (str (UUID/randomUUID))))]
        (Files/move src-path halfway-path replace-move-options)
        (try
          (when-not (= target :keep)
            (delete! tgt))
          (create-parent-directories! tgt)
          (Files/move halfway-path tgt-path copy-options)
          [[src tgt]]
          (catch Throwable e
            (Files/move halfway-path src-path replace-move-options)
            (throw e)))))))

(defn move-file!
  "Moves a file. Returns the source and target as a vector pair.
  Options:
  :fail silently will not throw an exception on failure and instead return an empty vector.
  :target :keep will not replace the target file if it exists.
  :target :replace (default) will replace the target file if it exists."
  ([^File src ^File tgt]
   (move-file! src tgt {}))
  ([^File src ^File tgt opts]
   (let [opts (merge move-defaults opts)]
     (maybe-silently (fail-silently? opts) [] (do-move-file! src tgt opts)))))

(defn move!
  "Moves a file or directory to the specified target location.
  Returns the source and target as a vector pair if successful."
  ([^File src ^File tgt]
   (move! src tgt {}))
  ([^File src ^File tgt opts]
   (if (.isDirectory src)
     (move-directory! src tgt opts)
     (move-file! src tgt opts))))

;; copy

(def ^:private ^"[Ljava.nio.file.CopyOption;" replacing-copy-options (into-array CopyOption [StandardCopyOption/REPLACE_EXISTING]))
(def ^:private ^"[Ljava.nio.file.CopyOption;" replacing-copy-attributes-copy-options (into-array CopyOption [StandardCopyOption/REPLACE_EXISTING StandardCopyOption/COPY_ATTRIBUTES]))

(def ^:private ^"[Ljava.nio.file.CopyOption;" copy-attributes-copy-options (into-array CopyOption [StandardCopyOption/COPY_ATTRIBUTES]))

(def ^:private copy-defaults {:target :replace})

(defn- do-copy-file! [^File src ^File tgt {:keys [target]}]
  (let [src-path (.toPath src)
        tgt-path (.toPath tgt)
        copy-options (if (= target :replace) replace-copy-options keep-copy-options)]
    (if (same-path? src-path tgt-path)
      [[src src]] ; nop - NB [src src] because no case-change is performed on a fs that determines (same-path? src tgt)
      (do (when-not (= target :keep)
            (delete! tgt))
          (create-parent-directories! tgt)
          (Files/copy src-path tgt-path copy-options)
          [[src tgt]]))))

(defn copy-file!
  "Copies a file to target location. Returns the source and target as a vector pair.
  Options:
  :fail :silently will not throw an exception on failure, and instead return an empty vector.
  :target :keep will not replace the target file if it exists.
  :target :replace (default) will replace the target file if it exists.
  Returns a vector of source+target File pairs if successful"
  ([^File src ^File tgt]
   (copy-file! src tgt {}))
  ([^File src ^File tgt opts]
   (let [opts (merge copy-defaults opts)]
     (maybe-silently (fail-silently? opts) [] (do-copy-file! src tgt opts)))))

(defn- make-tree-copier [^Path source ^Path target]
  (proxy [SimpleFileVisitor] []
    (preVisitDirectory [^Path source-dir ^BasicFileAttributes _attrs]
      (let [target-dir (.resolve target (.relativize source source-dir))]
        (try (Files/copy source-dir target-dir copy-attributes-copy-options) ; this will copy = create the directory, but no contents
             (catch FileAlreadyExistsException _) ; fine
             (catch IOException _
               ;; TODO: log
               FileVisitResult/SKIP_SUBTREE))
        FileVisitResult/CONTINUE))
    (visitFile [^Path source-file ^BasicFileAttributes _attrs]
      (let [target-file (.resolve target (.relativize source source-file))]
        (Files/copy source-file target-file replace-copy-options)) ; always replace
      FileVisitResult/CONTINUE)
    (postVisitDirectory [^Path source-dir ^IOException exc]
      (when-not exc
        (let [target-dir (.resolve target (.relativize source source-dir))]
          (try (Files/setLastModifiedTime target-dir (Files/getLastModifiedTime source-dir no-follow-link-options))
               (catch IOException _))))
                 ;; TODO: log

      FileVisitResult/CONTINUE)))

(defn- copy-tree! [^Path source-path ^Path target-path]
  (create-parent-directories! (.toFile target-path))
  (Files/walkFileTree source-path (make-tree-copier source-path target-path)))

(defn- do-copy-directory! [^File src ^File tgt {:keys [target]}]
  (let [src-path (.toPath src)
        tgt-path (.toPath tgt)]
    (if (same-path? src-path tgt-path)
      [[src src]]
      (do
        (case target
          :keep
          (if (.exists tgt)
            (throw (FileAlreadyExistsException. (str target)))
            (copy-tree! src-path tgt-path))

          :replace
          (do
            (delete-directory! tgt {:fail :silently})
            (copy-tree! src-path tgt-path))

          :merge
          (copy-tree! src-path tgt-path))
        [[src tgt]]))))

(defn copy-directory!
  "Copies a directory to target location. Returns the source and target as a vector pair.
  Options:
  :fail :silently will not throw an exception on failure, and instead return an empty vector.
  :target :keep will not replace the target directory if it exists.
  :target :replace (default) will delete the target directory and overwrite it with the source.
  :target :merge will keep the target directory but source files will overwrite their targets if any."
  ([^File src ^File tgt]
   (copy-directory! src tgt {}))
  ([^File src ^File tgt opts]
   (let [opts (merge copy-defaults opts)]
     (maybe-silently (fail-silently? opts) [] (do-copy-directory! src tgt opts)))))

(defn copy!
  "Copies a file or directory to target location.
  Returns the source and target as a vector pair if successful."
  ([^File src ^File tgt]
   (copy! src tgt {}))
  ([^File src ^File tgt opts]
   (if (.isDirectory src)
     (copy-directory! src tgt opts)
     (copy-file! src tgt opts))))

(defn existing-path
  "Return the argument if it exists on disc, nil otherwise

  The argument can be anything that can be coerced to a file, e.g. a String,
  File, URL etc."
  [x]
  (when (.exists (io/file x))
    x))

(def ^:private re-path-evaluator
  #"(?x)        # enable comments and whitespace in this regex
  \$([A-Z_]+)   # first group matches env var syntax
  |
  (^\~)         # second group matches home syntax (only allowed at the beginning of the string)
  |
  ([^$]+)       # third group is a path, excludes `$` that means env var syntax
  |
  (.+)          # error group, i.e. dangling `$`s at the end of the string")

(defn evaluate-path
  "Evaluate path similarly to how the shells do it, but requiring the env vars

  Returns either an expanded string or nil if some specified env vars are
  absent from the env

  Expansions:
    $ENV_VAR    substitutes with the value of the existing env var
    ~           if at the beginning of the path, substitutes with user's home"
  [raw-path]
  (reduce
    (fn [acc [_ env home path error]]
      (cond
        error (reduced nil)
        env (if-let [var (System/getenv env)]
              (str acc var)
              (reduced nil))
        home (str acc (System/getProperty "user.home"))
        path (str acc path)))
    ""
    (re-seq re-path-evaluator raw-path)))

;; read

(defn read-bytes
  "Read all the bytes from a file and return a byte array."
  ^bytes [^File src]
  (Files/readAllBytes (.toPath src)))

(defn path-walker
  "Given a directory Path, returns a reducible that walks over all file Paths in
  the dir, recursively. The optional dir-path-pred should take a directory Path,
  and return true if the walk should recurse into the directory."
  ([^Path dir-path]
   (path-walker dir-path nil))
  ([^Path dir-path dir-path-pred]
   {:pre [(instance? Path dir-path)
          (or (nil? dir-path-pred) (ifn? dir-path-pred))]}
   (reify IReduceInit
     (reduce [_ f init]
       (let [acc-vol (volatile! init)]
         (Files/walkFileTree
           dir-path
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

         (unreduced @acc-vol))))))

(defn class-path-walker [^ClassLoader class-loader dir-path]
  (reify IReduceInit
    (reduce [_ f init]
      (let [enumeration (.getResources class-loader dir-path)]
        (loop [acc init]
          (if (.hasMoreElements enumeration)
            (let [^URL url (.nextElement enumeration)
                  url-str (str url)
                  acc (cond
                        (.startsWith url-str "file:")
                        (reduce (coll/preserving-reduced f) acc (path-walker (path url)))

                        (.startsWith url-str "jar:")
                        (let [[file-uri-str entry-path] (string/split (URLDecoder/decode (.getPath url) StandardCharsets/UTF_8) #"!" 2)
                              [file-scheme file-path] (string/split file-uri-str #":" 2)]
                          (with-open [fs (^[Path Map] FileSystems/newFileSystem (path (URI. file-scheme nil file-path nil)) {})]
                            (reduce (coll/preserving-reduced f) acc (path-walker (.getPath fs entry-path empty-string-array)))))

                        :else
                        (throw (IllegalArgumentException. (str "Unsupported URL scheme: " url))))]
              (if (reduced? acc)
                @acc
                (recur acc)))
            acc))))))

(defn file-walker
  "Given a directory File, returns a reducible that walks over all Files in
  the dir, recursively."
  ([^File dir-file include-hidden]
   (file-walker dir-file include-hidden nil))
  ([^File dir-file include-hidden ignored-dirnames]
   {:pre [(instance? File dir-file)
          (every? string? ignored-dirnames)]}
   (->Eduction
     (cond->> (map #(.toFile ^Path %))
              (not include-hidden)
              (comp (remove #(Files/isHidden %))))
     (if (and include-hidden
              (coll/empty? ignored-dirnames))
       (path-walker (.toPath dir-file))
       (path-walker (.toPath dir-file)
                    (fn dir-path-pred [^Path dir-path]
                      (and (not-any? (fn [^String ignored-dirname]
                                       (.endsWith dir-path ignored-dirname))
                                     ignored-dirnames)
                           (or include-hidden
                               (not (Files/isHidden dir-path))))))))))

(defn path-exists? [path]
  (Files/exists path empty-link-option-array))

(defn path-is-directory? [path]
  (Files/isDirectory path empty-link-option-array))

(defn path-attributes
  ^BasicFileAttributes [path]
  (Files/readAttributes ^Path path BasicFileAttributes ^"[Ljava.nio.file.LinkOption;" empty-link-option-array))

(defn path? [x]
  (instance? Path x))

(defn create-path-parent-directories! [^Path path]
  (when-let [p (.getParent path)]
    (create-path-directories! p)))

(defn path-last-modified-time
  ^long [p]
  (.toMillis (Files/getLastModifiedTime p empty-link-option-array)))

(defn set-path-last-modified-time!
  [p ^long mtime]
  (Files/setLastModifiedTime p (FileTime/fromMillis mtime)))

(defn path-size
  ^long [p]
  (Files/size p))

(defn make-path-parents [^Path p]
  (when-let [parent (.getParent p)]
    (Files/createDirectories parent empty-file-attrs)))

(defn delete-path-file! [p]
  (Files/delete p))
