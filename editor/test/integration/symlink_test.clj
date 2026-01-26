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

(ns integration.symlink-test
  (:require [clojure.java.io :as io]
            [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.defold-project :as project]
            [editor.fs :as fs]
            [editor.resource :as resource]
            [editor.workspace :as workspace]
            [integration.test-util :as test-util]
            [support.test-support :as test-support]
            [util.coll :as coll]
            [util.path :as path]))

(set! *warn-on-reflection* true)

(defn- path->proj-path
  (^String [workspace path]
   (path->proj-path (g/now) workspace path))
  (^String [basis workspace path]
   (let [project-directory-path (path/as-path (workspace/project-directory basis workspace))
         relative-path (path/relativize project-directory-path path)]
     (if (path/starts-with? relative-path "..")
       (throw (ex-info "The path is not located below the project directory."
                       {:path path
                        :project-directory-path project-directory-path}))
       (str \/ relative-path)))))

(defn path->resource
  ([workspace path]
   (g/with-auto-evaluation-context evaluation-context
     (path->resource workspace path evaluation-context)))
  ([workspace path evaluation-context]
   (let [basis (:basis evaluation-context)
         proj-path (path->proj-path basis workspace path)]
     (workspace/find-resource workspace proj-path evaluation-context))))

(defn- setup-symlink-test-project!
  [graph-id project-path game-object-proj-path]
  {:pre [(string? game-object-proj-path)]}
  (let [workspace (test-util/setup-workspace! graph-id project-path)]
    (test-util/write-file-resource! workspace
      "/main/main.collection"
      {:name "main"
       :instances
       [{:id "referenced_go"
         :prototype game-object-proj-path}]})
    (workspace/resource-sync! workspace)
    (let [project (test-util/setup-project! workspace)
          main-collection (test-util/resource-node project "/main/main.collection")]
      {:workspace workspace
       :project project
       :main-collection main-collection})))

(deftest stateless-directory-symlink-test
  (let [referencing-project-path (test-util/make-temp-project-copy! "test/resources/empty_project")
        referenced-project-path (test-util/make-temp-project-copy! "test/resources/symlinks_referenced")]
    (with-open [_referencing-project-deleter (test-util/make-directory-deleter referencing-project-path)
                _referenced-project-deleter (test-util/make-directory-deleter referenced-project-path)]

      (let [referencing-directory (path/of referencing-project-path "stateless_directory")
            referenced-directory (path/of referenced-project-path "stateless_directory")]
        (path/create-symbolic-link! referencing-directory referenced-directory)

        (fs/copy-directory!
          (io/file referenced-project-path "stateful_directory")
          (io/file referencing-project-path "stateful_directory"))

        (test-support/with-clean-system
          (let [{:keys [main-collection]} (setup-symlink-test-project! world referencing-project-path "/stateful_directory/referenced.go")]
            (is (not (g/error? (test-util/build-error! main-collection))))
            (is (not (g/error? (g/node-value main-collection :scene))))
            (is (not (g/error? (g/node-value main-collection :node-outline))))))))))

(deftest broken-stateless-directory-symlink-test
  (let [referencing-project-path (test-util/make-temp-project-copy! "test/resources/empty_project")
        referenced-project-path (test-util/make-temp-project-copy! "test/resources/symlinks_referenced")]
    (with-open [_referencing-project-deleter (test-util/make-directory-deleter referencing-project-path)
                _referenced-project-deleter (test-util/make-directory-deleter referenced-project-path)]

      (let [referencing-directory (path/of referencing-project-path "stateless_directory")
            referenced-directory (path/of referenced-project-path "stateless_directory")
            referenced-directory-renamed (path/resolve-sibling referenced-directory "stateless_directory_renamed")]
        (path/create-symbolic-link! referencing-directory referenced-directory)
        (path/move! referenced-directory referenced-directory-renamed)

        (fs/copy-directory!
          (io/file referenced-project-path "stateful_directory")
          (io/file referencing-project-path "stateful_directory"))

        (test-support/with-clean-system
          (let [{:keys [main-collection project workspace]} (setup-symlink-test-project! world referencing-project-path "/stateful_directory/referenced.go")]

            (testing "Broken symlink appears as defective file."
              (let [referencing-directory-resource (path->resource workspace referencing-directory)]
                (when (is (resource/file-resource? referencing-directory-resource))
                  (is (= :file (resource/source-type referencing-directory-resource)))
                  (is (resource/placeholder-resource-type? (resource/resource-type referencing-directory-resource)))
                  (let [referencing-directory-resource-node (project/get-resource-node project referencing-directory-resource)]
                    (when (is (some? referencing-directory-resource-node))
                      (is (g/defective? referencing-directory-resource-node)))))))

            (is (g/error? (test-util/build-error! main-collection)))
            (is (g/error? (g/node-value main-collection :scene)))
            (is (not (g/error? (g/node-value main-collection :node-outline))))

            (testing "Fixing broken symlink."
              (path/move! referenced-directory-renamed referenced-directory)
              (workspace/resource-sync! workspace)
              (is (not (g/error? (test-util/build-error! main-collection))))
              (is (not (g/error? (g/node-value main-collection :scene))))
              (is (not (g/error? (g/node-value main-collection :node-outline)))))))))))

