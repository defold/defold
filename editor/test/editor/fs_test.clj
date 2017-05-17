(ns editor.fs-test
  (:require [clojure.java.io :as io]
            [clojure.test :refer :all]
            [editor.fs :as fs]
            [integration.test-util :as test-util])
  (:import [java.io File]
           [java.nio.file NoSuchFileException FileAlreadyExistsException]))

(def ^:private file-test-tree [{"directory"
                                ["dir.txt" {"subdirectory"
                                            ["sub.txt" {"subsubdirectory"
                                                        ["subsub.txt"]}]}]}
                               "root.txt"])

(def ^:private no-root-file-test-tree (subvec file-test-tree 0 1))
(def ^:private no-subdirectory-file-test-tree (assoc-in file-test-tree [0 "directory"] ["dir.txt"]))

(defn- setup-delete-file-test [^File dir]
  (test-util/make-file-tree! dir file-test-tree))

(deftest delete-test
  (testing "Delete file"
    (test-util/with-temp-dir! dir
      (setup-delete-file-test dir)
      (is (= (io/file dir "root.txt") (fs/delete-file! (io/file dir "root.txt"))))
      (is (= no-root-file-test-tree (test-util/file-tree dir)))))
  
  (testing "Delete missing file"
    (test-util/with-temp-dir! dir
      (setup-delete-file-test dir)
      (is (= (io/file dir "non-existing") (fs/delete-file! (io/file dir "non-existing"))))
      (is (= file-test-tree (test-util/file-tree dir)))))
  
  (testing "Delete missing file failing"
    (test-util/with-temp-dir! dir
      (setup-delete-file-test dir)
      (is (thrown? NoSuchFileException (fs/delete-file! (io/file dir "non-existing") {:missing :fail})))
      (is (= file-test-tree (test-util/file-tree dir)))))
  
  (testing "Delete missing file failing silently"
    (test-util/with-temp-dir! dir
      (setup-delete-file-test dir)
      (is (= nil (fs/delete-file! (io/file dir "non-existing") {:missing :fail :fail :silently})))
      (is (= file-test-tree (test-util/file-tree dir)))))
  
  (testing "Delete dir"
    (test-util/with-temp-dir! dir
      (setup-delete-file-test dir)
      (let [target (io/file dir "directory" "subdirectory")]
        (is (= target (fs/delete-directory! target)))
        (is (= no-subdirectory-file-test-tree (test-util/file-tree dir))))))
  
  (testing "Delete missing dir"
    (test-util/with-temp-dir! dir
      (setup-delete-file-test dir)
      (let [target (io/file dir "directory" "non-existing-subdirectory")]
        (is (= target (fs/delete-directory! target)))
        (is (= file-test-tree (test-util/file-tree dir))))))
  
  (testing "Delete missing dir failing"
    (test-util/with-temp-dir! dir
      (setup-delete-file-test dir)
      (is (thrown? NoSuchFileException (fs/delete-directory! (io/file dir "non-existing-subdirectory") {:missing :fail})))
      (is (= file-test-tree (test-util/file-tree dir)))))
  
  (testing "Delete missing dir failing silently"
    (test-util/with-temp-dir! dir
      (setup-delete-file-test dir)
      (is (= nil (fs/delete-directory! (io/file dir "non-existing-subdirectory") {:missing :fail :fail :silently}))))))

(def ^:private silly-tree [{"a"
                            [{"b"
                              [{"b"
                                [{"b"
                                  ["file.txt"
                                   {"k"
                                    ["also.txt"]}]}]}
                               "file.txt"]}]}])

