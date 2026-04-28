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

(ns editor.resource-unpacker-test
  (:require [clojure.java.io :as io]
            [clojure.string :as string]
            [clojure.test :refer :all]
            [editor.fs :as fs])
  (:import [com.defold.libs ResourceUnpacker ResourceUnpacker$NativeLibraryLoader]
           [com.dynamo.bob Platform]
           [com.jogamp.common.jvm JNILibLoaderBase]
           [com.jogamp.common.os DynamicLibraryBundle NativeLibrary]
           [com.jogamp.opengl GLProfile]
           [jogamp.opengl GLDrawableFactoryImpl]
           [java.nio.file Path]
           [java.util LinkedHashMap Map$Entry]
           [java.util.concurrent CountDownLatch TimeUnit]))

(defn- make-temp-library! ^Path [dir name]
  (let [file (io/file dir name)]
    (fs/create-parent-directories! file)
    (spit file "")
    (.toPath file)))

(defn- ordered-library-pairs [libraries]
  (mapv (fn [^Map$Entry entry]
          [(.getKey entry) (.. entry getValue getFileName toString)])
        (.entrySet libraries)))

(defn- linked-library-map [& entries]
  (let [libraries (LinkedHashMap.)]
    (doseq [[logical-name path] entries]
      (.put libraries logical-name path))
    libraries))

(defn- jogl-tool-native-library-paths []
  (let [profile (GLProfile/getDefault)
        factory ^GLDrawableFactoryImpl (GLDrawableFactoryImpl/getFactoryImpl profile)
        helper (.getGLDynamicLookupHelper factory 0 0)]
    (mapv (fn [^NativeLibrary lib]
            (-> (.getNativeLibraryPath lib)
                io/file
                .toPath
                .toAbsolutePath
                .normalize))
          (.getToolLibraries ^DynamicLibraryBundle helper))))

(defn- bundled-jogl-library-names [libraries]
  (->> (keys libraries)
       (filter #(or (= "gluegen_rt" %)
                    (string/starts-with? % "jogl_")
                    (string/starts-with? % "nativewindow_")))
       set))

(defn- loaded-jogl-library-names [candidate-names]
  (->> candidate-names
       (filter JNILibLoaderBase/isLoaded)
       set))

(deftest discover-bundled-native-libraries-filters-and-sorts-linux-libs
  (let [lib-dir (fs/create-temp-directory! "resource-unpacker-linux")]
    (make-temp-library! lib-dir "libzeta.so")
    (make-temp-library! lib-dir "libalpha.so")
    (make-temp-library! lib-dir "ignore.txt")
    ;; Discovery only scans the top-level host lib directory.
    (.mkdirs (io/file lib-dir "nested"))
    (make-temp-library! (io/file lib-dir "nested") "libhidden.so")
    (is (= [["alpha" "libalpha.so"]
            ["zeta" "libzeta.so"]]
           (ordered-library-pairs
             (ResourceUnpacker/discoverBundledNativeLibraries (.toPath lib-dir) Platform/X86_64Linux))))))

(deftest discover-bundled-native-libraries-accepts-macos-jnilibs
  (let [lib-dir (fs/create-temp-directory! "resource-unpacker-macos")]
    (make-temp-library! lib-dir "libbeta.dylib")
    (make-temp-library! lib-dir "libalpha.jnilib")
    (make-temp-library! lib-dir "libgamma.so")
    (is (= [["alpha" "libalpha.jnilib"]
            ["beta" "libbeta.dylib"]]
           (ordered-library-pairs
             (ResourceUnpacker/discoverBundledNativeLibraries (.toPath lib-dir) Platform/Arm64MacOS))))))

(deftest discover-bundled-native-libraries-fails-for-missing-empty-and-duplicate-libs
  (let [root-dir (fs/create-temp-directory! "resource-unpacker-errors")
        missing-lib-dir (.toPath (io/file root-dir "missing"))
        empty-lib-dir (fs/create-temp-directory! "resource-unpacker-empty")
        duplicate-lib-dir (fs/create-temp-directory! "resource-unpacker-duplicate")]
    (is (thrown-with-msg? java.io.IOException
                          #"does not exist"
                          (ResourceUnpacker/discoverBundledNativeLibraries missing-lib-dir Platform/X86_64Linux)))
    (is (thrown-with-msg? java.io.IOException
                          #"No bundled native libraries found"
                          (ResourceUnpacker/discoverBundledNativeLibraries (.toPath empty-lib-dir) Platform/X86_64Linux)))
    ;; Different macOS suffixes still collapse to the same logical name.
    (make-temp-library! duplicate-lib-dir "libparticle_shared.dylib")
    (make-temp-library! duplicate-lib-dir "libparticle_shared.jnilib")
    (is (thrown-with-msg? java.io.IOException
                          #"Duplicate bundled native library logical name 'particle_shared'"
                          (ResourceUnpacker/discoverBundledNativeLibraries (.toPath duplicate-lib-dir) Platform/Arm64MacOS)))))