(deftest stateful-directory-symlink-test
  (let [referencing-project-path (test-util/make-temp-project-copy! "test/resources/empty_project")
        referenced-project-path (test-util/make-temp-project-copy! "test/resources/symlinks_referenced")]
    (with-open [_referencing-project-deleter (test-util/make-directory-deleter referencing-project-path)
                _referenced-project-deleter (test-util/make-directory-deleter referenced-project-path)]

      (let [referencing-directory (path/of referencing-project-path "stateful_directory")
            referenced-directory (path/of referenced-project-path "stateful_directory")]
        (path/create-symbolic-link! referencing-directory referenced-directory)

        (fs/copy-directory!
          (io/file referenced-project-path "stateless_directory")
          (io/file referencing-project-path "stateless_directory"))

        (test-support/with-clean-system
          (let [{:keys [main-collection]} (setup-symlink-test-project! world referencing-project-path "/stateful_directory/referenced.go")]
            (is (not (g/error? (test-util/build-error! main-collection))))
            (is (not (g/error? (g/node-value main-collection :scene))))
            (is (not (g/error? (g/node-value main-collection :node-outline))))))))))

(deftest broken-stateful-directory-symlink-test
  (let [referencing-project-path (test-util/make-temp-project-copy! "test/resources/empty_project")
        referenced-project-path (test-util/make-temp-project-copy! "test/resources/symlinks_referenced")]
    (with-open [_referencing-project-deleter (test-util/make-directory-deleter referencing-project-path)
                _referenced-project-deleter (test-util/make-directory-deleter referenced-project-path)]

      (let [referencing-directory (path/of referencing-project-path "stateful_directory")
            referenced-directory (path/of referenced-project-path "stateful_directory")
            referenced-directory-renamed (path/resolve-sibling referenced-directory "stateful_directory_renamed")]
        (path/create-symbolic-link! referencing-directory referenced-directory)
        (path/move! referenced-directory referenced-directory-renamed)

        (fs/copy-directory!
          (io/file referenced-project-path "stateless_directory")
          (io/file referencing-project-path "stateless_directory"))

        (test-support/with-clean-system
          (let [{:keys [main-collection project workspace]} (setup-symlink-test-project! world referencing-project-path "/stateful_directory/referenced.go")]

            (testing "Broken symlink appears as defective file."
              (let [referencing-directory-resource (path->resource workspace referencing-directory)]
                (when (is (resource/file-resource? referencing-directory-resource))
                  (is (= :file (resource/source-type referencing-directory-resource)))
                  (is (resource/placeholder-resource-type? (resource/resource-type referencing-directory-resource)))
                  (let [referencing-directory-resource-node (project/get-resource-node project referencing-directory-resource)]
                    (when (is (some? referencing-directory-resource-node))
                      (is (g/defective? referencing-directory-resource-node)))))))

            (is (g/error? (test-util/build-error! main-collection)))
            (is (g/error? (g/node-value main-collection :scene)))
            (is (not (g/error? (g/node-value main-collection :node-outline))))

            (testing "Fixing broken symlink."
              (path/move! referenced-directory-renamed referenced-directory)
              (workspace/resource-sync! workspace)
              (is (not (g/error? (test-util/build-error! main-collection))))
              (is (not (g/error? (g/node-value main-collection :scene))))
              (is (not (g/error? (g/node-value main-collection :node-outline)))))))))))