(deftest move-rename-test
  (testing "Rename file"
    (test-util/with-temp-dir! dir
      (test-util/make-file-tree! dir ["old-name.txt"])
      (let [src (io/file dir "old-name.txt")
            tgt (io/file dir "new-name.txt")]
        (is (= [[src tgt]] (fs/move-file! src tgt)))
        (is (= ["new-name.txt"] (test-util/file-tree dir))))))
  
  (testing "Rename file no-change"
    (test-util/with-temp-dir! dir
      (test-util/make-file-tree! dir ["name.txt"])
      (let [src (io/file dir "name.txt")
            tgt (io/file dir "name.txt")]
        (is (= [[src tgt]] (fs/move-file! src tgt)))
        (is (= ["name.txt"] (test-util/file-tree dir))))))

  (testing "Rename file caps only"
    (test-util/with-temp-dir! dir
      (test-util/make-file-tree! dir ["name.txt"])
      (let [src (io/file dir "name.txt")
            tgt (io/file dir "Name.txt")]
        (is (= [[src tgt]] (fs/move-file! src tgt)))
        (is (= ["Name.txt"] (test-util/file-tree dir))))))

  (testing "Rename file replace"
    (test-util/with-temp-dir! dir
      (test-util/make-file-tree! dir ["also.txt" "name.txt"])
      (let [src (io/file dir "name.txt")
            tgt (io/file dir "also.txt")]
        (is (= [[src tgt]] (fs/move-file! src tgt)))
        (is (= ["also.txt"] (test-util/file-tree dir))))))

  (testing "Rename file replace dir"
    (test-util/with-temp-dir! dir
      (test-util/make-file-tree! dir [{"cake" ["tosca.txt"]} "name.txt"])
      (let [src (io/file dir "name.txt")
            tgt (io/file dir "cake")]
        (is (= [[src tgt]] (fs/move-file! src tgt)))
        (is (= ["cake"] (test-util/file-tree dir))))))
  
  (testing "Rename file failing"
    (test-util/with-temp-dir! dir
      (test-util/make-file-tree! dir ["also.txt" "name.txt"])
      (let [src (io/file dir "name.txt")
            tgt (io/file dir "also.txt")]
        (is (thrown? FileAlreadyExistsException (fs/move-file! src tgt {:target :keep})))
        (is (= ["also.txt" "name.txt"] (test-util/file-tree dir))))))
  
  (testing "Rename file failing silently"
    (test-util/with-temp-dir! dir
      (test-util/make-file-tree! dir ["also.txt" "name.txt"])
      (let [src (io/file dir "name.txt")
            tgt (io/file dir "also.txt")]
        (is (= [] (fs/move-file! src tgt {:target :keep :fail :silently})))
        (is (= ["also.txt" "name.txt"] (test-util/file-tree dir))))))

  (testing "Rename missing file failing"
    (test-util/with-temp-dir! dir
      (test-util/make-file-tree! dir ["also.txt"]) ; no name.txt
      (let [src (io/file dir "name.txt")
            tgt (io/file dir "also.txt")]
        (is (thrown? NoSuchFileException (fs/move-file! src tgt {:target :keep})))
        (is (= ["also.txt"] (test-util/file-tree dir))))))
  
  (testing "Rename missing file failing silently"
    (test-util/with-temp-dir! dir
      (test-util/make-file-tree! dir ["also.txt"]) ; no name.txt
      (let [src (io/file dir "name.txt")
            tgt (io/file dir "also.txt")]
        (is (= [] (fs/move-file! src tgt {:target :keep :fail :silently})))
        (is (= ["also.txt"] (test-util/file-tree dir))))))

  (testing "Rename dir"
    (test-util/with-temp-dir! dir
      (test-util/make-file-tree! dir [{"directory" ["name.txt"]}])
      (let [src (io/file dir "directory")
            tgt (io/file dir "another")]
        (is (= [[src tgt]] (fs/move-directory! src tgt)))
        (is (= [{"another" ["name.txt"]}] (test-util/file-tree dir))))))

  (testing "Rename dir no-change"
    (test-util/with-temp-dir! dir
      (test-util/make-file-tree! dir [{"directory" ["name.txt"]}])
      (let [src (io/file dir "directory")
            tgt (io/file dir "directory")]
        (is (= [[src tgt]] (fs/move-directory! src tgt)))
        (is (= [{"directory" ["name.txt"]}] (test-util/file-tree dir))))))

  (testing "Rename dir caps only"
    (test-util/with-temp-dir! dir
      (test-util/make-file-tree! dir [{"directory" ["name.txt"]}])
      (let [src (io/file dir "directory")
            tgt (io/file dir "Directory")]
        (is (= [[src tgt]] (fs/move-directory! src tgt)))
        (is (= [{"Directory" ["name.txt"]}] (test-util/file-tree dir))))))
  
  (testing "Rename dir replace"
    (test-util/with-temp-dir! dir
      (test-util/make-file-tree! dir [{"directory" ["name.txt"]}
                                      {"directory2" ["name2.txt"]}])
      (let [src (io/file dir "directory")
            tgt (io/file dir "directory2")]
        (is (= [[src tgt]] (fs/move-directory! src tgt)))
        (is (= [{"directory2" ["name.txt"]}] (test-util/file-tree dir))))))

  (testing "Rename dir replace file"
    (test-util/with-temp-dir! dir
      (test-util/make-file-tree! dir [{"directory" ["file.txt"]} "cake"])
      (let [src (io/file dir "directory")
            tgt (io/file dir "cake")]
        (is (= [[src tgt]] (fs/move-directory! src tgt)))
        (is (= [{"cake" ["file.txt"]}] (test-util/file-tree dir))))))
  
  (testing "Rename dir failing"
    (test-util/with-temp-dir! dir
      (let [tree [{"another" ["file2.txt"]} {"directory" ["file.txt"]}]]
        (test-util/make-file-tree! dir tree)
        (let [src (io/file dir "directory")
              tgt (io/file dir "another")]
          (is (thrown? FileAlreadyExistsException (fs/move-directory! src tgt {:target :keep})))
          (is (= tree (test-util/file-tree dir)))))))

  (testing "Rename dir failing silently"
    (test-util/with-temp-dir! dir
      (let [tree [{"another" ["file2.txt"]} {"directory" ["file.txt"]}]]
        (test-util/make-file-tree! dir tree)
        (let [src (io/file dir "directory")
              tgt (io/file dir "another")]
          (is (= [] (fs/move-directory! src tgt {:target :keep :fail :silently})))
          (is (= tree (test-util/file-tree dir)))))))

  (testing "Rename missing dir failing"
    (test-util/with-temp-dir! dir
      (let [tree [{"another" ["file2.txt"]}]] ; no {"directory" [...]}
        (test-util/make-file-tree! dir tree)
        (let [src (io/file dir "directory")
              tgt (io/file dir "another")]
          (is (thrown? NoSuchFileException (fs/move-directory! src tgt)))
          (is (= tree (test-util/file-tree dir)))))))

  (testing "Rename missing dir failing silently"
    (test-util/with-temp-dir! dir
      (let [tree [{"another" ["file2.txt"]}]] ; no {"directory" [...]}
        (test-util/make-file-tree! dir tree)
        (let [src (io/file dir "directory")
              tgt (io/file dir "another")]
          (is (= [] (fs/move-directory! src tgt {:fail :silently})))
          (is (= tree (test-util/file-tree dir))))))))

