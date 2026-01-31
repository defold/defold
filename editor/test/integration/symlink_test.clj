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
     (or (workspace/find-resource workspace proj-path evaluation-context)
         (throw (ex-info "The path does not refer to a resource in the workspace."
                         {:path path}))))))

(defn- make-missing-file-error-info
  [proj-path]
  {:pre [(string? proj-path)]}
  {:error-type :missing-file-error
   :proj-path proj-path})

(def ^:private expected-missing-file-error-info make-missing-file-error-info)

(defn- as-missing-file-error-info
  [value]
  (let [pattern #"\bfile '(.*?)' could not be found\b"
        message (g/error-message value)]
    (if-some [[_ proj-path] (re-find pattern message)]
      (make-missing-file-error-info proj-path)
      value)))

(defn- make-broken-symlink-error-info
  [proj-path target-path]
  {:pre [(string? proj-path)
         (path/path? target-path)]}
  {:error-type :broken-symlink-error
   :proj-path proj-path
   :target-path target-path})

(defn- expected-broken-symlink-error-info
  [proj-path referenced-project-path]
  (make-broken-symlink-error-info
    proj-path
    (path/resolve-proj-path referenced-project-path proj-path)))

(defn- as-broken-symlink-error-info
  [value]
  (let [pattern #"\bsymlink '(.*?)' refers to missing path '(.*?)'"
        message (g/error-message value)]
    (if-some [[_ proj-path target-path] (re-find pattern message)]
      (make-broken-symlink-error-info proj-path (path/of target-path))
      value)))

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
    (let [project (test-util/setup-project! workspace)]
      [workspace project])))

(defn- check-stateless-directory-symlink-valid! [project referencing-directory]
  (testing "Symlink appears as directory."
    (let [workspace (project/workspace project)
          referencing-directory-resource (path->resource workspace referencing-directory)]
      (is (resource/file-resource? referencing-directory-resource))
      (is (= :folder (resource/source-type referencing-directory-resource)))))
  (testing "Nodes work correctly."
    (let [main-collection (project/get-resource-node project "/main/main.collection")]
      (is (not (g/error? (test-util/build-error! main-collection))))
      (is (not (g/error? (g/node-value main-collection :scene))))
      (is (not (g/error? (g/node-value main-collection :node-outline)))))))

(defn- check-stateless-directory-symlink-broken! [project referencing-directory]
  (testing "Symlink appears as defective file."
    (let [workspace (project/workspace project)
          referencing-directory-resource (path->resource workspace referencing-directory)
          referencing-directory-resource-node (project/get-resource-node project referencing-directory-resource)]
      (is (resource/file-resource? referencing-directory-resource))
      (is (= :file (resource/source-type referencing-directory-resource)))
      (is (resource/placeholder-resource-type? (resource/resource-type referencing-directory-resource)))
      (is (g/defective? referencing-directory-resource-node))))
  (testing "Nodes produce errors."
    (let [main-collection (project/get-resource-node project "/main/main.collection")
          expected-error-info (expected-missing-file-error-info "/stateless_directory/referenced.png")]
      (is (= expected-error-info (as-missing-file-error-info (test-util/build-error! main-collection))))
      (is (= expected-error-info (as-missing-file-error-info (g/node-value main-collection :scene))))
      (is (not (g/error? (g/node-value main-collection :node-outline)))))))

(deftest stateless-directory-valid-symlink-test
  (let [referencing-project-path (test-util/make-temp-project-copy! "test/resources/empty_project")
        referenced-project-path (test-util/make-temp-project-copy! "test/resources/symlinks_referenced")]
    (with-open [_referencing-project-deleter (test-util/make-directory-deleter referencing-project-path)
                _referenced-project-deleter (test-util/make-directory-deleter referenced-project-path)]
      (let [referencing-directory (path/of referencing-project-path "stateless_directory")
            referenced-directory (path/of referenced-project-path "stateless_directory")
            referenced-directory-renamed (path/resolve-sibling referenced-directory "stateless_directory_renamed")]

        (fs/copy-directory!
          (io/file referenced-project-path "stateful_directory")
          (io/file referencing-project-path "stateful_directory"))

        ;; Symlink is valid before loading the project.
        (path/create-symlink! referencing-directory referenced-directory)

        (test-support/with-clean-system
          (let [[workspace project] (setup-symlink-test-project! world referencing-project-path "/stateful_directory/referenced.go")]

            (testing "Before breaking symlink."
              (check-stateless-directory-symlink-valid! project referencing-directory))

            ;; Break symlink.
            (path/move! referenced-directory referenced-directory-renamed)
            (workspace/resource-sync! workspace)

            (testing "After breaking symlink."
              (check-stateless-directory-symlink-broken! project referencing-directory))))))))