(deftest stateless-file-symlink-test
  (let [referencing-project-path (test-util/make-temp-project-copy! "test/resources/empty_project")
        referenced-project-path (test-util/make-temp-project-copy! "test/resources/symlinks_referenced")]
    (with-open [_referencing-project-deleter (test-util/make-directory-deleter referencing-project-path)
                _referenced-project-deleter (test-util/make-directory-deleter referenced-project-path)]
      (let [referencing-directory (path/of referencing-project-path "stateless_files")
            referenced-directory (path/of referenced-project-path "stateless_files")]
        (coll/transpire! (path/tree-walker referenced-directory)
          (remove path/hidden?)
          (fn [referenced-file]
            (let [referencing-file (path/reroot referenced-file referenced-directory referencing-directory)]
              (path/create-parent-directories! referencing-file)
              (path/create-symbolic-link! referencing-file referenced-file))))

        (fs/copy-directory!
          (io/file referenced-project-path "stateful_files")
          (io/file referencing-project-path "stateful_files"))

        (test-support/with-clean-system
          (let [{:keys [main-collection]} (setup-symlink-test-project! world referencing-project-path "/stateful_files/referenced.go")]
            (is (not (g/error? (test-util/build-error! main-collection))))
            (is (not (g/error? (g/node-value main-collection :scene))))
            (is (not (g/error? (g/node-value main-collection :node-outline))))))))))

(deftest broken-stateless-file-symlink-test
  (let [referencing-project-path (test-util/make-temp-project-copy! "test/resources/empty_project")
        referenced-project-path (test-util/make-temp-project-copy! "test/resources/symlinks_referenced")]
    (with-open [_referencing-project-deleter (test-util/make-directory-deleter referencing-project-path)
                _referenced-project-deleter (test-util/make-directory-deleter referenced-project-path)]

      (let [referencing-directory (path/of referencing-project-path "stateless_files")
            referenced-directory (path/of referenced-project-path "stateless_files")

            referencing-file->referenced-file
            (coll/transmute (path/tree-walker referenced-directory) {}
              (remove path/hidden?)
              (fn [referencing-file->referenced-file referenced-file]
                (let [referencing-file (path/reroot referenced-file referenced-directory referencing-directory)]
                  (path/create-parent-directories! referencing-file)
                  (path/create-symbolic-link! referencing-file referenced-file)
                  (assoc referencing-file->referenced-file referencing-file referenced-file))))

            referenced-file->renamed-file
            (coll/transmute-kv referencing-file->referenced-file {}
              (fn [referenced-file->renamed-file _referencing-file referenced-file]
                (let [renamed-filename (str "renamed_" (path/leaf referenced-file))
                      renamed-file (path/resolve-sibling referenced-file renamed-filename)]
                  (path/move! referenced-file renamed-file)
                  (assoc referenced-file->renamed-file referenced-file renamed-file))))]

        (fs/copy-directory!
          (io/file referenced-project-path "stateful_files")
          (io/file referencing-project-path "stateful_files"))

        (test-support/with-clean-system
          (let [{:keys [main-collection project workspace]} (setup-symlink-test-project! world referencing-project-path "/stateful_files/referenced.go")]

            (testing "Broken symlinks appear as files of their respective resource-type."
              (let [ext->resource-type (workspace/get-resource-type-map workspace)]
                (doseq [referencing-file (keys referencing-file->referenced-file)]
                  (let [referencing-file-resource (path->resource workspace referencing-file)]
                    (when (and (is (resource/file-resource? referencing-file-resource))
                               (is (= :file (resource/source-type referencing-file-resource))))
                      (let [ext (resource/type-ext referencing-file-resource)
                            resource-type (ext->resource-type ext)]
                        (when (is (= resource-type (resource/resource-type referencing-file-resource)))
                          (let [referencing-file-resource-node (project/get-resource-node project referencing-file-resource)]
                            (is (some? referencing-file-resource-node))))))))))

            (is (g/error? (test-util/build-error! main-collection)))
            (is (g/error? (g/node-value main-collection :scene)))
            (is (not (g/error? (g/node-value main-collection :node-outline))))

            (testing "Fixing broken symlinks."
              (coll/transpire! referenced-file->renamed-file
                (fn [[referenced-file renamed-file]]
                  (path/move! renamed-file referenced-file)))

              (workspace/resource-sync! workspace)
              (is (not (g/error? (test-util/build-error! main-collection))))
              (is (not (g/error? (g/node-value main-collection :scene))))
              (is (not (g/error? (g/node-value main-collection :node-outline)))))))))))

