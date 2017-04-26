(ns editor.fs
  (:require [clojure.string :as string]
            [clojure.java.io :as io])
  (:import [java.util UUID]
           [java.io File InputStream IOException]
           [java.nio.file Path Paths Files attribute.FileAttribute attribute.BasicFileAttributes CopyOption LinkOption StandardCopyOption SimpleFileVisitor FileVisitResult
            NoSuchFileException FileAlreadyExistsException]))

(defn to-folder ^File [^File file]
  (if (.isFile file) (.getParentFile file) file))

(defn- same-path? [^Path src ^Path tgt]
  (try (Files/isSameFile src tgt)
       (catch NoSuchFileException _
         false)))

(defn same-file? [^File file1 ^Path file2]
  (same-path? (.toPath file1) (.toPath file2)))

(defn set-executable! ^File [^File target]
  (.setExecutable target true))

;; delete

(def ^:private delete-defaults {:missing :ignore})

(defn- do-delete-file! [^File file {:keys [missing]}]
  (if (= missing :fail)
    (Files/delete (.toPath file))
    (Files/deleteIfExists (.toPath file)))
  [file])

(defmacro maybe-silently [silently replacement & body]
  `(if ~silently
     (try ~@body (catch java.lang.Throwable ~'_ ~replacement))
     ~@body))

(defn- fail-silently? [opts]
  (= (:fail opts) :silently))

(defn delete-file!
  ([^File file]
   (delete-file! file {}))
  ([^File file opts]
   (let [opts (merge delete-defaults opts)]
     (maybe-silently (fail-silently? opts) [] (do-delete-file! file opts)))))

(def ^:private delete-directory-visitor
  (proxy [SimpleFileVisitor] []
    (postVisitDirectory [^Path dir ^BasicFileAttributes attrs]
      (Files/delete dir)
      FileVisitResult/CONTINUE)
    (visitFile [^Path file ^BasicFileAttributes attrs]
      (Files/delete file)
      FileVisitResult/CONTINUE)))

(defn- do-delete-directory! [^File directory {:keys [missing]}]
  (when (or (= missing :fail) (.exists directory))
    (Files/walkFileTree (.toPath directory) delete-directory-visitor))
  [directory])

(defn delete-directory!
  ([^File directory]
   (delete-directory! directory {}))
  ([^File directory opts]
   (let [opts (merge delete-defaults opts)]
     (maybe-silently (fail-silently? opts) [] (do-delete-directory! directory opts)))))

(defn delete!
  ([^File file]
   (delete! file {}))
  ([^File file opts]
   (let [opts (merge delete-defaults opts)]
     ;; .isDirectory will return false also if the file does not exist
     (if (.isDirectory file)
       (delete-directory! file opts)
       (delete-file! file opts)))))

(defn delete-on-exit!
  [^File file]
  (if (.isDirectory file)
    (.. Runtime getRuntime (addShutdownHook (Thread. #(delete-directory! file {:fail :silently}))))
    (.deleteOnExit file)))

;; temp

(def ^:private empty-file-attrs (into-array FileAttribute []))

(defn create-temp-file!
  (^File [] (create-temp-file! nil nil))
  (^File [prefix] (create-temp-file! prefix nil))
  (^File [prefix suffix]
   (doto (.toFile (Files/createTempFile prefix suffix empty-file-attrs))
     (delete-on-exit!))))

(defn create-temp-directory!
  (^File [] (create-temp-directory! nil))
  (^File [hint]
   (doto (.toFile (Files/createTempDirectory hint empty-file-attrs))
     (delete-on-exit!))))

;; create directories, files

(defn create-directories! ^File [^File directory]
  (.toFile (Files/createDirectories (.toPath directory) empty-file-attrs)))

(defn create-parent-directories! ^File [^File file]
  (create-directories! (.getParentFile file)))

(defn create-directory! ^File [^File directory]
  (.toFile (Files/createDirectory (.toPath directory) empty-file-attrs)))

(def ^:private ^"[Ljava.nio.file.LinkOption;" no-follow-link-options (into-array LinkOption [LinkOption/NOFOLLOW_LINKS]))

(defn create-file!
  (^File [^File target]
   (if (not (Files/exists (.toPath target) no-follow-link-options))
     (do (create-parent-directories! target)
         (Files/createFile (.toPath target) empty-file-attrs))
     target))
  (^File [^File target content]
   (create-file! target)
   (spit target content)
   target))

(defn touch-file! ^File [^File target]
  (create-file! target)
  (.setLastModified target (System/currentTimeMillis))
  target)

;; move

(def ^:private ^"[Ljava.nio.file.CopyOption;" replace-copy-options (into-array CopyOption [StandardCopyOption/REPLACE_EXISTING StandardCopyOption/COPY_ATTRIBUTES]))
(def ^:private ^"[Ljava.nio.file.CopyOption;" keep-copy-options (into-array CopyOption [StandardCopyOption/COPY_ATTRIBUTES]))

(def ^:private move-defaults {:target :replace})

(def ^:private fs-temp-dir (create-temp-directory! "moved"))

(def case-sensitive?
  (let [lower-file (io/file (io/file fs-temp-dir "casetest"))
        upper-file (io/file (io/file fs-temp-dir "CASETEST"))]
    (touch-file! lower-file)
    (not (same-path? (.toPath lower-file) (.toPath upper-file)))))

(declare copy-directory!)

(defn- do-move-directory! [^File src ^File tgt {:as opts}]
  (let [src-path (.toPath src)
        tgt-path (.toPath tgt)
        halfway (io/file fs-temp-dir (str (UUID/randomUUID)))]
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
  ([^File src ^File tgt]
   (move-directory! src tgt {}))
  ([^File src ^File tgt opts]
   (let [opts (merge move-defaults opts)]
     (maybe-silently (fail-silently? opts) [] (do-move-directory! src tgt opts)))))
    
(def ^:private ^"[Ljava.nio.file.CopyOption;" replace-move-options (into-array CopyOption [StandardCopyOption/REPLACE_EXISTING]))
(def ^:private ^"[Ljava.nio.file.CopyOption;" keep-move-options (into-array CopyOption []))

(defn- do-move-file! [^File src ^File tgt {:keys [target] :as opts}]
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
  ([^File src ^File tgt]
   (move-file! src tgt {}))
  ([^File src ^File tgt opts]
   (let [opts (merge move-defaults opts)]
     (maybe-silently (fail-silently? opts) [] (do-move-file! src tgt opts)))))

(defn move!
  "Moves a file system entry to the specified target location. Any existing
  files at the target location will be overwritten. If it does not already
  exist, the path leading up to the target location will be created. Returns
  a sequence of [source, destination] File pairs that were successfully moved."
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
  ([^File src ^File tgt]
   (copy-file! src tgt {}))
  ([^File src ^File tgt opts]
   (let [opts (merge copy-defaults opts)]
     (maybe-silently (fail-silently? opts) [] (do-copy-file! src tgt opts)))))

(defn- make-tree-copier [^Path source ^Path target]
  (proxy [SimpleFileVisitor] []
    (preVisitDirectory [^Path source-dir ^BasicFileAttributes attrs]
      (let [target-dir (.resolve target (.relativize source source-dir))]
        (try (Files/copy source-dir target-dir copy-attributes-copy-options) ; this will copy = create the directory, but no contents
             (catch FileAlreadyExistsException _) ; fine
             (catch IOException _
               ;; TODO: log
               FileVisitResult/SKIP_SUBTREE))
        FileVisitResult/CONTINUE))
    (visitFile [^Path source-file ^BasicFileAttributes attrs]
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
          (if (.exists target)
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
  ([^File src ^File tgt]
   (copy-directory! src tgt {}))
  ([^File src ^File tgt opts]
   (let [opts (merge copy-defaults opts)]
     (maybe-silently (fail-silently? opts) [] (do-copy-directory! src tgt opts)))))

(defn copy!
  ([^File src ^File tgt]
   (copy! src tgt {}))
  ([^File src ^File tgt opts]
   (if (.isDirectory src)
     (copy-directory! src tgt opts)
     (copy-file! src tgt opts))))
