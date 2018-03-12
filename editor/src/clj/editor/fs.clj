(ns editor.fs
  (:require [clojure.java.io :as io])
  (:import [java.util UUID]
           [java.io File FileNotFoundException IOException RandomAccessFile]
           [java.nio.channels OverlappingFileLockException]
           [java.nio.file AccessDeniedException CopyOption FileAlreadyExistsException Files FileVisitResult LinkOption NoSuchFileException Path SimpleFileVisitor StandardCopyOption]
           [java.nio.file.attribute BasicFileAttributes FileAttribute]))

(set! *warn-on-reflection* true)

;; util

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

(defn set-executable! ^File [^File target]
  (.setExecutable target true))

(defn set-writable! ^File [^File target]
  (.setWritable target true))

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

(defn empty-directory?
  "Returns true if the argument refers to an existing empty directory."
  [^File directory]
  (if-not (instance? File directory)
    false
    (try
      (and (.isDirectory directory)
           (with-open [directory-stream (Files/newDirectoryStream (.toPath directory))]
             (not (.hasNext (.iterator directory-stream)))))
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
          (set-writable! ~target)
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

(defn create-directories!
  "Creates the directory path up to and including directory. Returns the directory."
  ^File [^File directory]
  (.toFile (Files/createDirectories (.toPath directory) empty-file-attrs)))

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
  "Creates a file if it does not already exist and fills it with optional UTF-8 content.
  Missing parent directories are created. Returns the file."
  (^File [^File target]
   (if (not (Files/exists (.toPath target) no-follow-link-options))
     (do (create-parent-directories! target)
         (.toFile (Files/createFile (.toPath target) empty-file-attrs)))
     target))
  (^File [^File target content]
   (create-file! target)
   (spit target content)
   target))

(defn touch-file!
  "Creates a file if it does not exist and updates its last modified time."
  ^File [^File target]
  (create-file! target)
  (.setLastModified target (System/currentTimeMillis))
  target)

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
  :target :replace (default) will replace the target file if it exists."
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
               (catch IOException _
                 ;; TODO: log
                 ))))
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
