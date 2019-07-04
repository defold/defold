;;
;; Hello.
;;
;; Please read this first.
;;
;; This is a reimplementation of the bundling of Android applications from the
;; Bob Java version to a Clojure version that is more in line with the rest of
;; the editor code base.
;;
;; It is not finished. Look for TODO and NOTE items in the file to get an idea
;; of the work that remains.

(ns editor.bundle-android
  "Pretty much a mechanical translation of AndroidBundler.java from Bob"
  (:require [cljstache.core :as cljstache]
            [clojure.java.io :as io]
            [clojure.java.shell :as shell]
            [clojure.string :as string]
            [dynamo.graph :as g]
            [editor.defold-project :as project]
            [editor.fs :as fs]
            [editor.game-project-core :as game-project-core]
            [editor.resource :as resource]
            [editor.system :as system]
            [editor.workspace :as workspace]
            [editor.yamlparser :as yp]
            [service.log :as log])
  (:import [com.android.manifmerger ManifestMerger2 ManifestMerger2$MergeType MergingReport MergingReport$MergedManifestKind]
           [com.android.utils ILogger]
           [com.defold.editor.archive ArchiveBuilder ManifestBuilder]
           [com.defold.editor.archive.publisher NullPublisher PublisherSettings]
           [com.defold.editor.pipeline ResourceNode]
           [com.dynamo.liveupdate.proto Manifest$HashAlgorithm Manifest$SignAlgorithm]
           [java.awt AlphaComposite RenderingHints]
           [java.awt.image BufferedImage]
           [java.io ByteArrayOutputStream File FileOutputStream IOException]
           [java.net URI]
           [java.nio.file Files FileSystem FileSystems LinkOption StandardOpenOption]
           [java.nio.file.attribute FileAttribute]
           [java.util HashMap]
           [java.util.zip CRC32 ZipEntry ZipFile ZipInputStream ZipOutputStream]
           [javax.imageio ImageIO]))


(set! *warn-on-reflection* true)

;; TODO: Change the output folder to include target platform name? - Probably
;; not actually. Because we want to be able to package "fat" apks that include
;; 32 and 64 bit binaries. Still have to split between debug and release though,
;; so maybe that should be a folder so that they don't overwrite each other.
;; Currently there are no subfolders created under the target folder.

;; TODO: Get rid of all temp folders that are created in the system temp folder.
;; Just create them in the bundle directory with a name like _tmp and delete
;; when bundling is done. Much easier to get an overview and debug stuff while
;; developing. The current approach creates more than one temporary directory in
;; some random location which is not very helpful if something goes wrong.

;; TODO: Streamline bundle-info. Remove all unused stuff and flatten it to make
;; stuff easier to find and use! Also write down for each key when creating the
;; initial structure what they are used for (comment on line above). I spent a
;; lot of time on this task figuring out what all the things were for and I
;; think some of the information stored in the bundle-info is not even needed.

;; TODO: Implement any missing Bob options if relevant. Compression of archives?
;; Liveupdate?

;; TODO: The "unit" tests need more love. I have mostly done REPL/CIDER based
;; testing when developing.