(deftest move-file-test
  (testing "Move file"
    (test-util/with-temp-dir! dir
      (test-util/make-file-tree! dir [{"directory" ["also.txt"]} "name.txt"])
      (let [src (io/file dir "name.txt")
            tgt (io/file dir "directory" "name.txt")]
        (is (= [[src tgt]] (fs/move-file! src tgt)))
        (is (= [{"directory" ["also.txt" "name.txt"]}] (test-util/file-tree dir))))))
  
  (testing "Move file renaming"
    (test-util/with-temp-dir! dir
      (test-util/make-file-tree! dir [{"directory" ["also.txt"]} "name.txt"])
      (let [src (io/file dir "name.txt")
            tgt (io/file dir "directory" "new-name.txt")]
        (is (= [[src tgt]] (fs/move-file! src tgt)))
        (is (= [{"directory" ["also.txt" "new-name.txt"]}] (test-util/file-tree dir))))))
  
  (testing "Move file replace"
    (test-util/with-temp-dir! dir
      (test-util/make-file-tree! dir [{"directory" ["also.txt"]} "name.txt"])
      (let [src (io/file dir "name.txt")
            tgt (io/file dir "directory" "also.txt")]
        (is (= [[src tgt]] (fs/move-file! src tgt)))
        (is (= [{"directory" ["also.txt"]}] (test-util/file-tree dir))))))

  (testing "Move file failing"
    (test-util/with-temp-dir! dir
      (let [tree [{"directory" ["also.txt"]} "name.txt"]]
        (test-util/make-file-tree! dir tree)
        (let [src (io/file dir "name.txt")
              tgt (io/file dir "directory" "also.txt")]
          (is (thrown? FileAlreadyExistsException (fs/move-file! src tgt {:target :keep})))
          (is (= tree  (test-util/file-tree dir)))))))
  
  (testing "Move file failing silently"
    (test-util/with-temp-dir! dir
      (let [tree [{"directory" ["also.txt"]} "name.txt"]]
        (test-util/make-file-tree! dir tree)
        (let [src (io/file dir "name.txt")
              tgt (io/file dir "directory" "also.txt")]
          (is (= [] (fs/move-file! src tgt {:target :keep :fail :silently})))
          (is (= tree  (test-util/file-tree dir)))))))
  
  (testing "Move missing file failing"
    (test-util/with-temp-dir! dir
      (let [tree [{"directory" ["also.txt"]}]] ; no name.txt
        (test-util/make-file-tree! dir tree)
        (let [src (io/file dir "name.txt")
              tgt (io/file dir "directory" "name.txt")]
          (is (thrown? NoSuchFileException (fs/move-file! src tgt)))
          (is (= tree (test-util/file-tree dir)))))))

  (testing "Move missing file failing silently"
    (test-util/with-temp-dir! dir
      (let [tree [{"directory" ["also.txt"]}]] ; no name.txt
        (test-util/make-file-tree! dir tree)
        (let [src (io/file dir "name.txt")
              tgt (io/file dir "directory" "name.txt")]
          (is (= [] (fs/move-file! src tgt {:fail :silently})))
          (is (= tree (test-util/file-tree dir))))))))

