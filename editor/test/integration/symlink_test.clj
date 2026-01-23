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
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.defold-project :as project]
            [editor.resource :as resource]
            [editor.workspace :as workspace]
            [integration.test-util :as test-util]
            [support.test-support :as test-support]
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

(deftest directory-symlink-test
  (let [referencing-project-path (test-util/make-temp-project-copy! "test/resources/empty_project")
        referenced-project-path (test-util/make-temp-project-copy! "test/resources/symlinks_referenced")]
    (with-open [_referencing-project-deleter (test-util/make-directory-deleter referencing-project-path)
                _referenced-project-deleter (test-util/make-directory-deleter referenced-project-path)]

      (let [referencing-directory (path/of referencing-project-path "linked_directory")
            referenced-directory (path/of referenced-project-path "linked_directory")]
        (path/create-symbolic-link! referencing-directory referenced-directory)

        (test-support/with-clean-system
          (let [{:keys [main-collection]} (setup-symlink-test-project! world referencing-project-path "/linked_directory/referenced.go")]
            (is (not (g/error? (g/node-value main-collection :build-targets))))
            (is (not (g/error? (g/node-value main-collection :scene))))
            (is (not (g/error? (g/node-value main-collection :node-outline))))))))))

(deftest broken-directory-symlink-test
  (let [referencing-project-path (test-util/make-temp-project-copy! "test/resources/empty_project")
        referenced-project-path (test-util/make-temp-project-copy! "test/resources/symlinks_referenced")]
    (with-open [_referencing-project-deleter (test-util/make-directory-deleter referencing-project-path)
                _referenced-project-deleter (test-util/make-directory-deleter referenced-project-path)]

      (let [referencing-directory (path/of referencing-project-path "linked_directory")
            referenced-directory (path/of referenced-project-path "linked_directory")
            referenced-directory-renamed (path/resolve-sibling referenced-directory "linked_directory_renamed")]
        (path/create-symbolic-link! referencing-directory referenced-directory)
        (path/move! referenced-directory referenced-directory-renamed)

        (test-support/with-clean-system
          (let [{:keys [main-collection project workspace]} (setup-symlink-test-project! world referencing-project-path "/linked_directory/referenced.go")]

            (testing "Broken symlink appear as defective file."
              (let [referencing-directory-resource (path->resource workspace referencing-directory)]
                (when (is (resource/file-resource? referencing-directory-resource))
                  (is (= :file (resource/source-type referencing-directory-resource)))
                  (is (resource/placeholder-resource-type? (resource/resource-type referencing-directory-resource)))
                  (let [referencing-directory-resource-node (project/get-resource-node project referencing-directory-resource)]
                    (when (is (some? referencing-directory-resource-node))
                      (is (g/defective? referencing-directory-resource-node)))))))

            (is (g/error? (g/node-value main-collection :build-targets)))
            (is (g/error? (g/node-value main-collection :scene)))
            (is (not (g/error? (g/node-value main-collection :node-outline))))

            (testing "Fixing broken symlink."
              (path/move! referenced-directory-renamed referenced-directory)
              (workspace/resource-sync! workspace)
              (is (not (g/error? (g/node-value main-collection :build-targets))))
              (is (not (g/error? (g/node-value main-collection :scene))))
              (is (not (g/error? (g/node-value main-collection :node-outline)))))))))))

(deftest file-symlink-test
  (let [referencing-project-path (test-util/make-temp-project-copy! "test/resources/empty_project")
        referenced-project-path (test-util/make-temp-project-copy! "test/resources/symlinks_referenced")]
    (with-open [_referencing-project-deleter (test-util/make-directory-deleter referencing-project-path)
                _referenced-project-deleter (test-util/make-directory-deleter referenced-project-path)]

      (let [referencing-directory (path/of referencing-project-path "linked_files")
            referenced-directory (path/of referenced-project-path "linked_files")]
        (->> (path/tree-walker referenced-directory)
             (run! (fn [referenced-file]
                     (let [referencing-file (path/reroot referenced-file referenced-directory referencing-directory)]
                       (path/create-parent-directories! referencing-file)
                       (path/create-symbolic-link! referencing-file referenced-file)))))

        (test-support/with-clean-system
          (let [{:keys [main-collection]} (setup-symlink-test-project! world referencing-project-path "/linked_files/referenced.go")]
            (is (not (g/error? (g/node-value main-collection :build-targets))))
            (is (not (g/error? (g/node-value main-collection :scene))))
            (is (not (g/error? (g/node-value main-collection :node-outline))))))))))

(deftest broken-file-symlink-test
  (let [referencing-project-path (test-util/make-temp-project-copy! "test/resources/empty_project")
        referenced-project-path (test-util/make-temp-project-copy! "test/resources/symlinks_referenced")]
    (with-open [_referencing-project-deleter (test-util/make-directory-deleter referencing-project-path)
                _referenced-project-deleter (test-util/make-directory-deleter referenced-project-path)]

      (let [referencing-directory (path/of referencing-project-path "linked_files")
            referenced-directory (path/of referenced-project-path "linked_files")

            referencing-file->referenced-file
            (reduce
              (fn [referencing-file->referenced-file referenced-file]
                (let [referencing-file (path/reroot referenced-file referenced-directory referencing-directory)]
                  (path/create-parent-directories! referencing-file)
                  (path/create-symbolic-link! referencing-file referenced-file)
                  (assoc referencing-file->referenced-file referencing-file referenced-file)))
              {}
              (path/tree-walker referenced-directory))

            referenced-file->renamed-file
            (reduce-kv
              (fn [referenced-file->renamed-file _referencing-file referenced-file]
                (let [renamed-filename (str "renamed_" (path/leaf referenced-file))
                      renamed-file (path/resolve-sibling referenced-file renamed-filename)]
                  (path/move! referenced-file renamed-file)
                  (assoc referenced-file->renamed-file referenced-file renamed-file)))
              {}
              referencing-file->referenced-file)]

        (test-support/with-clean-system
          (let [{:keys [main-collection project workspace]} (setup-symlink-test-project! world referencing-project-path "/linked_files/referenced.go")]

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
                            (when (and (is (some? referencing-file-resource-node))
                                       (resource/stateful-resource-type? resource-type))
                              (is (g/defective? referencing-file-resource-node)))))))))))

            (is (g/error? (g/node-value main-collection :build-targets)))
            (is (g/error? (g/node-value main-collection :scene)))
            (is (not (g/error? (g/node-value main-collection :node-outline))))

            (testing "Fixing broken symlink."
              (reduce-kv
                (fn [_ referenced-file renamed-file]
                  (path/move! renamed-file referenced-file))
                nil
                referenced-file->renamed-file)

              (workspace/resource-sync! workspace)
              (is (not (g/error? (g/node-value main-collection :build-targets))))
              (is (not (g/error? (g/node-value main-collection :scene))))
              (is (not (g/error? (g/node-value main-collection :node-outline)))))))))))