(deftest stateless-directory-broken-symlink-test
  (let [referencing-project-path (test-util/make-temp-project-copy! "test/resources/empty_project")
        referenced-project-path (test-util/make-temp-project-copy! "test/resources/symlinks_referenced")]
    (with-open [_referencing-project-deleter (test-util/make-directory-deleter referencing-project-path)
                _referenced-project-deleter (test-util/make-directory-deleter referenced-project-path)]
      (let [referencing-directory (path/of referencing-project-path "stateless_directory")
            referenced-directory (path/of referenced-project-path "stateless_directory")
            referenced-directory-renamed (path/resolve-sibling referenced-directory "stateless_directory_renamed")]

        (fs/copy-directory!
          (io/file referenced-project-path "stateful_directory")
          (io/file referencing-project-path "stateful_directory"))

        ;; Symlink is broken before loading the project.
        (path/create-symlink! referencing-directory referenced-directory)
        (path/move! referenced-directory referenced-directory-renamed)

        (test-support/with-clean-system
          (let [[workspace project] (setup-symlink-test-project! world referencing-project-path "/stateful_directory/referenced.go")]

            (testing "Before fixing symlink."
              (check-stateless-directory-symlink-broken! project referencing-directory))

            ;; Fix symlink.
            (path/move! referenced-directory-renamed referenced-directory)
            (workspace/resource-sync! workspace)

            (testing "After fixing symlink."
              (check-stateless-directory-symlink-valid! project referencing-directory))))))))

(defn- check-stateful-directory-symlink-valid! [project referencing-directory]
  (testing "Symlink appears as directory."
    (let [workspace (project/workspace project)
          referencing-directory-resource (path->resource workspace referencing-directory)]
      (is (resource/file-resource? referencing-directory-resource))
      (is (= :folder (resource/source-type referencing-directory-resource)))))
  (testing "Nodes work correctly."
    (let [main-collection (project/get-resource-node project "/main/main.collection")]
      (is (not (g/error? (test-util/build-error! main-collection))))
      (is (not (g/error? (g/node-value main-collection :scene))))
      (is (not (g/error? (g/node-value main-collection :node-outline)))))))

(defn- check-stateful-directory-symlink-broken! [project referencing-directory]
  (testing "Broken symlink appears as defective file."
    (let [workspace (project/workspace project)
          referencing-directory-resource (path->resource workspace referencing-directory)
          referencing-directory-resource-node (project/get-resource-node project referencing-directory-resource)]
      (is (resource/file-resource? referencing-directory-resource))
      (is (= :file (resource/source-type referencing-directory-resource)))
      (is (resource/placeholder-resource-type? (resource/resource-type referencing-directory-resource)))
      (is (g/defective? referencing-directory-resource-node))))
  (testing "Nodes produce errors."
    (let [main-collection (project/get-resource-node project "/main/main.collection")
          expected-error-info (expected-missing-file-error-info "/stateful_directory/referenced.go")]
      (is (= expected-error-info (as-missing-file-error-info (test-util/build-error! main-collection))))
      (is (= expected-error-info (as-missing-file-error-info (g/node-value main-collection :scene))))
      (is (not (g/error? (g/node-value main-collection :node-outline)))))))

(deftest stateful-directory-valid-symlink-test
  (let [referencing-project-path (test-util/make-temp-project-copy! "test/resources/empty_project")
        referenced-project-path (test-util/make-temp-project-copy! "test/resources/symlinks_referenced")]
    (with-open [_referencing-project-deleter (test-util/make-directory-deleter referencing-project-path)
                _referenced-project-deleter (test-util/make-directory-deleter referenced-project-path)]
      (let [referencing-directory (path/of referencing-project-path "stateful_directory")
            referenced-directory (path/of referenced-project-path "stateful_directory")
            referenced-directory-renamed (path/resolve-sibling referenced-directory "stateful_directory_renamed")]

        (fs/copy-directory!
          (io/file referenced-project-path "stateless_directory")
          (io/file referencing-project-path "stateless_directory"))

        ;; Symlink is valid before loading the project.
        (path/create-symlink! referencing-directory referenced-directory)

        (test-support/with-clean-system
          (let [[workspace project] (setup-symlink-test-project! world referencing-project-path "/stateful_directory/referenced.go")]

            (testing "Before breaking symlink."
              (check-stateful-directory-symlink-valid! project referencing-directory))

            ;; Break symlink.
            (path/move! referenced-directory referenced-directory-renamed)
            (workspace/resource-sync! workspace)

            (testing "After breaking symlink."
              (check-stateful-directory-symlink-broken! project referencing-directory))))))))