(deftest move-dir-test
  (testing "Move dir"
    (test-util/with-temp-dir! dir
      (test-util/make-file-tree! dir [{"another" ["also.txt"]}
                                      {"directory" ["name.txt"]}])
      (let [src (io/file dir "directory")
            tgt (io/file dir "another" "directory")]
        (is (= [[src tgt]] (fs/move-directory! src tgt)))
        (is (= [{"another" ["also.txt" {"directory" ["name.txt"]}]}] (test-util/file-tree dir))))))

  (testing "Move dir renaming"
    (test-util/with-temp-dir! dir
      (test-util/make-file-tree! dir [{"another" ["also.txt"]}
                                      {"directory" ["name.txt"]}])
      (let [src (io/file dir "directory")
            tgt (io/file dir "another" "directory2")]
        (is (= [[src tgt]] (fs/move-directory! src tgt)))
        (is (= [{"another" ["also.txt" {"directory2" ["name.txt"]}]}] (test-util/file-tree dir))))))

  (testing "Move dir replace"
    (test-util/with-temp-dir! dir
      (test-util/make-file-tree! dir [{"another"
                                       [{"directory2" ["name2.txt"]}]}
                                      {"directory" ["name.txt"]}])
      (let [src (io/file dir "directory")
            tgt (io/file dir "another" "directory2")]
        (is (= [[src tgt]] (fs/move-directory! src tgt)))
        (is (= [{"another" [{"directory2" ["name.txt"]}]}] (test-util/file-tree dir))))))

  (testing "Move dir merging"
    (test-util/with-temp-dir! dir
      (test-util/make-file-tree! dir [{"another"
                                       [{"directory2" ["name2.txt"]}]}
                                      {"directory" ["name.txt"]}])
      (let [src (io/file dir "directory")
            tgt (io/file dir "another" "directory2")]
        (is (= [[src tgt]] (fs/move-directory! src tgt {:target :merge})))
        (is (= [{"another" [{"directory2" ["name.txt" "name2.txt"]}]}] (test-util/file-tree dir))))))
  
  (testing "Move dir failing"
    (test-util/with-temp-dir! dir
      (let [tree [{"another"
                   [{"directory2" ["name2.txt"]}]}
                  {"directory" ["name.txt"]}]]
        (test-util/make-file-tree! dir tree)
        (let [src (io/file dir "directory")
              tgt (io/file dir "another" "directory2")]
          (is (thrown? FileAlreadyExistsException (fs/move-directory! src tgt {:target :keep})))
          (is (= tree (test-util/file-tree dir)))))))

  (testing "Move dir failing silently"
    (test-util/with-temp-dir! dir
      (let [tree [{"another"
                   [{"directory2" ["name2.txt"]}]}
                  {"directory" ["name.txt"]}]]
        (test-util/make-file-tree! dir tree)
        (let [src (io/file dir "directory")
              tgt (io/file dir "another" "directory2")]
          (is (= [] (fs/move-directory! src tgt {:target :keep :fail :silently})))
          (is (= tree (test-util/file-tree dir)))))))
  
  (testing "Move missing dir failing"
    (test-util/with-temp-dir! dir
      (let [tree [{"another"
                   [{"directory2" ["name2.txt"]}]}]] ; no {"directory" [...]}
        (test-util/make-file-tree! dir tree)
        (let [src (io/file dir "directory")
              tgt (io/file dir "another" "directory2")]
          (is (thrown? NoSuchFileException (fs/move-directory! src tgt {:target :keep})))
          (is (= tree (test-util/file-tree dir)))))))
  
  (testing "Move missing dir failing silently"
    (test-util/with-temp-dir! dir
      (let [tree [{"another"
                   [{"directory2" ["name2.txt"]}]}]] ; no {"directory" [...]}
        (test-util/make-file-tree! dir tree)
        (let [src (io/file dir "directory")
              tgt (io/file dir "another" "directory2")]
          (is (= [] (fs/move-directory! src tgt {:target :keep :fail :silently})))
          (is (= tree (test-util/file-tree dir)))))))


  ;; Below tests assume this initial file tree: (def silly-tree above)
  ;;
  ;; .
  ;; └── a
  ;;     └── b
  ;;         ├── b
  ;;         │   └── b
  ;;         │       ├── file.txt
  ;;         │       └── k
  ;;         │           └── also.txt
  ;;         └── file.txt
  ;;
  ;; ... Then moves directory /a/b/b up to /a using :keep :merge and :replace
  ;;
  ;; This will break if move-directory would do for instance "cp src trg; rm src" for :merge
  ;; or "rm trg; cp src trg rm src" for :replace

  (testing "Move dir replacing parent"
    ;; expect:
    ;; .
    ;; └── a
    ;;     └── b
    ;;         └── b
    ;;             ├── file.txt
    ;;             └── k
    ;;                 └── also.txt
    ;;
    (test-util/with-temp-dir! dir
      (test-util/make-file-tree! dir silly-tree)
      (let [src (io/file dir "a" "b" "b")
            tgt (io/file dir "a" "b")]
        (is (= [[src tgt]] (fs/move-directory! src tgt)))
        (is (= [{"a"
                 [{"b"
                   [{"b"
                     ["file.txt"
                      {"k"
                       ["also.txt"]}]}]}]}] (test-util/file-tree dir))))))

  (testing "Move dir merging with parent"
    ;; expect
    ;; .
    ;; └── a
    ;;     └── b
    ;;         ├── b
    ;;         │   ├── file.txt
    ;;         │   └── k
    ;;         │       └── also.txt
    ;;         └── file.txt
    (test-util/with-temp-dir! dir
      (test-util/make-file-tree! dir silly-tree)
      (let [src (io/file dir "a" "b" "b")
            tgt (io/file dir "a" "b")]
        (is (= [[src tgt]] (fs/move-directory! src tgt {:target :merge})))
        (is (= [{"a"
                 [{"b"
                   [{"b"
                     ["file.txt"
                      {"k"
                       ["also.txt"]}]}
                    "file.txt"]}]}] (test-util/file-tree dir)))))))

