(ns editor.bundle-android-test
  (:require [clojure.java.io :as io]
            [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.fs :as fs]
            [editor.bundle-android :as ba]
            [integration.test-util :as tu]
            ;;[potemkin.namespaces :as namespaces]
            )
  (:import [com.android.manifmerger MergingReport MergingReport$MergedManifestKind]
           [java.io File]
           [java.nio.file Files]
           [java.nio.file.attribute FileAttribute]))

(def project-path "test/resources/bundle_project/BundleTest")

;; This fails compilation with a NPE. I don't know why. Works in graph.clj.
#_(namespaces/import-vars
    [editor.bundle-android
     collect-bundling-information
     create-directories!
     merge-android-manifests!
     create-resource-directories!
     copy-icons!
     get-bundle-and-ext-resources
     bundle!])

(defmacro force-require
  [var-names]
  (let [defs (for [n var-names
                   :let [s (symbol (str "ba/" n))]]
               `(def ~n (var-get (var ~s))))]
    `(do ~@defs)))

(force-require
  [collect-bundling-information
   create-directories!
   merge-android-manifests!
   create-resource-directories!
   copy-icons!
   get-bundle-and-ext-resources
   store-resources!
   bundle!])

(defn make-temp-dir
  []
  (.toFile (Files/createTempDirectory "bundle" (into-array FileAttribute []))))

(defmacro with-bundle-dir
  "Binds `bundle-dir` to a File object represting a temporary directory. The
  directory will be deleted when this forms exits."
  [& forms]
  `(let [~'bundle-dir (make-temp-dir)]
     (try
       ~@forms
       (finally
         (fs/delete-directory! ~'bundle-dir {:fail :silently})))))

(defn collect-info
  [project bundle-dir]
  (g/with-auto-evaluation-context ec
    (collect-bundling-information project bundle-dir {} ec)))

(defmacro with-project-and-bundle-info
  "Expects `bundle-dir` to be bound to a File object naming a directory to use for
  bundling. Binds `project` to the loaded defold project and `bundle-info` to
  the data structure collected using
  `editor.bundle-android/collect-bundling-information`."
  [& forms]
  `(tu/with-loaded-project ~project-path
     (let [~'bundle-info (collect-info ~'project ~'bundle-dir)]
       ~@forms)))

(deftest create-directories
  (testing "All required directories are created"
    (with-bundle-dir
      (with-project-and-bundle-info
        (create-directories! bundle-info)
        (create-resource-directories! bundle-info)
        (let [^File res-dir (:res-dir bundle-info)
              ^File app-dir (:app-dir bundle-info)
              ^File tmp-res-dir (:tmp-res-dir bundle-info)
              ^File tmp-extensions-dir (:tmp-extensions-dir bundle-info)
              ^File rjava-dir (:rjava-dir bundle-info)
              platform-to-lib (:platform-to-lib bundle-info)
              lib-dirs-exist (reduce #(and %1 (.exists ^File %2))
                                     true
                                     (map #(io/file app-dir "libs" %)
                                          (vals platform-to-lib)))
              resource-dirs-exist (reduce #(and %1 (.exists ^File %2))
                                          true
                                          (map #(io/file res-dir %)
                                               ["drawable"
                                                "drawable-ldpi"
                                                "drawable-mdpi"
                                                "drawable-hdpi"
                                                "drawable-xhdpi"
                                                "drawable-xxhdpi"
                                                "drawable-xxxhdpi"]))]
          (is (.exists res-dir))
          (is (.exists app-dir))
          (is (.exists tmp-res-dir))
          (is (.exists tmp-extensions-dir))
          (is (.exists rjava-dir))
          (is lib-dirs-exist)
          (is resource-dirs-exist))))))

(def android-main-manifest
   "<?xml version=\"1.0\" encoding=\"utf-8\"?>
   <manifest xmlns:android=\"http://schemas.android.com/apk/res/android\"
           package=\"com.defold.testmerge\"
           android:versionCode=\"14\"
           android:versionName=\"1.0\"
           android:installLocation=\"auto\">
       <uses-feature android:required=\"true\" android:glEsVersion=\"0x00020000\" />
       <uses-sdk android:minSdkVersion=\"9\" android:targetSdkVersion=\"26\" />
       <application android:label=\"Test Project\" android:hasCode=\"true\">
       </application>
       <uses-permission android:name=\"android.permission.VIBRATE\" />
   </manifest>")

(def android-lib-manifest
  "<?xml version=\"1.0\" encoding=\"utf-8\"?>
  <manifest xmlns:android=\"http://schemas.android.com/apk/res/android\" package=\"com.defold.testmerge\">
      <uses-feature android:required=\"true\" android:glEsVersion=\"0x00030000\" />
      <application>
          <meta-data android:name=\"com.facebook.sdk.ApplicationName\"
              android:value=\"Test Project\" />
          <activity android:name=\"com.facebook.FacebookActivity\"
            android:theme=\"@android:style/Theme.Translucent.NoTitleBar\"
            android:configChanges=\"keyboard|keyboardHidden|screenLayout|screenSize|orientation\"
            android:label=\"Test Project\" />
      </application>
  </manifest>")

(def android-merged-manifest
  ;; NOTE: This is whitespace sensitive. The test will fail if this is changed
  ;; to be properly aligned with the first line.
  "<?xml version=\"1.0\" encoding=\"utf-8\"?>
<manifest xmlns:android=\"http://schemas.android.com/apk/res/android\"
    package=\"com.defold.testmerge\"
    android:installLocation=\"auto\"
    android:versionCode=\"14\"
    android:versionName=\"1.0\" >

    <uses-sdk
        android:minSdkVersion=\"9\"
        android:targetSdkVersion=\"26\" />

    <uses-permission android:name=\"android.permission.VIBRATE\" />

    <uses-feature
        android:glEsVersion=\"0x00030000\"
        android:required=\"true\" />

    <uses-permission android:name=\"android.permission.WRITE_EXTERNAL_STORAGE\" />
    <uses-permission android:name=\"android.permission.READ_PHONE_STATE\" />
    <uses-permission android:name=\"android.permission.READ_EXTERNAL_STORAGE\" />

    <application
        android:hasCode=\"true\"
        android:label=\"Test Project\" >
        <meta-data
            android:name=\"com.facebook.sdk.ApplicationName\"
            android:value=\"Test Project\" />

        <activity
            android:name=\"com.facebook.FacebookActivity\"
            android:configChanges=\"keyboard|keyboardHidden|screenLayout|screenSize|orientation\"
            android:label=\"Test Project\"
            android:theme=\"@android:style/Theme.Translucent.NoTitleBar\" />
    </application>

</manifest>")

(deftest merge-manifests
  (testing "Merging Android manifests"
    (with-bundle-dir
      (with-project-and-bundle-info
        (let [main (io/file bundle-dir "main_manifest.xml")
              lib (io/file bundle-dir "lib_manifest.xml")
              _ (do (spit main android-main-manifest)
                    (spit lib android-lib-manifest))
              report (merge-android-manifests! main [lib])]
          (is (= android-merged-manifest
                 (.getMergedDocument ^MergingReport report MergingReport$MergedManifestKind/MERGED))))))))

(deftest copy-icons
  (testing "See if the icons get created"
    (with-bundle-dir
      (with-project-and-bundle-info
        (create-directories! bundle-info)
        (create-resource-directories! bundle-info)
        (copy-icons! bundle-info)
        (let [res-dir (:res-dir bundle-info)
              all-files-exist? (fn [fcoll] (reduce #(and %1 (.exists ^File %2)) true fcoll))
              drawable-folders ["drawable-xxxhdpi" "drawable-xxhdpi" "drawable-xhdpi" "drawable-hdpi" "drawable-mdpi" "drawable-ldpi"]
              icons-exist (and (all-files-exist?
                                 (map #(io/file res-dir % "icon.png")
                                      drawable-folders))
                               (.exists (io/file res-dir "drawable-xxxhdpi" "push_icon_small.png"))
                               (.exists (io/file res-dir "drawable-xxxhdpi" "push_icon_large.png")))]
          (is icons-exist))))))

(deftest get-android-bundle-resources
  (testing "Test different bundle resource and exclusion combinations"
    ;; TODO: Add more resources and exclusions to test different scenarios
    (with-bundle-dir
      (let [bundle-info (with-project-and-bundle-info
                          (get-bundle-and-ext-resources bundle-info))
            resources (:resources bundle-info)]
        (is (vector? resources))
        (is (= 2 (count resources)))))))

(deftest store-resources
  (testing "Store android resources in a place where the Android tools can see them."
    (with-bundle-dir
      (let [bundle-info (with-project-and-bundle-info
                          (store-resources! (get-bundle-and-ext-resources bundle-info)))
            check-me (io/file (-> bundle-info :tmp-extensions-dir) "native_ext/res/android/res/drawable-xxxhdpi/android_yeah.png")]
        (is (.exists check-me))))))

;; NOTE: Disabled because it is slow and we already tested the functions
;; individually. Re-enable if you hate fast tests.
#_(deftest bundle
    (testing "Run the whole bundling from start to finish"
      ;; This does not check anything explicitly. Just makes sure there are no
      ;; exceptions or other catastrophic stuff in the dark cracks between
      ;; individually tested functions.
      (with-bundle-dir
        (with-project-and-bundle-info
          (bundle! project bundle-dir {})))))