(deftest stateful-file-symlink-test
  (let [referencing-project-path (test-util/make-temp-project-copy! "test/resources/empty_project")
        referenced-project-path (test-util/make-temp-project-copy! "test/resources/symlinks_referenced")]
    (with-open [_referencing-project-deleter (test-util/make-directory-deleter referencing-project-path)
                _referenced-project-deleter (test-util/make-directory-deleter referenced-project-path)]
      (let [referencing-directory (path/of referencing-project-path "stateful_files")
            referenced-directory (path/of referenced-project-path "stateful_files")]
        (coll/transpire! (path/tree-walker referenced-directory)
          (remove path/hidden?)
          (fn [referenced-file]
            (let [referencing-file (path/reroot referenced-file referenced-directory referencing-directory)]
              (path/create-parent-directories! referencing-file)
              (path/create-symbolic-link! referencing-file referenced-file))))

        (fs/copy-directory!
          (io/file referenced-project-path "stateless_files")
          (io/file referencing-project-path "stateless_files"))

        (test-support/with-clean-system
          (let [{:keys [main-collection]} (setup-symlink-test-project! world referencing-project-path "/stateful_files/referenced.go")]
            (is (not (g/error? (test-util/build-error! main-collection))))
            (is (not (g/error? (g/node-value main-collection :scene))))
            (is (not (g/error? (g/node-value main-collection :node-outline))))))))))

(deftest broken-stateful-file-symlink-test
  (let [referencing-project-path (test-util/make-temp-project-copy! "test/resources/empty_project")
        referenced-project-path (test-util/make-temp-project-copy! "test/resources/symlinks_referenced")]
    (with-open [_referencing-project-deleter (test-util/make-directory-deleter referencing-project-path)
                _referenced-project-deleter (test-util/make-directory-deleter referenced-project-path)]

      (let [referencing-directory (path/of referencing-project-path "stateful_files")
            referenced-directory (path/of referenced-project-path "stateful_files")

            referencing-file->referenced-file
            (coll/transmute (path/tree-walker referenced-directory) {}
              (remove path/hidden?)
              (fn [referencing-file->referenced-file referenced-file]
                (let [referencing-file (path/reroot referenced-file referenced-directory referencing-directory)]
                  (path/create-parent-directories! referencing-file)
                  (path/create-symbolic-link! referencing-file referenced-file)
                  (assoc referencing-file->referenced-file referencing-file referenced-file))))

            referenced-file->renamed-file
            (coll/transmute-kv referencing-file->referenced-file {}
              (fn [referenced-file->renamed-file _referencing-file referenced-file]
                (let [renamed-filename (str "renamed_" (path/leaf referenced-file))
                      renamed-file (path/resolve-sibling referenced-file renamed-filename)]
                  (path/move! referenced-file renamed-file)
                  (assoc referenced-file->renamed-file referenced-file renamed-file))))]

        (fs/copy-directory!
          (io/file referenced-project-path "stateless_files")
          (io/file referencing-project-path "stateless_files"))

        (test-support/with-clean-system
          (let [{:keys [main-collection project workspace]} (setup-symlink-test-project! world referencing-project-path "/stateful_files/referenced.go")]

            (testing "Broken symlinks appear as defective files of their respective resource-type."
              (let [ext->resource-type (workspace/get-resource-type-map workspace)]
                (doseq [referencing-file (keys referencing-file->referenced-file)]
                  (let [referencing-file-resource (path->resource workspace referencing-file)]
                    (when (and (is (resource/file-resource? referencing-file-resource))
                               (is (= :file (resource/source-type referencing-file-resource))))
                      (let [ext (resource/type-ext referencing-file-resource)
                            resource-type (ext->resource-type ext)]
                        (when (is (= resource-type (resource/resource-type referencing-file-resource)))
                          (let [referencing-file-resource-node (project/get-resource-node project referencing-file-resource)]
                            (when (is (some? referencing-file-resource-node))
                              (is (g/defective? referencing-file-resource-node)))))))))))

            (is (g/error? (test-util/build-error! main-collection)))
            (is (g/error? (g/node-value main-collection :scene)))
            (is (not (g/error? (g/node-value main-collection :node-outline))))

            (testing "Fixing broken symlinks."
              (coll/transpire! referenced-file->renamed-file
                (fn [[referenced-file renamed-file]]
                  (path/move! renamed-file referenced-file)))

              (workspace/resource-sync! workspace)
              (is (not (g/error? (test-util/build-error! main-collection))))
              (is (not (g/error? (g/node-value main-collection :scene))))
              (is (not (g/error? (g/node-value main-collection :node-outline)))))))))))