(deftest copy-file-test
  (testing "Copy file"
    (test-util/with-temp-dir! dir
      (test-util/make-file-tree! dir ["name.txt"])
      (let [src (io/file dir "name.txt")
            tgt (io/file dir "name2.txt")]
        (is (= [[src tgt]] (fs/copy-file! src tgt)))
        (is (= ["name.txt" "name2.txt"] (test-util/file-tree dir))))))

  (testing "Copy file no-change"
    (test-util/with-temp-dir! dir
      (test-util/make-file-tree! dir ["name.txt"])
      (let [src (io/file dir "name.txt")
            tgt (io/file dir "name.txt")]
        (is (= [[src tgt]] (fs/copy-file! src tgt)))
        (is (= ["name.txt"] (test-util/file-tree dir))))))
  
  (testing "Copy file caps only"
    (test-util/with-temp-dir! dir
      (test-util/make-file-tree! dir ["name.txt"])
      (let [src (io/file dir "name.txt")
            tgt (io/file dir "Name.txt")]
        (if fs/case-sensitive?
          (do
            (is (= [[src tgt]] (fs/copy-file! src tgt)))
            (is (= ["Name.txt" "name.txt"] (test-util/file-tree dir))))
          (do
            (is (= [[src src]] (fs/copy-file! src tgt)))
            (is (= ["name.txt"] (test-util/file-tree dir))))))))

  (testing "Copy file replace"
    (test-util/with-temp-dir! dir
      (test-util/make-file-tree! dir ["also.txt" "name.txt"])
      (let [src (io/file dir "name.txt")
            tgt (io/file dir "also.txt")]
        (is (= [[src tgt]] (fs/copy-file! src tgt)))
        (is (= ["also.txt" "name.txt"] (test-util/file-tree dir)))
        (is (= (slurp src) (slurp tgt))))))
  
  (testing "Copy file replace dir"
    (test-util/with-temp-dir! dir
      (test-util/make-file-tree! dir [{"directory" ["also.txt"]} "name.txt"])
      (let [src (io/file dir "name.txt")
            tgt (io/file dir "directory")]
        (is (= [[src tgt]] (fs/copy-file! src tgt)))
        (is (= ["directory" "name.txt"] (test-util/file-tree dir)))
        (is (= (slurp src) (slurp tgt))))))
  
  (testing "Copy file failing"
    (test-util/with-temp-dir! dir
      (test-util/make-file-tree! dir ["also.txt" "name.txt"])
      (let [src (io/file dir "name.txt")
            tgt (io/file dir "also.txt")]
        (is (thrown? FileAlreadyExistsException (fs/copy-file! src tgt {:target :keep})))
        (is (= ["also.txt" "name.txt"] (test-util/file-tree dir)))
        (is (not= (slurp src) (slurp tgt))))))
  
  (testing "Copy file failing silently"
    (test-util/with-temp-dir! dir
      (test-util/make-file-tree! dir ["also.txt" "name.txt"])
      (let [src (io/file dir "name.txt")
            tgt (io/file dir "also.txt")]
        (is (= [] (fs/copy-file! src tgt {:target :keep :fail :silently})))
        (is (= ["also.txt" "name.txt"] (test-util/file-tree dir)))
        (is (not= (slurp src) (slurp tgt))))))

  (testing "Copy missing file failing"
    (test-util/with-temp-dir! dir
      (test-util/make-file-tree! dir ["also.txt"]) ; no name.txt
      (let [src (io/file dir "name.txt")
            tgt (io/file dir "also.txt")]
        (is (thrown? NoSuchFileException (fs/copy-file! src tgt {:target :keep})))
        (is (= ["also.txt"] (test-util/file-tree dir))))))

  (testing "Copy missing file failing silently"
    (test-util/with-temp-dir! dir
      (test-util/make-file-tree! dir ["also.txt"]) ; no name.txt
      (let [src (io/file dir "name.txt")
            tgt (io/file dir "also.txt")]
        (is (= [] (fs/copy-file! src tgt {:target :keep :fail :silently})))
        (is (= ["also.txt"] (test-util/file-tree dir)))))))