(deftest stateful-directory-broken-symlink-test
  (let [referencing-project-path (test-util/make-temp-project-copy! "test/resources/empty_project")
        referenced-project-path (test-util/make-temp-project-copy! "test/resources/symlinks_referenced")]
    (with-open [_referencing-project-deleter (test-util/make-directory-deleter referencing-project-path)
                _referenced-project-deleter (test-util/make-directory-deleter referenced-project-path)]
      (let [referencing-directory (path/of referencing-project-path "stateful_directory")
            referenced-directory (path/of referenced-project-path "stateful_directory")
            referenced-directory-renamed (path/resolve-sibling referenced-directory "stateful_directory_renamed")]

        (fs/copy-directory!
          (io/file referenced-project-path "stateless_directory")
          (io/file referencing-project-path "stateless_directory"))

        ;; Symlink is broken before loading the project.
        (path/create-symlink! referencing-directory referenced-directory)
        (path/move! referenced-directory referenced-directory-renamed)

        (test-support/with-clean-system
          (let [[workspace project] (setup-symlink-test-project! world referencing-project-path "/stateful_directory/referenced.go")]

            (testing "Before fixing symlink."
              (check-stateful-directory-symlink-broken! project referencing-directory))

            ;; Fix symlink.
            (path/move! referenced-directory-renamed referenced-directory)
            (workspace/resource-sync! workspace)

            (testing "After fixing symlink."
              (check-stateful-directory-symlink-valid! project referencing-directory))))))))

(defn- create-file-symlinks! [referencing-directory referenced-directory]
  (coll/reduce-> (path/tree-walker referenced-directory) {}
    (remove path/hidden?)
    (fn [referencing-file->referenced-file referenced-file]
      (let [referencing-file (path/reroot referenced-file referenced-directory referencing-directory)]
        (path/create-parent-directories! referencing-file)
        (path/create-symlink! referencing-file referenced-file)
        (assoc referencing-file->referenced-file referencing-file referenced-file)))))

(defn- rename-referenced-files! [referenced-files]
  (coll/reduce-> referenced-files {}
    (fn [referenced-file->renamed-file referenced-file]
      (let [renamed-filename (str "renamed_" (path/leaf referenced-file))
            renamed-file (path/resolve-sibling referenced-file renamed-filename)]
        (path/move! referenced-file renamed-file)
        (assoc referenced-file->renamed-file referenced-file renamed-file)))))

(defn- check-stateless-file-symlinks-valid! [project referencing-files]
  (testing "Symlinks appear as files of their respective resource-type."
    (let [workspace (project/workspace project)
          ext->resource-type (workspace/get-resource-type-map workspace)]
      (doseq [referencing-file referencing-files]
        (let [referencing-file-resource (path->resource workspace referencing-file)
              referencing-file-resource-node (project/get-resource-node project referencing-file-resource)
              ext (resource/type-ext referencing-file-resource)
              resource-type (ext->resource-type ext)]
          (is (resource/file-resource? referencing-file-resource))
          (is (= :file (resource/source-type referencing-file-resource)))
          (is (= resource-type (resource/resource-type referencing-file-resource)))
          (is (not (g/defective? referencing-file-resource-node)))))))
  (testing "Nodes work correctly."
    (let [main-collection (project/get-resource-node project "/main/main.collection")]
      (is (not (g/error? (test-util/build-error! main-collection))))
      (is (not (g/error? (g/node-value main-collection :scene))))
      (is (not (g/error? (g/node-value main-collection :node-outline)))))))

(defn- check-stateless-file-symlinks-broken! [project referencing-files referenced-project-path]
  (testing "Broken symlinks appear as non-defective files of their respective resource-type."
    (let [workspace (project/workspace project)
          ext->resource-type (workspace/get-resource-type-map workspace)]
      (doseq [referencing-file referencing-files]
        (let [referencing-file-resource (path->resource workspace referencing-file)
              referencing-file-resource-node (project/get-resource-node project referencing-file-resource)
              ext (resource/type-ext referencing-file-resource)
              resource-type (ext->resource-type ext)]
          (is (resource/file-resource? referencing-file-resource))
          (is (= :file (resource/source-type referencing-file-resource)))
          (is (= resource-type (resource/resource-type referencing-file-resource)))
          (is (not (g/defective? referencing-file-resource-node))))))) ; Stateless files are never marked defective.
  (testing "Nodes produce errors."
    (let [main-collection (project/get-resource-node project "/main/main.collection")
          expected-error-info (expected-broken-symlink-error-info "/stateless_files/referenced.png" referenced-project-path)]
      (is (= expected-error-info (as-broken-symlink-error-info (test-util/build-error! main-collection))))
      (is (= expected-error-info (as-broken-symlink-error-info (g/node-value main-collection :scene))))
      (is (not (g/error? (g/node-value main-collection :node-outline)))))))