(deftest preload-native-libraries-loads-all-paths-in-parallel
  (let [lib-dir (fs/create-temp-directory! "resource-unpacker-preload")
        alpha-path (make-temp-library! lib-dir "libalpha.so")
        beta-path (make-temp-library! lib-dir "libbeta.so")
        gamma-path (make-temp-library! lib-dir "libgamma.so")
        libraries (linked-library-map ["alpha" alpha-path]
                                      ["beta" beta-path]
                                      ["gamma" gamma-path])
        ;; The latch forces all worker threads to overlap before any load returns.
        started (CountDownLatch. 3)
        loaded-paths (atom [])
        thread-ids (atom #{})
        preloaded (ResourceUnpacker/preloadNativeLibraries
                    libraries
                    (reify ResourceUnpacker$NativeLibraryLoader
                      (^void load [_ ^Path library-path]
                        (swap! loaded-paths conj (.toString library-path))
                        (swap! thread-ids conj (.threadId (Thread/currentThread)))
                        (.countDown started)
                        (when-not (.await started 5 TimeUnit/SECONDS)
                          (throw (RuntimeException. (str "Timed out waiting for parallel preload of " library-path))))
                        nil)))]
    (is (= #{"alpha" "beta" "gamma"} (set (map first (ordered-library-pairs preloaded)))))
    (is (= #{(.toString alpha-path) (.toString beta-path) (.toString gamma-path)}
           (set @loaded-paths)))
    (is (= 3 (count @thread-ids)))))

(deftest preload-native-libraries-aggregates-failures
  (let [lib-dir (fs/create-temp-directory! "resource-unpacker-failures")
        alpha-path (make-temp-library! lib-dir "libalpha.so")
        beta-path (make-temp-library! lib-dir "libbeta.so")
        gamma-path (make-temp-library! lib-dir "libgamma.so")
        libraries (linked-library-map ["alpha" alpha-path]
                                      ["beta" beta-path]
                                      ["gamma" gamma-path])]
    (try
      ;; Only one library succeeds so we can assert that failures are aggregated.
      (ResourceUnpacker/preloadNativeLibraries
        libraries
        (reify ResourceUnpacker$NativeLibraryLoader
          (^void load [_ ^Path library-path]
            (when-not (= "libgamma.so" (.. library-path getFileName toString))
              (throw (RuntimeException. (str "boom:" library-path))))
            nil)))
      (is false "Expected aggregated native preload failure")
      (catch IllegalStateException e
        (let [message (.getMessage e)]
          (is (.contains message (.toString alpha-path)))
          (is (.contains message (.toString beta-path)))
          (is (not (.contains message (.toString gamma-path))))
          (is (= 2 (count (.getSuppressed e)))))))))

(deftest unpack-resources-preloads-host-bundled-native-libraries
  (ResourceUnpacker/unpackResources)
  (let [platform (Platform/getHostPlatform)
        unpack-path (System/getProperty ResourceUnpacker/DEFOLD_UNPACK_PATH_KEY)
        lib-dir (.toPath (io/file unpack-path (str (.getPair platform) "/lib")))
        discovered (ResourceUnpacker/discoverBundledNativeLibraries lib-dir platform)
        preloaded (ResourceUnpacker/getPreloadedLibraries)]
    (is (= (ordered-library-pairs discovered)
           (ordered-library-pairs preloaded)))
    (doseq [^Map$Entry entry (.entrySet discovered)]
      (is (= (.getValue entry)
             (ResourceUnpacker/getPreloadedLibraryPath (.getKey entry)))))
    (doseq [logical-name ["particle_shared" "mouse_capture_shared"]]
      (when-let [^Path library-path (.get preloaded logical-name)]
        (is (.startsWith library-path lib-dir))
        (is (= library-path
               (ResourceUnpacker/getPreloadedLibraryPath logical-name)))))))

(deftest unpack-resources-supports-jogl-glprofile-init-singleton
  (ResourceUnpacker/unpackResources)
  ;; Match the startup path in Start.java: preload first, then let JOGL initialize itself.
  (GLProfile/initSingleton)
  (is (GLProfile/isInitialized))
  (let [platform (Platform/getHostPlatform)
        unpack-path (System/getProperty ResourceUnpacker/DEFOLD_UNPACK_PATH_KEY)
        lib-dir (-> (io/file unpack-path (str (.getPair platform) "/lib"))
                    .toPath
                    .toAbsolutePath
                    .normalize)
        preloaded (ResourceUnpacker/getPreloadedLibraries)
        bundled-jogl-library-names (bundled-jogl-library-names preloaded)
        loaded-jogl-library-names (loaded-jogl-library-names bundled-jogl-library-names)
        tool-library-paths (jogl-tool-native-library-paths)]
    ;; JOGL exposes exact paths for its GL tool bundle and logical names for the other
    ;; native libraries it requests to load. Since unpackResources pins java.library.path
    ;; to the unpacked lib directory, these names resolve back to the bundled copies.
    (is (= (.toString lib-dir) (System/getProperty "java.library.path")))
    (is (seq tool-library-paths))
    (doseq [^Path tool-library-path tool-library-paths]
      (is (.isAbsolute tool-library-path)))
    (is (contains? loaded-jogl-library-names "gluegen_rt"))
    (is (some loaded-jogl-library-names ["jogl_desktop" "jogl_mobile"]))
    (is (seq (filter #(string/starts-with? % "nativewindow_") loaded-jogl-library-names)))
    (is (every? #(contains? bundled-jogl-library-names %) loaded-jogl-library-names))))