(deftest copy-dir-test
  (testing "Copy dir"
    (test-util/with-temp-dir! dir
      (test-util/make-file-tree! dir [{"directory" ["name.txt"]}])
      (let [src (io/file dir "directory")
            tgt (io/file dir "directory2")]
        (is (= [[src tgt]] (fs/copy-directory! src tgt)))
        (is (= [{"directory" ["name.txt"]} {"directory2" ["name.txt"]}] (test-util/file-tree dir))))))
  
  (testing "Copy dir no-change"
    (test-util/with-temp-dir! dir
      (test-util/make-file-tree! dir [{"directory" ["name.txt"]}])
      (let [src (io/file dir "directory")
            tgt (io/file dir "directory")]
        (is (= [[src tgt]] (fs/copy-directory! src tgt)))
        (is (= [{"directory" ["name.txt"]}] (test-util/file-tree dir))))))

  (testing "Copy dir caps only"
    (test-util/with-temp-dir! dir
      (test-util/make-file-tree! dir [{"directory" ["name.txt"]}])
      (let [src (io/file dir "directory")
            tgt (io/file dir "Directory")]
        (if fs/case-sensitive?
          (do
            (is (= [[src tgt]] (fs/copy-directory! src tgt)))
            (is (= [{"Directory" ["name.txt"]} {"directory" ["name.txt"]}] (test-util/file-tree dir))))
          (do
            (is (= [[src src]] (fs/copy-directory! src tgt)))
            (is (= [{"directory" ["name.txt"]}] (test-util/file-tree dir))))))))

  (testing "Copy dir replace"
    (test-util/with-temp-dir! dir
      (test-util/make-file-tree! dir [{"directory" ["name.txt"]} {"directory2" ["name2.txt"]}])
      (let [src (io/file dir "directory")
            tgt (io/file dir "directory2")]
        (is (= [[src tgt]] (fs/copy-directory! src tgt)))
        (is (= [{"directory" ["name.txt"]} {"directory2" ["name.txt"]}] (test-util/file-tree dir))))))
  
  (testing "Copy dir replace file"
    (test-util/with-temp-dir! dir
      (test-util/make-file-tree! dir ["cake" {"directory" ["name.txt"]}])
      (let [src (io/file dir "directory")
            tgt (io/file dir "cake")]
        (is (= [[src tgt]] (fs/copy-directory! src tgt)))
        (is (= [{"cake" ["name.txt"]} {"directory" ["name.txt"]}] (test-util/file-tree dir))))))
  
  (testing "Copy dir merge"
    (test-util/with-temp-dir! dir
      (test-util/make-file-tree! dir [{"another"
                                       [{"directory2" ["name2.txt"]}]}
                                      {"directory" ["name.txt"]}])
      (let [src (io/file dir "directory")
            tgt (io/file dir "another" "directory2")]
        (is (= [[src tgt]] (fs/copy-directory! src tgt {:target :merge})))
        (is (= [{"another" [{"directory2" ["name.txt" "name2.txt"]}]} {"directory" ["name.txt"]}] (test-util/file-tree dir))))))

  (testing "Copy dir failing"
    (test-util/with-temp-dir! dir
      (let [tree [{"another"
                   [{"directory2" ["name2.txt"]}]}
                  {"directory" ["name.txt"]}]]
        (test-util/make-file-tree! dir tree)
        (let [src (io/file dir "directory")
              tgt (io/file dir "another" "directory2")]
          (is (thrown? FileAlreadyExistsException (fs/copy-directory! src tgt {:target :keep})))
          (is (= tree (test-util/file-tree dir)))))))

  (testing "Copy dir failing silently"
    (test-util/with-temp-dir! dir
      (let [tree [{"another"
                   [{"directory2" ["name2.txt"]}]}
                  {"directory" ["name.txt"]}]]
        (test-util/make-file-tree! dir tree)
        (let [src (io/file dir "directory")
              tgt (io/file dir "another" "directory2")]
          (is (= [] (fs/copy-directory! src tgt {:target :keep :fail :silently})))
          (is (= tree (test-util/file-tree dir)))))))
  
  (testing "Copy missing dir failing"
    (test-util/with-temp-dir! dir
      (let [tree [{"another"
                   [{"directory2" ["name2.txt"]}]}]] ; no {"directory" [...]}
        (test-util/make-file-tree! dir tree)
        (let [src (io/file dir "directory")
              tgt (io/file dir "another" "directory2")]
          (is (thrown? FileAlreadyExistsException (fs/copy-directory! src tgt {:target :keep})))
          (is (= tree (test-util/file-tree dir)))))))
  
  (testing "Copy missing dir failing silently"
    (test-util/with-temp-dir! dir
      (let [tree [{"another"
                   [{"directory2" ["name2.txt"]}]}]] ; no {"directory" [...]}
        (test-util/make-file-tree! dir tree)
        (let [src (io/file dir "directory")
              tgt (io/file dir "another" "directory2")]
          (is (= [] (fs/move-directory! src tgt {:target :keep :fail :silently})))
          (is (= tree (test-util/file-tree dir))))))))