(defn init!
  "Get Bob root directory from env or create a temp folder.
  Returns the root directory as a File object."
  ;; TODO: Remove references to Bob? TODO: Remove the reliance on this env var.
  ;; Should use only temporary directory under the target folder.
  ^File []
  (if-let [env-root (io/file (System/getenv "DM_BOB_ROOTFOLDER"))]
    (if-not (.isDirectory env-root)
      (throw (IOException. (format "Invalid value for environment variable DM_BOB_ROOTFOLDER: %s is not a directory." env-root)))
      env-root)
    (let [f (.toFile (Files/createTempDirectory nil (into-array FileAttribute [])))]
      ;; TODO: We should probably delete this right away after bundling? But we
      ;; can also keep this as an extra cleanup because it will just ignore if
      ;; the directory is gone already.
      (.addShutdownHook (Runtime/getRuntime) (Thread. #(fs/delete! f {:missing :ignore :fail :silently})))
      f)))

;; TODO: Move the architechture and platform stuff somewhere else when making
;; bundling for additional arch/platforms combos.
;; TODO: It seems likely that these structures can be simplified.
(defn- make-architectures
  [architectures default-architectures]
  {:architectures architectures
   :default-architectures default-architectures})

(defn- make-platform
  [arch os exe-suffixes exe-prefix lib-prefix lib-suffix extender-paths architectures extender-pair]
  {:arch arch
   :os os
   :exe-suffixes exe-suffixes
   :exe-prefix exe-prefix
   :lib-suffix lib-suffix
   :lib-prefix lib-prefix
   :extender-paths extender-paths
   :architectures architectures
   :extender-pair extender-pair})

(def ^:const architectures
  {:osx (make-architectures ["x86_64-darwin"], ["x86_64-darwin"])
   :windows (make-architectures ["x86_64-win32", "x86-win32"], ["x86_64-win32", "x86-win32"])
   :linux (make-architectures ["x86_64-linux"], ["x86_64-linux"])
   :ios (make-architectures ["arm64-darwin", "armv7-darwin", "x86_64-ios"], ["arm64-darwin", "armv7-darwin"])
   :android (make-architectures ["arm64-android", "armv7-android"], ["armv7-android", "arm64-android"])
   :web (make-architectures ["js-web", "wasm-web"], ["js-web", "wasm-web"])})

(def ^:const platforms
  {:x86-darwin (make-platform "x86", "darwin", [""], "", "lib", ".dylib", ["osx", "x86-osx"], (:osx architectures), "x86-osx")
   :x86-64-darwin (make-platform "x86_64", "darwin", [""], "", "lib", ".dylib", ["osx", "x86_64-osx"], (:osx architectures), "x86_64-osx")
   :x86-win32 (make-platform "x86", "win32", [".exe"], "", "", ".dll", ["win32", "x86-win32"], (:windows architectures), "x86-win32")
   :x86-64-win32 (make-platform "x86_64", "win32", [".exe"], "", "", ".dll", ["win32", "x86_64-win32"], (:windows architectures), "x86_64-win32")
   :x86-linux (make-platform "x86", "linux", [""], "", "lib", ".so", ["linux", "x86-linux"], (:linux architectures), "x86-linux")
   :x86-64-linux (make-platform "x86_64", "linux", [""], "", "lib", ".so", ["linux", "x86_64-linux"], (:linux architectures), "x86_64-linux")
   :armv7-darwin (make-platform "armv7", "darwin", [""], "", "lib", ".so", ["ios", "armv7-ios"], (:ios architectures), "armv7-ios")
   :arm64-darwin (make-platform "arm64", "darwin", [""], "", "lib", ".so", ["ios", "arm64-ios"], (:ios architectures), "arm64-ios")
   :x86-64-ios (make-platform "x86_64", "ios", [""], "", "lib", ".so", ["ios", "x86_64-ios"], (:ios architectures), "x86_64-ios")
   :armv7-android (make-platform "armv7", "android", [".so"], "lib", "lib", ".so", ["android", "armv7-android"], (:android architectures), "armv7-android")
   :arm64-android (make-platform "arm64", "android", [".so"], "lib", "lib", ".so", ["android", "arm64-android"], (:android architectures), "arm64-android")
   :js-web (make-platform "js", "web", [".js"], "", "lib", "", ["web", "js-web"], (:web architectures), "js-web")
   :wasm-web (make-platform "wasm", "web", [".js", ".wasm"], "", "lib", "", ["web", "wasm-web"], (:web architectures), "wasm-web")})

(defn- platform-pair
  "Returns a string that represents an architechture and operating system
  combination."
  [platform]
  (str (:arch platform) "-" (:os platform)))

(defn host-platform-key
  "Returns a keyword representing the current host platform. One of:
  :x86-64-win32
  :x86-win32
  :x86-64-darwin
  :x86-darwin
  :x86-64-linux
  :x86-linux
  Or :unknown if the platform cannot be determined."
  []
  (let [name (.toLowerCase (System/getProperty "os.name"))
        arch (.toLowerCase (System/getProperty "os.arch"))]
    (cond
      (> (.indexOf name "win") -1) (if (or (= arch "x86_64") (= arch "amd64"))
                                     :x86-64-win32
                                     :x86-win32)
      (> (.indexOf name "mac") -1) (if (or (= arch "x86_64") (= arch "amd64"))
                                     :x86-64-darwin
                                     :x86-darwin)
      (> (.indexOf name "linux") -1) (if (or (= arch "x86_64") (= arch "amd64"))
                                       :x86-64-linux
                                       :x86-linux)
      :else :unknown)))

(def ^:private ^:const default-dmengine-exe-names
  {:release "dmengine_release"
   :debug "dmengine"
   :headless "dmengine_headless"})

(defn- copy-resource-to-file!
  "Copies a JVM resource (io/resource) to `file`. Will create parent directories
  if needed."
  [resource-name ^File file]
  (when-let [res (io/resource resource-name)]
    (do (.mkdirs (.getParentFile file))
        (with-open [in (io/input-stream res)]
          (io/copy in file)))))

;; NOTE: The following two functions have a Java smell. They can probably be
;; improved or removed by having better data up front.
(defn- get-exe-with-extension
  "Return an executable path in the filesystem for the given platform, name and
  extensions combination."
  ^String [root-dir platform name extension]
  (io/file root-dir (platform-pair platform) (str (:exe-prefix platform) (str name extension))))

(defn- get-exe-files
  "Returns a sequence of possible exe files for a platform and name combination."
  [root-dir platform name]
  (for [ext (:exe-suffixes platform)]
    (get-exe-with-extension root-dir platform name ext)))

(defn init-android!
  "Sets up files and directories on disk for android bundling. This includes
  creating the temporary bundling work directory (root-dir) and
  extracting/copying any needed files and tools to that folder. Returns a map
  with some information needed for functions later in the bundling process."
  [^File root-dir target-platforms variant]
  (let [platform-key (host-platform-key)
        host-platform (get platforms platform-key)
        host-lib-prefix (:lib-prefix host-platform)
        host-lib-suffix (:lib-suffix host-platform)
        host-pair (platform-pair host-platform)
        libc-name (str host-lib-prefix "c++" host-lib-suffix)
        target-platforms (select-keys platforms target-platforms)
        ;; TODO: Redo the get-exe-files to not return files and perhaps not full paths?
        exe-files (for [tp target-platforms
                        exe-file (get-exe-files "." (val tp) (default-dmengine-exe-names variant))]
                    [(name (key tp)) (.getName ^File exe-file)])]
    (copy-resource-to-file!
      (str "lib/" host-pair "/" libc-name)
      (io/file root-dir (str host-pair "/lib/" libc-name)))
    (copy-resource-to-file!
      "lib/android.jar"
      (io/file root-dir "lib/android.jar"))
    (copy-resource-to-file!
      "lib/classes.dex"
      (io/file root-dir "lib/classes.dex"))
    (dorun (for [[platform exe] exe-files]
             (copy-resource-to-file!
               (str "libexec/" platform "/" exe)
               (io/file root-dir platform exe))))
    (with-open [zip (ZipInputStream. (io/input-stream (io/resource "android-res.zip")))]
      (loop [entry (.getNextEntry zip)]
        (when entry
          (let [out (io/file root-dir "android" "res" (.getName entry))]
            (fs/create-directories! (.getParentFile out))
            (io/copy zip out)
            (.closeEntry zip)
            (recur (.getNextEntry zip))))))
    {:target-dir root-dir
     :host-platform host-platform
     :host-platform-key platform-key
     :platforms target-platforms}))

(defn- project-name->binary-name
  "Scrubs a given string of characters that might not be suitable for an
  executable on all platforms. Any such characters will simply be removed. If
  the scrubbed version is the empty string, the name 'dmengine' will be
  returned."
  [^String project-name]
  (let [stripped (.replaceAll project-name "[^a-zA-Z0-9_]" "")]
    (if (empty? stripped)
      "dmengine"
      stripped)))

(defn- platform-binary-names
  "Returns a sequence of executable names for the given platform."
  [platform basename]
  (let [{prefix :exe-prefix, suffixes :exe-suffixes} platform]
    (for [suffix suffixes]
      (str prefix basename suffix))))

(defn- get-native-extension-engine-binaries
  "Return a sequence of all locally built executables."
  [platform extender-exe-dir]
  (let [binary-names (platform-binary-names platform "dmengine")]
    (filter #(.exists ^File %)
            (for [name binary-names]
              (io/file extender-exe-dir (:extender-pair platform) name)))))

(defn- collect-platform-binaries
  "Find existing exe and dex files for the given platforms."
  [root-dir platforms extender-exe-dir build-variant]
  (reduce
    (fn [m [platform-name platform]]
      (if-let [^File exe (first (get-native-extension-engine-binaries platform extender-exe-dir))]
        (-> m
            (update :platform-to-exe assoc platform-name exe)
            (update :platform-to-exe-path assoc platform-name (.getAbsolutePath exe))
            (update :classes-dex into (->> (repeat (str extender-exe-dir "/" (platform-pair platform)))
                                           (map-indexed (fn [i path]
                                                          (let [i (inc i)]
                                                            (if (= i 1)
                                                              (io/file path "classes.dex")
                                                              (io/file path (format "classes%d.dex" i))))))
                                           (take-while #(.exists ^File %)))))
        ;; TODO: first only!?!?!? That was a mistake. Have to have all of them?
        (let [^File exe (first (get-exe-files root-dir platform (default-dmengine-exe-names build-variant)))]
          (-> m
              (update :classes-dex conj (io/file root-dir "lib/classes.dex"))
              (update :platform-to-exe assoc platform-name exe)
              (update :platform-to-exe-path assoc platform-name (.getAbsolutePath exe))))))
    {:classes-dex #{}
     :platform-to-exe {}
     :platform-to-exe-path {}}
    platforms))

(defn- make-path-map-hierarchical
  "Helper function for turning the game project settings into a keyword to value
  map."
  [m]
  (reduce
    (fn [m [k v]] (assoc-in m (map keyword k) v))
    {}
    m))

(def ^:private ^:const default-game-project-settings
  "Keyword to value mappings based on the android entries found in
  `game-project-core/default-settings`"
  (make-path-map-hierarchical (game-project-core/default-settings)))

(defn- fix-resource-path-slashes
  "Trim the path and prepend a slash if there is none and remove slash from end if
  there is one."
  [path]
  (let [path1 (string/trim path)]
    (if-not (empty? path1)
      (let [path2 (if (string/ends-with? path1 "/")
                    (subs path1 0 (dec (count path1)))
                    path1)]
        (if-not (string/starts-with? path2 "/")
          (str "/" path2)
          path2))
      path1)))

(defn- starts-with-exclusion-path
  "Checks if path (a string) starts with any of the strings in exclusion-paths.
  Returns true or nil."
  [path exclusion-paths]
  (some identity (map #(string/starts-with? path %) exclusion-paths)))

(defn- get-bundle-and-ext-resources
  "Finds all the resources that were explicitly specified as bundle resources by
  the user and anything under res directories in extensions. Except whatever the
  user specified in the exclusions list. The result is added to bundle-info
  under the key :resources."
  [bundle-info]
  (let [defold-project (:project bundle-info)
        game-project (:project (:settings bundle-info))
        platforms (:platforms bundle-info)
        resource-map (g/node-value (project/workspace defold-project) :resource-map)
        exclude-set (->> (string/split (:bundle_exclude_resources game-project) #",")
                         (into #{} (comp (map fix-resource-path-slashes)
                                         (remove empty?)
                                         (filter resource-map))))
        bundle-resource-paths (->> (string/split (:bundle_resources game-project) #",")
                                   (into [] (comp (map fix-resource-path-slashes)
                                                  (map #(resource-map (string/trim %)))
                                                  (remove empty?))))
        ext-manifest-files (filter #(= "ext.manifest" (resource/resource-name %)) (vals resource-map))
        extender-paths (distinct (mapcat #(-> % second :extender-paths) platforms))
        extension-res-dirs (into [] (comp (mapcat #(for [ext-path extender-paths]
                                                     (str (resource/parent-proj-path (resource/proj-path %)) "/res/" ext-path "/res")))
                                          (map resource-map)
                                          (remove empty?))
                                 ext-manifest-files)
        paths (->> (concat bundle-resource-paths extension-res-dirs)
                   (into [] (remove #(starts-with-exclusion-path (resource/proj-path %) exclude-set))))
        resources (->> paths
                       (into [] (comp (mapcat (fn [r] (resource/resource-seq r #(not (exclude-set (resource/proj-path %))))))
                                      (filter #(and (= :file (resource/source-type %))
                                                    (not (exclude-set (resource/proj-path %)))))))) ;; TODO: Check if we really need this exclude check. We already did it in the mapcat?
        ext-manifests (into [] (remove empty?)
                            (for [ep extender-paths
                                  mf ext-manifest-files]
                              (let [epk (keyword ep)
                                    m (yp/load (slurp mf) keyword)]
                                (-> m :platforms epk))))]
    (merge bundle-info {;;:extension-res-dirs extension-res-dirs
                        ;;bundle-resource-paths bundle-resource-paths
                        :extender-paths extender-paths
                        :resource-paths paths
                        :resources resources
                        :ext-manifests ext-manifests})))

(defn- collect-bundling-information
  "Collect all information we can up front that is needed for Android bundling.
  Returns a map of information used for bundling. Mostly paths and options."
  [defold-project bundle-dir options evaluation-context]
  (let [ws (project/workspace defold-project evaluation-context)
        project-dir (workspace/project-path ws evaluation-context)
        {:keys [strip-executable certificate key variant platforms]} options
        variant (or variant :release)
        platforms (or platforms [:armv7-android :arm64-android])
        bundle-info (init-android! (init!) platforms variant)
        root-dir (:target-dir bundle-info)
        extender-exe-dir (.getPath (io/file root-dir "build"))
        game-project-settings (-> defold-project
                                  (project/get-resource-node "/game.project" evaluation-context)
                                  (g/node-value :settings-map)
                                  (make-path-map-hierarchical))
        clean-title (project-name->binary-name (-> game-project-settings :project :title))
        app-dir (io/file bundle-dir clean-title)
        settings (-> (merge {:exe-name clean-title
                             :has-icons? true}
                            default-game-project-settings
                            game-project-settings)
                     (assoc :orientation-support
                            (if (-> game-project-settings :display :dynamic_orientation)
                              (let [{:keys [width height]} (-> game-project-settings :display)]
                                (if (and (and width height) (> width height))
                                    "landscape"
                                    "portrait"))
                              "sensor")))
        tmp-res-dir (.toFile (Files/createTempDirectory "res" (into-array FileAttribute [])))]
    (-> (merge bundle-info
               {:settings settings
                :project defold-project
                :project-dir project-dir
                :content-root "build/default"
                :platform-to-lib {:armv7-android "armeabi-v7a"
                                  :arm64-android "arm64-v8a"}
                :platform-to-striptool {:armv7-android "strip_android"
                                        :arm64-android "strip_android_aarch64"}
                :variant variant
                :strip-executable strip-executable
                :certificate (or certificate "")
                :key (or key "")
                :extender-exe-dir extender-exe-dir
                :app-dir app-dir
                :ap1 (io/file app-dir (str clean-title ".ap1"))
                :ap2 (io/file app-dir (str clean-title ".ap2"))
                :ap3 (io/file app-dir (str clean-title ".ap3"))
                :apk (io/file app-dir (str clean-title ".apk"))
                :res-dir (io/file bundle-dir clean-title "res")
                :manifest-file (io/file app-dir "AndroidManifest.xml")
                :aapt-path (str (system/defold-unpack-path) "/" (platform-pair (:host-platform bundle-info)) "/bin/aapt")
                :zipalign-path (str (system/defold-unpack-path) "/" (platform-pair (:host-platform bundle-info)) "/bin/zipalign")
                :apkc-path (str (system/defold-unpack-path) "/" (platform-pair (:host-platform bundle-info)) "/bin/apkc")
                ;; TODO: Should delete this temp dir when done.
                :tmp-res-dir tmp-res-dir
                ;; This is the root of where we put all the extension resources. We
                ;; need to move (and extract) them so the android tools can find
                ;; them because a lot of the resources will be in zip files.
                :tmp-extensions-dir (io/file tmp-res-dir "extensions")
                :rjava-dir (io/file tmp-res-dir "rjava")})
        (merge (collect-platform-binaries root-dir (:platforms bundle-info) extender-exe-dir variant))
        (get-bundle-and-ext-resources))))

(defn- create-directories!
  "Create all the base directories needed for the bundling process. Drawable
  resources directories are created later."
  [bundle-info]
  (let [{:keys [app-dir res-dir tmp-res-dir platform-to-lib tmp-extensions-dir rjava-dir]} bundle-info]
    (fs/delete-directory! app-dir)
    (fs/create-directories! app-dir)
    (fs/create-directories! res-dir)
    (fs/create-directories! tmp-res-dir)
    (fs/create-directories! tmp-extensions-dir)
    (fs/create-directories! rjava-dir)
    (dorun
      (for [v (vals platform-to-lib)]
        (fs/create-directories! (io/file app-dir "libs" v)))))
  bundle-info)

(defn- android-manifest?
  [resource]
  (= "AndroidManifest.xml" (resource/resource-name resource)))

(defn- android-manifests
  "Get all android manifest resources from the given project."
  [project evaluation-context]
  ;; TODO: should probably not grab all of them. Just take the ones that are in
  ;; a native extension directory under the explicit path
  ;; manifests/android/AndroidManifest.xml. This is the only supported place for
  ;; those files so users may expect files not placed there to not be merged.
  ;; They could for example have placed an Android manifest in a temporary
  ;; location when testing different versions and then the bundling gets ruined
  ;; because we grabbed all of them.
  (filterv android-manifest? (g/node-value project :resources evaluation-context)))

(defn merge-android-manifests!
  "Will merge Android manifests using the official Google ManifestMerger.
  This function will throw exceptions if ManifestMerger does.
  `app-manifest` should be the main application manifest file.
  `lib-manifests` should be a sequence of library manifest files.
  Returns a report object of type `MergingReport` that contains the final merged
  document or error information if the merge failed and did not throw an
  exception."
  ^MergingReport [^File app-manifest lib-manifests]
  (let [logger (reify ILogger
                 (error [this t fmt args]
                   (if t
                     (log/error :exception t :msg (apply format fmt args))
                     (log/error :msg (apply format fmt args))))
                 (warning [this fmt args]
                   (log/warn :msg (apply format fmt args)))
                 (info [this fmt args]
                   (log/info :msg (apply format fmt args)))
                 (verbose [this fmt args]
                   (log/trace :msg (apply format fmt args))))
        invoker (doto (ManifestMerger2/newMerger app-manifest logger ManifestMerger2$MergeType/APPLICATION)
                  (.addLibraryManifests (into-array lib-manifests)))
        report (.merge invoker)]
    report))

(defn- merge-and-write-manifest!
  "Merge all found AndroidManifest.xml files using the Google manifest merger and
  put the result in a new AndroidManifest.xml in the app-dir."
  [bundle-info evaluation-context]
  (let [project (:project bundle-info)
        settings (:settings bundle-info)
        ws (project/workspace project evaluation-context)
        source-manifest-resource (-> settings :android :manifest)
        all-manifest-resources (android-manifests project evaluation-context)
        extension-manifest-resources (remove #{source-manifest-resource} all-manifest-resources)
        rendered-source-manifest (cljstache/render (slurp source-manifest-resource) settings)
        rendered-extension-manifests (map #(cljstache/render (slurp %) settings) extension-manifest-resources)
        ^File main-manifest-file (-> bundle-info :manifest-file)
        extension-manifest-files (for [i (range 1 (inc (count rendered-extension-manifests)))]
                                   (io/file (.getParent main-manifest-file) (str "ext_manifest_" i ".xml")))]
    ;; Write main manifest file for input to merge tool or as final result if
    ;; there were no extension manifests
    (spit main-manifest-file rendered-source-manifest)
    (when (seq rendered-extension-manifests)
      (dorun (for [[mf txt] (partition 2 (interleave extension-manifest-files rendered-extension-manifests))]
               (spit mf txt)))
      (let [report (merge-android-manifests! main-manifest-file extension-manifest-files)]
        (if (.isSuccess (.getResult report))
          (do (run! #(fs/delete! % {:fail :silently}) extension-manifest-files)
              (spit main-manifest-file (.getMergedDocument report MergingReport$MergedManifestKind/MERGED)))
          (throw (ex-info "Failed to merge Android manifests" {:report (str report)}))))))
  bundle-info)

(defn- create-resource-directories!
  "Create output directories for the android drawable resources."
  [bundle-info]
  (let [res-dir (:res-dir bundle-info)]
    (fs/create-directories! (io/file res-dir "drawable"))
    (fs/create-directories! (io/file res-dir "drawable-ldpi"))
    (fs/create-directories! (io/file res-dir "drawable-mdpi"))
    (fs/create-directories! (io/file res-dir "drawable-hdpi"))
    (fs/create-directories! (io/file res-dir "drawable-xhdpi"))
    (fs/create-directories! (io/file res-dir "drawable-xxhdpi"))
    (fs/create-directories! (io/file res-dir "drawable-xxxhdpi")))
  bundle-info)

(defn- get-fallback-icon-image
  "Returns a BufferedImage of the first image found in category/key in the
  game.project, searching alternative-keys in order. If no such file is found,
  returns a BufferedImage of default_icon.png."
  ^BufferedImage [bundle-info category alternative-keys]
  (let [cat (get (:settings bundle-info) category)
        res (some #(get cat %) alternative-keys)]
    (if res
      (ImageIO/read (io/file res))
      (ImageIO/read (io/resource "default_icon.png")))))

(defn- resize-image
  "Make a copy of a BufferedImage, resized with bililnear interpolation to a
  square of given size."
  ^BufferedImage [^BufferedImage original ^long square-size]
  (let [size (int square-size)
        resized-image (BufferedImage. size size BufferedImage/TYPE_INT_ARGB)
        g (.createGraphics resized-image)]
    (doto g
      (.setRenderingHint RenderingHints/KEY_INTERPOLATION RenderingHints/VALUE_INTERPOLATION_BILINEAR)
      (.setRenderingHint RenderingHints/KEY_RENDERING RenderingHints/VALUE_RENDER_QUALITY)
      (.setRenderingHint RenderingHints/KEY_ANTIALIASING RenderingHints/VALUE_ANTIALIAS_ON)
      (.drawImage original 0 0 size size nil)
      (.dispose)
      (.setComposite AlphaComposite/Src))
    resized-image))

(defn- android-asset-directory?
  "Checks if a directory is an android asset directory by checking for the
  presence of any of a set of known subdirectories:
  #{\"values\" \"xml\" \"layout\" \"animator\" \"anim\" \"color\" \"drawable\"
    \"mipmap\" \"menu\" \"raw\" \"font\" \"drawable-xxxhdpi\" \"drawable-xxhdpi\"
    \"drawable-xhdpi\" \"drawable-hdpi\" \"drawable-mdpi\" \"drawable-ldpi\"}"
  [^File dir]
  (some (fn [^File sub-dir]
          (#{"values" "xml" "layout" "animator" "anim" "color" "drawable" "mipmap" "menu" "raw" "font"
             "drawable-xxxhdpi" "drawable-xxhdpi" "drawable-xhdpi" "drawable-hdpi" "drawable-mdpi" "drawable-ldpi"}
            (.getName sub-dir)))
        (filter #(.isDirectory ^File %) (.listFiles dir))))

(defn- gen-icon!
  "Generates an icon file for the given resource name, using the resource directly
  if it exists, otherwise a fallback image will be resized to the given size and
  used instead."
  [bundle-info fallback-image category resource-name file-name square-size]
  (let [out (io/file (:res-dir bundle-info) file-name)
        prop (-> (:settings bundle-info)
                 (get category)
                 (get resource-name))]
    (if prop
      (io/copy (io/file prop) out)
      (ImageIO/write ^BufferedImage (resize-image fallback-image square-size) "png" out))))

(defn- copy-icons!
  "Copies the icons specified in the game.project file to the corresponding
  drawable* directories in the app-dir."
  [bundle-info]
  ;; These property names must be the same as the ones in game.project, but as keywords.
  (let [icon-prop-names [:app_icon_192x192 :app_icon_144x144 :app_icon_96x96 :app_icon_72x72 :app_icon_48x48 :app_icon_36x36 :app_icon_32x32]
        sizes [192 144 96 72 48 36 32]
        dir-names ["drawable-xxxhdpi" "drawable-xxhdpi" "drawable-xhdpi" "drawable-hdpi" "drawable-mdpi" "drawable-ldpi" "drawable-ldpi"]
        fallback-image (get-fallback-icon-image bundle-info :android icon-prop-names)
        push-icon-names ["push_icon_small" "push_icon_large"]
        push-notification-icon-prop-names (-> (into [] (comp (mapcat #(vector (str "push_icon_small_" %) (str "push_icon_large_" %)))
                                                             (map keyword))
                                                    ["xxxhdpi" "xxhdpi" "xhdpi" "hdpi" "mdpi" "ldpi"])
                                              (conj :push_icon_small :push_icon_large))
        push-notification-icon-dir-names (into [] (comp (distinct)
                                                        (mapcat #(vector % %))) ;; Duplicate all entries to get one directory for each small/large icon pair.
                                               (conj dir-names "drawable"))]
    (dorun (for [[prop-name size dir] (partition 3 (interleave icon-prop-names sizes dir-names))]
             (gen-icon! bundle-info fallback-image :android prop-name (str dir "/icon.png") size)))
    (dorun (for [[icon-name icon-prop dir] (partition 3 (interleave (cycle push-icon-names)
                                                                    push-notification-icon-prop-names
                                                                    push-notification-icon-dir-names))]
             (when-let [res (-> bundle-info :settings :android icon-prop)]
               (io/copy (io/input-stream res) (io/file (:res-dir bundle-info) dir (str icon-name ".png")))))))
  bundle-info)

(defn- store-resources!
  "Copies all found resources to the bundling working directory."
  [bundle-info]
  ;; TODO: Do we even need this "extensions" directory? Can't we just put
  ;; everything in the tmp-resources-dir?
  (let [{:keys [tmp-extensions-dir resources]} bundle-info]
    (dorun (for [r resources]
             (let [file (io/file (subs (resource/proj-path r) 1))
                   dir (io/file tmp-extensions-dir (.getParentFile file))
                   out-file (io/file tmp-extensions-dir file)]
               (when (.exists out-file)
                 (throw (ex-info (format "Duplicate resource: %s" (resource/proj-path r)) {:resource r})))
               (fs/create-directories! dir)
               (io/copy (io/input-stream r) out-file)))))
  bundle-info)

(defn- find-android-assets
  "This is an information gathering function but it must be run after
  `store-resources!` because it will look for Android resource directories among
  the directories written to disk by that function. The reason for looking there
  instead of looking in the in-memory project resources is that we need on disk
  resources to feed to the Android tools."
  [bundle-info]
  (let [{:keys [extender-paths tmp-extensions-dir target-dir resource-paths]} bundle-info
        dirs (->> resource-paths
                  (into []
                        (comp
                          (map #(subs (resource/proj-path %) 1))
                          (map #(io/file tmp-extensions-dir %))
                          (mapcat file-seq)
                          (filter #(.isDirectory ^File %))
                          (filter (fn [f] (some #(string/includes? (.getAbsolutePath ^File f) %) extender-paths)))
                          (filter android-asset-directory?))))
        dirs (concat dirs (filter #(.isDirectory ^File %) (.listFiles (io/file target-dir "android" "res"))))]
    (assoc-in bundle-info [:settings :android :resource-dirs] dirs)))

(defn- run-aapt!
  "Runs the bundled aapt program with the given arguments. Returns nil on success,
  otherwise throws an exception."
  [bundle-info args]
  (let [platform (:host-platform bundle-info)
        env (if (#{:x86-64-linux :x86-linux} (:host-platform-key bundle-info))
              {"LD_LIBRARY_PATH" (.getCanonicalPath
                                   (io/file (:target-dir bundle-info)
                                            (platform-pair platform)
                                            "lib"))}
              {})
        aapt-path (:aapt-path bundle-info)

        {:keys [out err exit] :as result}
        (apply shell/sh (concat [aapt-path] args [:env env]))]

    (when-not (zero? exit)
      (throw (ex-info (format "aapt error: %s %s" out err) {})))))

(defn- create-rjava!
  "Build the R.java resource index file."
  [bundle-info]
  (let [target-dir (:target-dir bundle-info)
        resource-dirs (conj (get-in bundle-info [:settings :android :resource-dirs]) (:res-dir bundle-info))
        debuggable (get-in bundle-info [:settings :android :debuggable])
        extra-packages (string/join ":" ["com.google.firebase"
                                         "com.google.android.gms"
                                         "com.google.android.gms.common"
                                         "com.android.support"])
        manifest (.getCanonicalPath ^File (:manifest-file bundle-info))
        android-jar (str target-dir "/lib/android.jar")
        rjava-dir (.getCanonicalPath ^File (:rjava-dir bundle-info))
        ap1 (.getCanonicalPath ^File (:ap1 bundle-info))
        resources (interpose "-S" (map #(.getCanonicalPath ^File %) resource-dirs))
        args (remove
               nil?
               (concat
                 ["package" "-f"
                  "--extra-packages" extra-packages
                  "-m" "--auto-add-overlay"
                  "-M" manifest
                  "-I" android-jar
                  "-J" rjava-dir
                  "-F" ap1
                  (when debuggable "--debug-mode")
                  (when-not (empty? resources) "-S")]
                 resources))]
    (run-aapt! bundle-info args)
    bundle-info))

(defn- get-zipfs
  "Returns a new FileSystem instance for a given zip file, with file creation
  permissions."
  ^FileSystem [^File zipfile]
  (let [fs-env (doto (HashMap.) (.put "create" "true"))
        uri (URI/create (str "jar:" (.toURI zipfile)))]
    (FileSystems/newFileSystem uri fs-env)))

(defn- add-to-zip!
  "Add a file in zipfs under entry-name"
  [^FileSystem zipfs ^File file ^String entry-name]
  (let [path (.getPath zipfs (str "/" entry-name) (into-array String []))
        parent (.getParent path)]
    (when (and parent (Files/notExists parent (into-array LinkOption [])))
      (Files/createDirectories parent (into-array FileAttribute [])))
    (with-open [out (Files/newOutputStream path (into-array [StandardOpenOption/CREATE StandardOpenOption/WRITE]))]
      (io/copy file out))))

(defn- package-dex-files!
  "Adds all the dex files to the .ap1 file."
  [bundle-info]
  (let [{:keys [classes-dex ap1]} bundle-info]
    (with-open [fs (get-zipfs ap1)]
      (run! #(add-to-zip! fs % (.getName ^File %)) classes-dex))
    bundle-info))

(defn- package-archived-project-files!
  "Adds the archived game files to the .ap1 file."
  [bundle-info]
  (let [{:keys [ap1 project-dir content-root]} bundle-info
        files (into [] (comp
                         (map #(io/file project-dir content-root %))
                         (filter #(.exists ^File %)))
                    ["game.projectc" "game.arci" "game.arcd" "game.dmanifest" "game.public.der"])]
    (with-open [fs (get-zipfs ap1)]
      (run! #(add-to-zip! fs % (str "assets/" (.getName ^File %))) files))
    bundle-info))

(defn- strip-executables!
  "Strips symbols from the project executables."
  [bundle-info]
  (if (:strip-executable bundle-info)
    (let [striptools (into {}
                           (map #(let [[platform tool-name] %]
                                   ;; TODO: Move this to the config/data collection at start
                                   [platform (str (system/defold-unpack-path) "/" (name platform) "/bin/" tool-name)]))
                           (:platform-to-striptool bundle-info))]
      (assoc bundle-info
             :stripped-exes
             (into {} (map #(let [[platform ^File exe] %
                                  stripped (io/file (.getParentFile exe) (str (.getName exe) ".stripped"))
                                  _ (io/copy exe stripped)
                                  {:keys [out err exit] :as result} (shell/sh (get striptools platform) (.getCanonicalPath stripped))]
                              (when-not (zero? exit)
                                (throw (ex-info (str "Striptool error: " out err) result)))
                              [platform stripped]))
                   (:platform-to-exe bundle-info))))
    bundle-info))

(defn- package-executables!
  "Adds the project executables to the .ap1 file. Uses stripped versions if they
  exist."
  [bundle-info]
  (let [{:keys [platform-to-exe platform-to-lib stripped-exes ap1]} bundle-info
        exe-name (-> bundle-info :settings :exe-name) ;; TODO: why did I put the exe name here? Move it to the top level?
        exes (if stripped-exes
               stripped-exes
               platform-to-exe)]
    ;; TODO: more than one exe per platform?
    (with-open [fs (get-zipfs ap1)]
      (dorun (for [[platform exe] exes]
               (add-to-zip! fs exe (str "lib/" (platform-to-lib platform) "/lib" exe-name ".so"))))))
  bundle-info)

(defn create-archives!
  "Creates archive files from the compiled game resources."
  [bundle-info]
  (let [content-root (-> bundle-info :content-root)
        project-identifier (-> bundle-info :settings :project :title)
        root-node (ResourceNode. "<AnonymousRoot>", "<AnonymousRoot>")
        publisher (NullPublisher. (PublisherSettings.)) ;; TODO: Actually use any configured publisher settings.
        manifestbuilder (doto (ManifestBuilder.)
                          (.setDependencies root-node)
                          (.setResourceHashAlgorithm Manifest$HashAlgorithm/HASH_SHA1)
                          (.setSignatureHashAlgorithm Manifest$HashAlgorithm/HASH_SHA256)
                          (.setSignatureSignAlgorithm Manifest$SignAlgorithm/SIGN_RSA)
                          (.setProjectIdentifier ^String project-identifier))
        archiver (ArchiveBuilder. content-root manifestbuilder)]
    ;; TODO(Markus Gustavsson 2019-09-23): Finish this. Need archive files to be
    ;; able to bundle application correctly. The current bundling code has been
    ;; verified to work when first archiving using the complete Bob build
    ;; pipeline so this is the last missing piece. I wanted to convert the
    ;; archiving code to Clojure as well but did not have the time. Using the
    ;; existing Bob classes is a workaround that can be used as a first working
    ;; implementation.
    bundle-info))

(defn- sign-apk!
  "Signs the .ap1 contents with the apkc program, placing output in the .ap2
  file."
  [bundle-info]
  (let [{:keys [ap1 ap2 apkc-path key certificate]} bundle-info
        ap1-path (.getCanonicalPath ^File ap1)
        ap2-path (.getCanonicalPath ^File ap2)

        {:keys [out err exit] :as result}
        (if-not (and (empty? key) (empty? certificate))
          (shell/sh apkc-path (str "--in=" ap1-path) (str "--out=" ap2-path) (str "-cert=" certificate) (str "-key=" key))
          (shell/sh apkc-path (str "--in=" ap1-path) (str "--out=" ap2-path)))]
    (when-not (zero? exit)
      (throw (ex-info (str "apkc error: " out err) result))))
  bundle-info)

(defn- rezip-with-stored-assets!
  "Iterates through all the entries in .ap2 and puts them in .ap3 but skips
  compression on all files under the assets directory."
  ;; TODO: We should be able to skip this if we can just use store instead of
  ;; deflate when first putting stuff in assets?
  [bundle-info]
  (let [{:keys [^File ap2 ^File ap3]} bundle-info]
    (with-open [inzip (ZipFile. ap2) ;; NOTE: ZipInputStream freaks out with our files so we have to use ZipFile.
                out (ZipOutputStream. (FileOutputStream. ap3))]
      (doseq [^ZipEntry ie (iterator-seq (.iterator (.stream inzip)))]
        (let [asset (string/starts-with? (.getName ie) "assets")
              oe (ZipEntry. (.getName ie))
              in (.getInputStream inzip ie)]
          (if asset
            (let [baos (ByteArrayOutputStream.)
                  _ (io/copy in baos)
                  size (.getSize ie)
                  crc (CRC32.)
                  bytes (.toByteArray baos)]
              (when (pos? size)
                (.update crc bytes 0 size))
              (.setCrc oe (.getValue crc))
              (.setMethod oe ZipEntry/STORED)
              (.setCompressedSize oe size)
              (.setSize oe size)
              (.putNextEntry out oe)
              (.write out bytes))
            (do
              (.putNextEntry out oe)
              (io/copy in out)))
          (.closeEntry out)))))
  bundle-info)

(defn- zipalign!
  "Run the zipalign program on the .ap3 file, producing the .apk file."
  [bundle-info]
  (let [{:keys [zipalign-path ap3 apk]} bundle-info
        ap3-path (.getCanonicalPath ^File ap3)
        apk-path (.getCanonicalPath ^File apk)
        {:keys [out err exit] :as result} (shell/sh zipalign-path "-v" "4" ap3-path apk-path)]
    (when-not (zero? exit)
      (throw (ex-info (str "zipalign error: " out err) result))))
  bundle-info)

(defn bundle!
  "Bundles a game project for the Android platform. The project must be built
  before this function is called or inputs to the bundling will be missing."
  [defold-project bundle-dir options]
  (g/with-auto-evaluation-context evaluation-context
    (-> (collect-bundling-information defold-project bundle-dir options evaluation-context)
        (create-directories!)
        (merge-and-write-manifest! evaluation-context)
        (create-resource-directories!)
        (copy-icons!)
        (store-resources!)
        (find-android-assets)
        (create-rjava!)
        (package-dex-files!)
        (create-archives!)
        (package-archived-project-files!)
        (package-executables!)
        (sign-apk!)
        (rezip-with-stored-assets!)
        (zipalign!))))