(deftest stateless-file-valid-symlink-test
  (let [referencing-project-path (test-util/make-temp-project-copy! "test/resources/empty_project")
        referenced-project-path (test-util/make-temp-project-copy! "test/resources/symlinks_referenced")]
    (with-open [_referencing-project-deleter (test-util/make-directory-deleter referencing-project-path)
                _referenced-project-deleter (test-util/make-directory-deleter referenced-project-path)]
      (let [referencing-directory (path/of referencing-project-path "stateless_files")
            referenced-directory (path/of referenced-project-path "stateless_files")]

        (fs/copy-directory!
          (io/file referenced-project-path "stateful_files")
          (io/file referencing-project-path "stateful_files"))

        ;; Symlinks are valid before loading the project.
        (let [referencing-file->referenced-file (create-file-symlinks! referencing-directory referenced-directory)
              referencing-files (keys referencing-file->referenced-file)
              referenced-files (vals referencing-file->referenced-file)]

          (test-support/with-clean-system
            (let [[workspace project] (setup-symlink-test-project! world referencing-project-path "/stateful_files/referenced.go")]

              (testing "Before breaking symlinks."
                (check-stateless-file-symlinks-valid! project referencing-files))

              ;; Break symlinks.
              (rename-referenced-files! referenced-files)
              (workspace/resource-sync! workspace)

              (testing "After breaking symlinks."
                (check-stateless-file-symlinks-broken! project referencing-files referenced-project-path)))))))))

(deftest stateless-file-broken-symlink-test
  (let [referencing-project-path (test-util/make-temp-project-copy! "test/resources/empty_project")
        referenced-project-path (test-util/make-temp-project-copy! "test/resources/symlinks_referenced")]
    (with-open [_referencing-project-deleter (test-util/make-directory-deleter referencing-project-path)
                _referenced-project-deleter (test-util/make-directory-deleter referenced-project-path)]
      (let [referencing-directory (path/of referencing-project-path "stateless_files")
            referenced-directory (path/of referenced-project-path "stateless_files")]

        (fs/copy-directory!
          (io/file referenced-project-path "stateful_files")
          (io/file referencing-project-path "stateful_files"))

        ;; Symlinks are broken before loading the project.
        (let [referencing-file->referenced-file (create-file-symlinks! referencing-directory referenced-directory)
              referencing-files (keys referencing-file->referenced-file)
              referenced-files (vals referencing-file->referenced-file)
              referenced-file->renamed-file (rename-referenced-files! referenced-files)]

          (test-support/with-clean-system
            (let [[workspace project] (setup-symlink-test-project! world referencing-project-path "/stateful_files/referenced.go")]

              (testing "Before fixing symlinks."
                (check-stateless-file-symlinks-broken! project referencing-files referenced-project-path))

              ;; Fix symlinks.
              (doseq [[referenced-file renamed-file] referenced-file->renamed-file]
                (path/move! renamed-file referenced-file))
              (workspace/resource-sync! workspace)

              (testing "After fixing symlinks."
                (check-stateless-file-symlinks-valid! project referencing-files)))))))))

(defn- check-stateful-file-symlinks-valid! [project referencing-files]
  (testing "Symlinks appear as files of their respective resource-type."
    (let [workspace (project/workspace project)
          ext->resource-type (workspace/get-resource-type-map workspace)]
      (doseq [referencing-file referencing-files]
        (let [referencing-file-resource (path->resource workspace referencing-file)
              referencing-file-resource-node (project/get-resource-node project referencing-file-resource)
              ext (resource/type-ext referencing-file-resource)
              resource-type (ext->resource-type ext)]
          (is (resource/file-resource? referencing-file-resource))
          (is (= :file (resource/source-type referencing-file-resource)))
          (is (= resource-type (resource/resource-type referencing-file-resource)))
          (is (not (g/defective? referencing-file-resource-node)))))))
  (testing "Nodes work correctly."
    (let [main-collection (project/get-resource-node project "/main/main.collection")]
      (is (not (g/error? (test-util/build-error! main-collection))))
      (is (not (g/error? (g/node-value main-collection :scene))))
      (is (not (g/error? (g/node-value main-collection :node-outline)))))))