(deftest various
  (testing "File move to dir"
    (test-util/with-temp-dir! dir
      (test-util/make-file-tree! dir ["file.txt" {"directory" []}])
      (let [src (io/file dir "file.txt")
            tgt (io/file dir "directory" "file.txt")]
        (is (= [[src tgt]] (fs/move-file! src tgt)))
        (is (= [{"directory" ["file.txt"]}] (test-util/file-tree dir))))))

  (testing "File move to same dir"
    (test-util/with-temp-dir! dir
      (test-util/make-file-tree! dir ["file.txt"])
      (let [src (io/file dir "file.txt")
            tgt (io/file dir "file.txt")]
        (is (= [[src tgt]] (fs/move-file! src tgt)))
        (is (= ["file.txt"] (test-util/file-tree dir))))))

  (testing "Directory rename"
    (test-util/with-temp-dir! dir
      (test-util/make-file-tree! dir [{"old-name" ["file.txt"]}])
      (is (= [[(io/file dir "old-name") (io/file dir "new-name")]]
             (fs/move-directory! (io/file dir "old-name") (io/file dir "new-name"))))
      (is (= [{"new-name" ["file.txt"]}] (test-util/file-tree dir)))))

  (testing "Directory move"
    (test-util/with-temp-dir! dir
      (test-util/make-file-tree! dir [{"project" []} {"archive" []}])
      (is (= [[(io/file dir "project") (io/file dir "archive" "project")]] (fs/move-directory! (io/file dir "project") (io/file dir "archive" "project"))))
      (is (= [{"archive" [{"project" []}]}] (test-util/file-tree dir)))))

  (testing "File overwrite"
    (test-util/with-temp-dir! dir
      (test-util/make-file-tree! dir ["source.txt" "destination.txt"])
      (let [src (io/file dir "source.txt")
            tgt (io/file dir "destination.txt")
            src-contents (slurp src)]
        (is (= [[src tgt]] (fs/move-file! src tgt)))
        (is (= ["destination.txt"] (test-util/file-tree dir)))
        (is (= src-contents (slurp tgt)))))))