(defn- check-stateful-file-symlinks-broken! [project referencing-files referenced-project-path]
  (testing "Broken symlinks appear as defective files of their respective resource-type."
    (let [workspace (project/workspace project)
          ext->resource-type (workspace/get-resource-type-map workspace)]
      (doseq [referencing-file referencing-files]
        (let [referencing-file-resource (path->resource workspace referencing-file)
              referencing-file-resource-node (project/get-resource-node project referencing-file-resource)
              ext (resource/type-ext referencing-file-resource)
              resource-type (ext->resource-type ext)]
          (is (resource/file-resource? referencing-file-resource))
          (is (= :file (resource/source-type referencing-file-resource)))
          (is (= resource-type (resource/resource-type referencing-file-resource)))
          (is (g/defective? referencing-file-resource-node))))))
  (testing "Nodes produce errors."
    (let [main-collection (project/get-resource-node project "/main/main.collection")
          expected-error-info (expected-broken-symlink-error-info "/stateful_files/referenced.go" referenced-project-path)]
      (is (= expected-error-info (as-broken-symlink-error-info (test-util/build-error! main-collection))))
      (is (= expected-error-info (as-broken-symlink-error-info (g/node-value main-collection :scene))))
      (is (not (g/error? (g/node-value main-collection :node-outline)))))))

(deftest stateful-file-valid-symlink-test
  (let [referencing-project-path (test-util/make-temp-project-copy! "test/resources/empty_project")
        referenced-project-path (test-util/make-temp-project-copy! "test/resources/symlinks_referenced")]
    (with-open [_referencing-project-deleter (test-util/make-directory-deleter referencing-project-path)
                _referenced-project-deleter (test-util/make-directory-deleter referenced-project-path)]
      (let [referencing-directory (path/of referencing-project-path "stateful_files")
            referenced-directory (path/of referenced-project-path "stateful_files")]

        (fs/copy-directory!
          (io/file referenced-project-path "stateless_files")
          (io/file referencing-project-path "stateless_files"))

        ;; Symlinks are valid before loading the project.
        (let [referencing-file->referenced-file (create-file-symlinks! referencing-directory referenced-directory)
              referencing-files (keys referencing-file->referenced-file)
              referenced-files (vals referencing-file->referenced-file)]

          (test-support/with-clean-system
            (let [[workspace project] (setup-symlink-test-project! world referencing-project-path "/stateful_files/referenced.go")]

              (testing "Before breaking symlinks."
                (check-stateful-file-symlinks-valid! project referencing-files))

              ;; Break symlinks.
              (rename-referenced-files! referenced-files)
              (workspace/resource-sync! workspace)

              (testing "After breaking symlinks."
                (check-stateful-file-symlinks-broken! project referencing-files referenced-project-path)))))))))

(deftest stateful-file-broken-symlink-test
  (let [referencing-project-path (test-util/make-temp-project-copy! "test/resources/empty_project")
        referenced-project-path (test-util/make-temp-project-copy! "test/resources/symlinks_referenced")]
    (with-open [_referencing-project-deleter (test-util/make-directory-deleter referencing-project-path)
                _referenced-project-deleter (test-util/make-directory-deleter referenced-project-path)]
      (let [referencing-directory (path/of referencing-project-path "stateful_files")
            referenced-directory (path/of referenced-project-path "stateful_files")]

        (fs/copy-directory!
          (io/file referenced-project-path "stateless_files")
          (io/file referencing-project-path "stateless_files"))

        ;; Symlinks are broken before loading the project.
        (let [referencing-file->referenced-file (create-file-symlinks! referencing-directory referenced-directory)
              referencing-files (keys referencing-file->referenced-file)
              referenced-files (vals referencing-file->referenced-file)
              referenced-file->renamed-file (rename-referenced-files! referenced-files)]

          (test-support/with-clean-system
            (let [[workspace project] (setup-symlink-test-project! world referencing-project-path "/stateful_files/referenced.go")]

              (testing "Before fixing symlinks."
                (check-stateful-file-symlinks-broken! project referencing-files referenced-project-path))

              ;; Fix symlinks.
              (doseq [[referenced-file renamed-file] referenced-file->renamed-file]
                (path/move! renamed-file referenced-file))
              (workspace/resource-sync! workspace)

              (testing "After fixing symlinks."
                (check-stateful-file-symlinks-valid! project referencing-files)))))))))
