(ns editor.code.data-test
  (:require [clojure.string :as string]
            [clojure.test :refer :all]
            [editor.code.data :as data :refer [->Cursor ->CursorRange]])
  (:import (java.io IOException)
           (java.nio CharBuffer)))

(defn c [row col]
  (let [cursor (->Cursor row col)]
    (->CursorRange cursor cursor)))

(defn cr [[from-row from-col] [to-row to-col]]
  (let [from (->Cursor from-row from-col)
        to (->Cursor to-row to-col)]
    (->CursorRange from to)))

(defrecord Clipboard [*representation-by-mime-type]
  data/Clipboard
  (has-content? [_this mime-type] (contains? @*representation-by-mime-type mime-type))
  (get-content [_this mime-type] (get @*representation-by-mime-type mime-type))
  (set-content! [_this representation-by-mime-type]
    (assert (map? representation-by-mime-type))
    (reset! *representation-by-mime-type representation-by-mime-type)))

(defn- make-test-clipboard [representation-by-mime-type]
  (assert (map? representation-by-mime-type))
  (->Clipboard (atom representation-by-mime-type)))

(defrecord GlyphMetrics [^double line-height ^double char-width ^double ascent]
  data/GlyphMetrics
  (ascent [_this] ascent)
  (line-height [_this] line-height)
  (char-width [_this character] char-width))

(defn layout-info
  ([] (layout-info nil))
  ([lines] (layout-info lines (->GlyphMetrics 14.0 9.0 6.0)))
  ([lines glyph-metrics]
   (data/layout-info 800.0 600.0 0.0 0.0 (count lines) 30.0 5.0 glyph-metrics 4)))

(defn- word-boundary-before-index? [line index]
  (#'data/word-boundary-before-index? line index))

(deftest word-boundary-before-index-test
  (is (true? (word-boundary-before-index? "word" 0)))
  (is (false? (word-boundary-before-index? "word" 1)))
  (is (true? (word-boundary-before-index? ".word" 1)))
  (is (true? (word-boundary-before-index? "word)" 4))))

(defn- word-boundary-after-index? [line index]
  (#'data/word-boundary-after-index? line index))

(deftest word-boundary-after-index-test
  (is (true? (word-boundary-after-index? "word" 4)))
  (is (false? (word-boundary-after-index? "word" 3)))
  (is (true? (word-boundary-after-index? "word." 4)))
  (is (true? (word-boundary-after-index? "(word" 1))))

(deftest cursor-comparison-test
  (let [cases [[(->Cursor 0 0) (->Cursor 0 1)]
               [(->Cursor 0 0) (->Cursor 1 0)]
               [(assoc (->Cursor 0 0) :type :a) (assoc (->Cursor 0 0) :type :b)]]]
    (doseq [expected-order cases _ (range 5)]
      (is (= expected-order (vec (apply sorted-set (shuffle expected-order))))))))

(deftest cursor-range-comparison-test
  (let [cases [[(cr [0 0] [0 1]) (cr [0 1] [0 2])]
               [(cr [0 1] [0 0]) (cr [0 2] [0 1])]
               [(cr [0 0] [0 1]) (cr [0 1] [0 0])]
               [(assoc (c 0 0) :type :a) (assoc (c 0 0) :type :b)]]]
    (doseq [expected-order cases _ (range 5)]
      (is (= expected-order (vec (apply sorted-set (shuffle expected-order))))))))

(deftest cursor-range-equivalence-test
  (let [cases [[[(cr [0 0] [0 1])]
                [(cr [0 0] [0 1]) (cr [0 0] [0 1])]]
               [[(cr [0 0] [0 1]) (cr [0 1] [0 0])]
                [(cr [0 0] [0 1]) (cr [0 1] [0 0])]]
               [[(assoc (cr [0 0] [0 1]) :type :a) (assoc (cr [0 0] [0 1]) :type :b)]
                [(assoc (cr [0 0] [0 1]) :type :a) (assoc (cr [0 0] [0 1]) :type :b)]]]]
    (doseq [[unique input] cases]
      (is (= unique (vec (apply sorted-set input)))))))

(deftest cursor-range-contains-test
  (are [range cursor contains?]
    (is (= contains?
           (data/cursor-range-contains? (cr (range 0) (range 1))
                                        (->Cursor (cursor 0) (cursor 1)))))
    [[0 0] [0 0]] [0 0] true
    [[0 0] [0 1]] [0 0] true
    [[0 0] [0 1]] [0 1] true
    [[0 1] [0 2]] [0 1] true
    [[0 1] [0 1]] [0 0] false
    [[0 1] [1 0]] [2 0] false
    [[0 1] [1 1]] [1 0] true
    [[0 1] [1 1]] [1 1] true
    [[0 1] [1 1]] [1 2] false
    [[0 1] [2 1]] [1 0] true
    [[0 1] [2 1]] [1 1] true
    [[0 1] [2 1]] [1 2] true
    [[0 1] [2 1]] [2 0] true
    [[0 1] [2 1]] [2 1] true
    [[0 1] [2 1]] [2 2] false))

(deftest cursor-range-midpoint-follows-test
  (let [cursor-range-midpoint-follows? #'data/cursor-range-midpoint-follows?]
    (are [range cursor follows?]
      (is (= follows?
             (cursor-range-midpoint-follows? (cr (range 0) (range 1))
                                             (->Cursor (cursor 0) (cursor 1)))))
      [[0 1] [0 3]] [0 0] true
      [[0 1] [0 3]] [0 1] true
      [[0 1] [0 3]] [0 2] false
      [[0 1] [0 3]] [0 3] false
      [[0 1] [0 3]] [0 4] false

      [[1 0] [3 0]] [0 0] true
      [[1 0] [3 0]] [1 0] true
      [[1 0] [3 0]] [2 0] false
      [[1 0] [3 0]] [3 0] false
      [[1 0] [3 0]] [4 0] false)))

(deftest offset-cursor-test
  (is (= (->Cursor 4 2) (data/offset-cursor (->Cursor 2 2) 2 0)))
  (is (= (->Cursor 2 4) (data/offset-cursor (->Cursor 2 2) 0 2)))

  (testing "Preserves extra data"
    (let [metadata {:my-metadata-prop :my-metadata-value}
          cursor (with-meta (assoc (->Cursor 0 0) :my-prop :my-prop-value) metadata)
          cursor' (data/offset-cursor cursor 1 1)]
      (is (= (:my-prop cursor) (:my-prop cursor')))
      (is (identical? (meta cursor) (meta cursor'))))))

(deftest offset-cursor-range-test
  (is (= (cr [4 2] [2 2]) (data/offset-cursor-range (cr [2 2] [2 2]) :start 2 0)))
  (is (= (cr [2 2] [4 2]) (data/offset-cursor-range (cr [2 2] [2 2]) :end 2 0)))
  (is (= (cr [4 2] [4 2]) (data/offset-cursor-range (cr [2 2] [2 2]) :both 2 0)))
  (is (= (cr [2 4] [2 2]) (data/offset-cursor-range (cr [2 2] [2 2]) :start 0 2)))
  (is (= (cr [2 2] [2 4]) (data/offset-cursor-range (cr [2 2] [2 2]) :end 0 2)))
  (is (= (cr [2 4] [2 4]) (data/offset-cursor-range (cr [2 2] [2 2]) :both 0 2)))

  (testing "Preserves extra data"
    (let [metadata {:my-metadata-prop :my-metadata-value}
          cursor-range (with-meta (assoc (c 0 0) :my-prop :my-prop-value) metadata)
          cursor-range' (data/offset-cursor-range cursor-range :both 1 1)]
      (is (= (:my-prop cursor-range) (:my-prop cursor-range')))
      (is (identical? (meta cursor-range) (meta cursor-range'))))))

(deftest lines-reader-test
  (testing "Regular use"
    (is (= "" (slurp (data/lines-reader []))))
    (is (= "" (slurp (data/lines-reader [""]))))
    (is (= "one" (slurp (data/lines-reader ["one"]))))
    (is (= "\n" (slurp (data/lines-reader ["" ""]))))
    (is (= "\ntwo" (slurp (data/lines-reader ["" "two"]))))
    (is (= "one\ntwo" (slurp (data/lines-reader ["one" "two"])))))

  (testing "Read method overloads"
    (testing "Arity zero"
      (let [reader (data/lines-reader ["ab"])]
        (is (= (int \a) (.read reader)))
        (is (= (int \b) (.read reader)))
        (is (= -1 (.read reader)))))

    (testing "Arity one"
      (testing "char[] dest"
        (let [reader (data/lines-reader ["abc"])
              dest-buffer (char-array 3)]
          (is (= 3 (.read reader dest-buffer)))
          (is (= "abc" (String. dest-buffer)))))
      (testing "CharBuffer dest"
        (let [reader (data/lines-reader ["abc"])
              dest-buffer (CharBuffer/allocate 3)]
          (is (= 3 (.read reader dest-buffer)))
          (is (= "abc" (String. (.array dest-buffer))))))))

  (testing "Superclass call to proxy"
    (let [reader (data/lines-reader ["abc"])]
      (is (= (int \a) (.read reader)))
      (is (= 1 (.skip reader 1)))
      (is (= (int \c) (.read reader)))))

  (testing "Read from closed Reader"
    (let [reader (data/lines-reader ["abc"])]
      (.close reader)
      (is (thrown? IOException (.read reader))))))

(deftest move-cursors-test
  (testing "Basic movement"
    (is (= [(c 0 0)] (data/move-cursors [(c 1 0)] #'data/cursor-up ["a" "b" "c"])))
    (is (= [(c 2 0)] (data/move-cursors [(c 1 0)] #'data/cursor-down ["a" "b" "c"])))
    (is (= [(c 0 0)] (data/move-cursors [(c 0 1)] #'data/cursor-left ["ab"])))
    (is (= [(c 0 2)] (data/move-cursors [(c 0 1)] #'data/cursor-right ["ab"]))))

  (testing "Out-of-bounds movement"
    (is (= [(c 0 0)] (data/move-cursors [(c 0 0)] #'data/cursor-up ["a" "b" "c"])))
    (is (= [(c 2 1)] (data/move-cursors [(c 2 1)] #'data/cursor-down ["a" "b" "c"])))
    (is (= [(c 0 0)] (data/move-cursors [(c 0 0)] #'data/cursor-left ["ab"])))
    (is (= [(c 0 2)] (data/move-cursors [(c 0 2)] #'data/cursor-right ["ab"]))))

  (testing "Vertical movement at top and bottom"
    (is (= [(c 0 0)] (data/move-cursors [(c 0 3)] #'data/cursor-up ["first" "last"])))
    (is (= [(c 1 4)] (data/move-cursors [(c 1 3)] #'data/cursor-down ["first" "last"]))))

  (testing "Vertical movement retains out-of-bounds column"
    (is (= [(c 0 2)] (data/move-cursors [(c 1 2)] #'data/cursor-up ["a" "ab"])))
    (is (= [(c 1 2)] (data/move-cursors [(c 0 2)] #'data/cursor-down ["ab" "a"]))))

  (testing "Horizontal movement wraps to surrounding lines"
    (is (= [(c 0 1)] (data/move-cursors [(c 1 0)] #'data/cursor-left ["a" "b" "c"])))
    (is (= [(c 2 0)] (data/move-cursors [(c 1 1)] #'data/cursor-right ["a" "b" "c"]))))

  (testing "Multiple cursors"
    (is (= [(c 0 0) (c 1 1)] (data/move-cursors [(c 1 0) (c 2 1)] #'data/cursor-up ["a" "b" "c"])))
    (is (= [(c 1 1) (c 2 0)] (data/move-cursors [(c 0 1) (c 1 0)] #'data/cursor-down ["a" "b" "c"])))
    (is (= [(c 0 0) (c 0 1)] (data/move-cursors [(c 0 1) (c 0 2)] #'data/cursor-left ["ab"])))
    (is (= [(c 0 1) (c 0 2)] (data/move-cursors [(c 0 0) (c 0 1)] #'data/cursor-right ["ab"]))))

  (testing "Multiple cursors merge when overlapping"
    (is (= [(c 0 0)] (data/move-cursors [(c 0 0) (c 0 1)] #'data/cursor-up ["a"])))
    (is (= [(c 0 0)] (data/move-cursors [(c 0 0) (c 1 0)] #'data/cursor-up ["a" "b"])))
    (is (= [(c 0 1)] (data/move-cursors [(c 0 0) (c 0 1)] #'data/cursor-down ["a"])))
    (is (= [(c 1 1)] (data/move-cursors [(c 0 1) (c 1 1)] #'data/cursor-down ["a" "b"])))
    (is (= [(c 0 0)] (data/move-cursors [(c 0 0) (c 0 1)] #'data/cursor-left ["ab"])))
    (is (= [(c 0 0)] (data/move-cursors [(c 0 0) (c 1 0)] #'data/cursor-left ["" "b"])))
    (is (= [(c 0 2)] (data/move-cursors [(c 0 1) (c 0 2)] #'data/cursor-right ["ab"])))
    (is (= [(c 1 0)] (data/move-cursors [(c 0 1) (c 1 0)] #'data/cursor-right ["a" ""])))))

(deftest splice-lines-test
  (let [splice-lines #'data/splice-lines]
    (is (= ["one"]
           (splice-lines [""]
                         [[(c 0 0) ["one"]]])))
    (is (= ["one"
            "two"]
           (splice-lines [""]
                         [[(c 0 0) ["one"
                                    "two"]]])))
    (is (= ["one"
            "two"]
           (splice-lines ["one"
                          "two"]
                         [[(c 0 0) [""]]])))
    (is (= ["before-one"
            "two"]
           (splice-lines ["one"
                          "two"]
                         [[(c 0 0) ["before-"]]])))
    (is (= ["one"
            "before-two"]
           (splice-lines ["one"
                          "two"]
                         [[(c 1 0) ["before-"]]])))
    (is (= ["one-after"
            "two"]
           (splice-lines ["one"
                          "two"]
                         [[(c 0 3) ["-after"]]])))
    (is (= ["one-after"
            "before-two"]
           (splice-lines ["one"
                          "two"]
                         [[(cr [0 3] [1 0]) ["-after"
                                             "before-"]]])))
    (is (= ["one"
            "two-after"
            "three"]
           (splice-lines ["one"
                          "two"]
                         [[(c 1 3) ["-after"
                                    "three"]]])))
    (is (= ["one"]
           (splice-lines ["owner"]
                         [[(cr [0 1] [0 2]) [""]]
                          [(cr [0 4] [0 5]) [""]]])))
    (is (= ["owner"]
           (splice-lines ["one"]
                         [[(c 0 1) ["w"]]
                          [(c 0 3) ["r"]]])))
    (is (= ["bone"
            "tomato"]
           (splice-lines ["one"
                          "two"]
                         [[(cr [0 0] [0 1]) ["bo"]]
                          [(cr [1 1] [1 2]) ["omat"]]])))))

(defn- splice-cursor-ranges [ascending-cursor-ranges ascending-cursor-ranges-and-replacements]
  (#'data/splice-cursor-ranges ascending-cursor-ranges ascending-cursor-ranges-and-replacements))

(defn- self-splice-cursor-ranges [ascending-cursor-ranges-and-replacements]
  (let [ascending-cursor-ranges (mapv first ascending-cursor-ranges-and-replacements)]
    (splice-cursor-ranges ascending-cursor-ranges ascending-cursor-ranges-and-replacements)))

(deftest splice-cursor-ranges-test
  (testing "Self-splicing"
    (is (= [(cr [0 0] [0 3])]
           (self-splice-cursor-ranges [[(c 0 0) ["one"]]])))
    (is (= [(cr [0 0] [1 3])]
           (self-splice-cursor-ranges [[(c 0 0) ["one"
                                                 "two"]]])))
    (is (= [(c 0 0)]
           (self-splice-cursor-ranges [[(c 0 0) [""]]])))
    (is (= [(cr [0 0] [0 7])]
           (self-splice-cursor-ranges [[(c 0 0) ["before-"]]])))
    (is (= [(cr [1 0] [1 7])]
           (self-splice-cursor-ranges [[(c 1 0) ["before-"]]])))
    (is (= [(cr [0 3] [0 9])]
           (self-splice-cursor-ranges [[(c 0 3) ["-after"]]])))
    (is (= [(cr [0 3] [1 7])]
           (self-splice-cursor-ranges [[(cr [0 3] [1 0]) ["-after"
                                                          "before-"]]])))
    (is (= [(cr [1 3] [2 5])]
           (self-splice-cursor-ranges [[(c 1 3) ["-after"
                                                 "three"]]])))
    (is (= [(c 0 1)
            (c 0 3)]
           (self-splice-cursor-ranges [[(cr [0 1] [0 2]) [""]]
                                       [(cr [0 4] [0 5]) [""]]])))
    (is (= [(cr [0 1] [0 2])
            (cr [0 4] [0 5])]
           (self-splice-cursor-ranges [[(c 0 1) ["w"]]
                                       [(c 0 3) ["r"]]])))
    (is (= [(cr [0 0] [0 2])
            (cr [1 1] [1 5])]
           (self-splice-cursor-ranges [[(cr [0 0] [0 1]) ["bo"]]
                                       [(cr [1 1] [1 2]) ["omat"]]])))
    (is (= [(cr [0 0] [0 1])
            (cr [1 0] [1 1])
            (cr [1 5] [1 6])]
           (self-splice-cursor-ranges [[(c 0 0) ["X"]]
                                       [(c 1 0) ["X"]]
                                       [(c 1 4) ["X"]]])))
    (is (= [(cr [0 0] [1 3])
            (cr [1 4] [2 3])]
           (self-splice-cursor-ranges [[(c 0 0) ["one"
                                                 "two"]]
                                       [(c 0 1) ["one"
                                                 "two"]]])))
    (is (= [(c 0 3)]
           (self-splice-cursor-ranges [[(cr [1 0] [0 3]) [""]]
                                       [(cr [2 0] [1 0]) [""]]
                                       [(cr [3 0] [2 0]) [""]]]))))

  (testing "More ranges than splices"
    (is (= [(cr [0 0] [0 3])
            (cr [0 4] [0 6])]
           (splice-cursor-ranges [(cr [0 0] [0 2])
                                  (cr [0 3] [0 5])]
                                 [[(c 0 1) ["X"]]])))

    (is (= [(cr [0 0] [0 4])
            (cr [1 0] [1 3])]
           (splice-cursor-ranges [(cr [0 0] [0 3])
                                  (cr [1 0] [1 3])]
                                 [[(c 0 1) ["X"]]]))))

  (testing "More splices than ranges"
    ;; "ABC" -> "AoneBtwoC"
    (is (= [(cr [0 0] [0 9])]
           (splice-cursor-ranges [(cr [0 0] [0 3])]
                                 [[(c 0 1) ["one"]]
                                  [(c 0 2) ["two"]]])))

    ;; "ABC" -> "Aone
    ;;           twoBone
    ;;           twoC"
    (is (= [(cr [0 0] [2 4])]
           (splice-cursor-ranges [(cr [0 0] [0 3])]
                                 [[(c 0 1) ["one"
                                            "two"]]
                                  [(c 0 2) ["one"
                                            "two"]]]))))

  (testing "Other"
    (is (= [(cr [31 4] [31 37])]
           (splice-cursor-ranges [(cr [30 4] [30 37])]
                                 [[(c 29 54) [""
                                              ""]]])))
    (is (= [(cr [0 0] [0 5])
            (cr [0 1] [0 2])]
           (splice-cursor-ranges [(cr [0 0] [0 4])
                                  (cr [0 1] [0 2])]
                                 [[(c 0 3) ["X"]]]))))

  (testing "Preserves extra data"
    (let [cursor-range (with-meta
                         (assoc
                           (->CursorRange
                             (with-meta
                               (assoc
                                 (->Cursor 0 0)
                                 :from-cursor-prop :from-cursor-value)
                               {:from-cursor-metadata-prop :from-cursor-metadata-value})
                             (with-meta
                               (assoc
                                 (->Cursor 0 0)
                                 :to-cursor-prop :to-cursor-value)
                               {:to-cursor-metadata-prop :to-cursor-metadata-value}))
                           :cursor-range-prop :cursor-range-value)
                         {:cursor-range-metadata-prop :cursor-range-metadata-value})]
      (let [cursor-range' (first (splice-cursor-ranges [cursor-range] [[(c 0 0) ["one"]]]))]
        (is (not= cursor-range cursor-range'))
        (is (= :cursor-range-value (:cursor-range-prop cursor-range')))
        (is (= :cursor-range-metadata-value (:cursor-range-metadata-prop (meta cursor-range'))))
        (is (= :from-cursor-value (:from-cursor-prop (.from cursor-range'))))
        (is (= :from-cursor-metadata-value (:from-cursor-metadata-prop (meta (.from cursor-range')))))
        (is (= :to-cursor-value (:to-cursor-prop (.to cursor-range'))))
        (is (= :to-cursor-metadata-value (:to-cursor-metadata-prop (meta (.to cursor-range')))))))))

(defn- insert-text [lines cursor-ranges text]
  (#'data/insert-text #"\t" "\t" nil lines cursor-ranges nil (layout-info lines) text))

(deftest insert-text-test
  (testing "Single cursor"
    (is (= {:cursor-ranges [(c 0 1)]
            :invalidated-row 0
            :lines ["a"]}
           (insert-text [""]
                        [(c 0 0)]
                        "a")))
    (is (= {:cursor-ranges [(c 0 2)]
            :invalidated-row 0
            :lines ["ab"]}
           (insert-text [""]
                        [(c 0 0)]
                        "ab")))
    (is (= {:cursor-ranges [(c 1 3)]
            :invalidated-row 1
            :lines ["--"
                    "abcd"]}
           (insert-text ["--"
                         "ad"]
                        [(c 1 1)]
                        "bc"))))

  (testing "Multiple cursors"
    (is (= {:cursor-ranges [(c 0 2) (c 1 3)]
            :invalidated-row 0
            :lines ["bc--"
                    "abcd"]}
           (insert-text ["--"
                         "ad"]
                        [(c 0 0) (c 1 1)]
                        "bc")))
    (is (= {:cursor-ranges [(c 1 2) (c 1 5) (c 1 8)]
            :invalidated-row 1
            :lines ["--"
                    "na-na-na"]}
           (insert-text ["--"
                         "--"]
                        [(c 1 0) (c 1 1) (c 1 2)]
                        "na"))))

  (testing "Newline"
    (is (= {:cursor-ranges [(c 1 0)]
            :invalidated-row 0
            :lines [""
                    ""]}
           (insert-text [""]
                        [(c 0 0)]
                        "\n")))
    (is (= {:cursor-ranges [(c 2 0)]
            :invalidated-row 1
            :lines ["first"
                    "sec"
                    "ond"
                    "third"]}
           (insert-text ["first"
                         "second"
                         "third"]
                        [(c 1 3)]
                        "\n")))
    (is (= {:cursor-ranges [(c 1 0) (c 3 0)]
            :invalidated-row 0
            :lines ["one"
                    " two"
                    "one"
                    " two"]}
           (insert-text ["one two"
                         "one two"]
                        [(c 0 3) (c 1 3)]
                        "\n")))
    (is (= {:cursor-ranges [(c 1 0) (c 2 0)]
            :invalidated-row 0
            :lines ["one"
                    " two one"
                    " two"]}
           (insert-text ["one two one two"]
                        [(c 0 3) (c 0 11)]
                        "\n")))))

(defn- append-distinct-lines [lines regions new-lines]
  (data/append-distinct-lines lines regions (layout-info lines) new-lines))

(defn- repeat-region [row line repeat-count]
  (assoc (cr [row 0] [row (count line)])
    :type :repeat
    :count repeat-count))

(defn- other-region [row line]
  (assoc (cr [row 0] [row (count line)])
    :type :other))

(deftest append-distinct-lines-test
  (testing "Appends distinct lines"
    (is (= {:lines ["a" "b"]
            :regions []}
           (append-distinct-lines ["a"] [] ["b"]))))

  (testing "Creates repeat region instead of appending duplicate lines"
    (is (= {:lines ["a"]
            :regions []}
           (append-distinct-lines [""] [] ["a"])))
    (is (= {:lines ["a"]
            :regions [(repeat-region 0 "a" 2)]}
           (append-distinct-lines ["a"] [] ["a"])))
    (is (= {:lines ["a" "boo"]
            :regions [(repeat-region 1 "boo" 2)]}
           (append-distinct-lines ["a" "boo"] [] ["boo"]))))

  (testing "Increases count on existing repeat region"
    (is (= {:lines ["a" "boo"]
            :regions [(repeat-region 1 "boo" 3)]}
           (append-distinct-lines ["a" "boo"] [(repeat-region 1 "boo" 2)] ["boo"]))))

  (testing "Other regions present"
    (is (= {:lines ["a" "boo"]
            :regions [(repeat-region 1 "boo" 3)
                      (other-region 1 "boo")]}
           (append-distinct-lines ["a" "boo"]
                                  [(repeat-region 1 "boo" 2)
                                   (other-region 1 "boo")]
                                  ["boo"])))

    (is (= {:lines ["a" "boo"]
            :regions [(repeat-region 1 "boo" 3)
                      (other-region 1 "boo")]}
           (append-distinct-lines ["a" "boo"]
                                  [(repeat-region 1 "boo" 2)
                                   (other-region 1 "boo")]
                                  ["boo"])))))

(deftest delete-test
  (let [backspace (fn [lines cursor-ranges] (data/delete lines cursor-ranges nil (layout-info lines) data/delete-character-before-cursor))
        delete (fn [lines cursor-ranges] (data/delete lines cursor-ranges nil (layout-info lines) data/delete-character-after-cursor))]
    (testing "Single cursor"
      (is (= {:cursor-ranges [(c 0 0)]
              :invalidated-row 0
              :lines [""]}
             (backspace ["a"]
                        [(c 0 1)])))
      (is (= {:cursor-ranges [(c 0 0)]
              :invalidated-row 0
              :lines [""]}
             (delete ["a"]
                     [(c 0 0)])))
      (is (= {:cursor-ranges [(c 0 1)]
              :invalidated-row 0
              :lines ["ac"]}
             (backspace ["abc"]
                        [(c 0 2)])))
      (is (= {:cursor-ranges [(c 0 1)]
              :invalidated-row 0
              :lines ["ac"]}
             (delete ["abc"]
                     [(c 0 1)])))
      (is (= {:cursor-ranges [(c 0 3)]
              :invalidated-row 0
              :lines ["onetwo"]}
             (backspace ["one"
                         "two"]
                        [(c 1 0)])))
      (is (= {:cursor-ranges [(c 0 3)]
              :invalidated-row 0
              :lines ["onetwo"]}
             (delete ["one"
                      "two"]
                     [(c 0 3)]))))

    (testing "Multiple cursors"
      (is (= {:cursor-ranges [(c 0 1) (c 0 2)]
              :invalidated-row 0
              :lines ["abc"]}
             (backspace ["a"
                         "b"
                         "c"]
                        [(c 1 0) (c 2 0)])))
      (is (= {:cursor-ranges [(c 0 1) (c 0 2)]
              :invalidated-row 0
              :lines ["abc"]}
             (delete ["a"
                      "b"
                      "c"]
                     [(c 0 1) (c 1 1)])))
      (is (= {:cursor-ranges [(c 0 2)]
              :invalidated-row 0
              :lines ["on"]}
             (backspace ["one"
                         ""]
                        [(c 0 3) (c 1 0)]))))))

(defn- str->ranges [str]
  (mapv (fn [[start end]]
          (->CursorRange (->Cursor 0 (/ start 2))
                         (->Cursor 0 (/ end 2))))
        (partition 2 (loop [start 0
                            indices []]
                       (if-some [end (string/index-of str \| start)]
                         (recur (inc end)
                                (if (and (odd? (count indices))
                                         (not= \space (get str (inc end) \space)))
                                  (conj indices end end)
                                  (conj indices end)))
                         indices)))))

(defn- make-random-cursors [count]
  (into []
        (comp (distinct)
              (take count))
        (repeatedly #(->Cursor (rand-int 10) (rand-int 100)))))

(deftest try-merge-cursor-range-pair-test
  (let [try-merge-cursor-range-pair #'data/try-merge-cursor-range-pair]

    ;;  =>  <=
    (doseq [_ (range 10)]
      (let [[from to] (make-random-cursors 2)
            a (->CursorRange from to)]
        (is (= a (try-merge-cursor-range-pair a a)))))

    ;;  =>  <A   <B
    (doseq [_ (range 10)]
      (let [[from a-to b-to] (sort (make-random-cursors 3))
            a (->CursorRange from a-to)
            b (->CursorRange from b-to)]
        (is (= b (try-merge-cursor-range-pair a b)))))

    ;;  =>  <B   <A
    (doseq [_ (range 10)]
      (let [[from b-to a-to] (sort (make-random-cursors 3))
            a (->CursorRange from a-to)
            b (->CursorRange from b-to)]
        (is (= a (try-merge-cursor-range-pair a b)))))

    ;;  A>   B>  <=
    (doseq [_ (range 10)]
      (let [[a-from b-from to] (sort (make-random-cursors 3))
            a (->CursorRange a-from to)
            b (->CursorRange b-from to)]
        (is (= a (try-merge-cursor-range-pair a b)))))

    ;;  B>   A>  <=
    (doseq [_ (range 10)]
      (let [[b-from a-from to] (sort (make-random-cursors 3))
            a (->CursorRange a-from to)
            b (->CursorRange b-from to)]
        (is (= b (try-merge-cursor-range-pair a b)))))

    ;;  A>  <A    B>  <B
    (doseq [_ (range 10)]
      (let [[a-from a-to b-from b-to] (sort (make-random-cursors 4))
            a (->CursorRange a-from a-to)
            b (->CursorRange b-from b-to)]
        (is (nil? (try-merge-cursor-range-pair a b)))))

    ;;  A>   B>  <A   <B
    (doseq [_ (range 10)]
      (let [[a-from b-from a-to b-to] (sort (make-random-cursors 4))
            a (->CursorRange a-from a-to)
            b (->CursorRange b-from b-to)]
        (is (= (->CursorRange a-from b-to) (try-merge-cursor-range-pair a b)))))

    ;;  A>   B>  <B   <A
    (doseq [_ (range 10)]
      (let [[a-from b-from b-to a-to] (sort (make-random-cursors 4))
            a (->CursorRange a-from a-to)
            b (->CursorRange b-from b-to)]
        (is (= a (try-merge-cursor-range-pair a b)))))

    ;;  B>  <B    A>  <A
    (doseq [_ (range 10)]
      (let [[b-from b-to a-from a-to] (sort (make-random-cursors 4))
            a (->CursorRange a-from a-to)
            b (->CursorRange b-from b-to)]
        (is (nil? (try-merge-cursor-range-pair a b)))))

    ;;  B>   A>  <B   <A
    (doseq [_ (range 10)]
      (let [[b-from a-from b-to a-to] (sort (make-random-cursors 4))
            a (->CursorRange a-from a-to)
            b (->CursorRange b-from b-to)]
        (is (= (->CursorRange b-from a-to) (try-merge-cursor-range-pair a b)))))

    ;;  B>   A>  <A   <B
    (doseq [_ (range 10)]
      (let [[b-from a-from a-to b-to] (sort (make-random-cursors 4))
            a (->CursorRange a-from a-to)
            b (->CursorRange b-from b-to)]
        (is (= b (try-merge-cursor-range-pair a b)))))

    ;;  A>  <=>  <B
    (doseq [_ (range 10)]
      (let [[a-from edge b-to] (sort (make-random-cursors 3))
            a (->CursorRange a-from edge)
            b (->CursorRange edge b-to)]
        (is (nil? (try-merge-cursor-range-pair a b)))))

    ;;  B>  <=>  <A
    (doseq [_ (range 10)]
      (let [[b-from edge a-to] (sort (make-random-cursors 3))
            a (->CursorRange edge a-to)
            b (->CursorRange b-from edge)]
        (is (nil? (try-merge-cursor-range-pair a b)))))

    ;;  A>  <=>
    (doseq [_ (range 10)]
      (let [[a-from edge] (sort (make-random-cursors 3))
            a (->CursorRange a-from edge)
            b (->CursorRange edge edge)]
        (is (= a (try-merge-cursor-range-pair a b)))))

    ;;  B>  <=>
    (doseq [_ (range 10)]
      (let [[b-from edge] (sort (make-random-cursors 3))
            a (->CursorRange edge edge)
            b (->CursorRange b-from edge)]
        (is (= b (try-merge-cursor-range-pair a b)))))

    ;; <=>  <A
    (doseq [_ (range 10)]
      (let [[edge a-to] (sort (make-random-cursors 2))
            a (->CursorRange edge a-to)
            b (->CursorRange edge edge)]
        (is (= a (try-merge-cursor-range-pair a b)))))

    ;; <=>  <B
    (doseq [_ (range 10)]
      (let [[edge b-to] (sort (make-random-cursors 2))
            a (->CursorRange edge edge)
            b (->CursorRange edge b-to)]
        (is (= b (try-merge-cursor-range-pair a b)))))

    (testing "Preserves extra data"
      (let [metadata {:my-metadata-prop :my-metadata-value}
            [a-from b-from a-to b-to] (sort (make-random-cursors 4))
            a (->CursorRange a-from a-to)
            b (with-meta (assoc (->CursorRange b-from b-to) :my-prop :my-prop-value) metadata)
            b' (try-merge-cursor-range-pair a b)]
        (is (= (:my-prop b) (:my-prop b')))
        (is (identical? (meta b) (meta b')))))))

(deftest merge-cursor-ranges-test
  (let [merge-cursor-ranges #'data/merge-cursor-ranges]
    (are [_ before added after]
      (is (= (str->ranges after)
             (merge-cursor-ranges (sort (into (str->ranges before)
                                              (str->ranges added))))))
      "0 1 2 3 4 5 6 7 8 9 A B"
      "        |x x|   |x x|  "
      "|---|                  "
      "|x x|   |x x|   |x x|  "

      "0 1 2 3 4 5 6 7 8 9 A B"
      "        |x x|   |x x|  "
      "  |---|                "
      "  |x x| |x x|   |x x|  "

      "0 1 2 3 4 5 6 7 8 9 A B"
      "        |x x|   |x x|  "
      "    |---|              "
      "    |x x|x x|   |x x|  "

      "0 1 2 3 4 5 6 7 8 9 A B"
      "        |x x|   |x x|  "
      "      |---|            "
      "      |x x x|   |x x|  "

      "0 1 2 3 4 5 6 7 8 9 A B"
      "        |x x|   |x x|  "
      "        |---|          "
      "        |x x|   |x x|  "

      "0 1 2 3 4 5 6 7 8 9 A B"
      "        |x x|   |x x|  "
      "          |---|        "
      "        |x x x| |x x|  "

      "0 1 2 3 4 5 6 7 8 9 A B"
      "        |x x|   |x x|  "
      "            |---|      "
      "        |x x|x x|x x|  "

      "0 1 2 3 4 5 6 7 8 9 A B"
      "        |x x|   |x x|  "
      "              |---|    "
      "        |x x| |x x x|  "

      "0 1 2 3 4 5 6 7 8 9 A B"
      "        |x x|   |x x|  "
      "                |---|  "
      "        |x x|   |x x|  "

      "0 1 2 3 4 5 6 7 8 9 A B"
      "        |x x|   |x x|  "
      "                  |---|"
      "        |x x|   |x x x|"

      "0 1 2 3 4 5 6 7 8 9 A B"
      "        |x x|   |x x|  "
      "              |-------|"
      "        |x x| |x x x x|"

      "0 1 2 3 4 5 6 7 8 9 A B"
      "        |x x|   |x x|  "
      "            |---------|"
      "        |x x|x x x x x|"

      "0 1 2 3 4 5 6 7 8 9 A B"
      "        |x x|   |x x|  "
      "          |-----------|"
      "        |x x x x x x x|"

      "0 1 2 3 4 5 6 7 8 9 A B"
      "        |x x|   |x x|  "
      "        |-------------|"
      "        |x x x x x x x|"

      "0 1 2 3 4 5 6 7 8 9 A B"
      "        |x x|   |x x|  "
      "      |---------------|"
      "      |x x x x x x x x|"

      "0 1 2 3 4 5 6 7 8 9 A B"
      "        |x x|   |x x|  "
      "        |-----------|  "
      "        |x x x x x x|  "

      "0 1 2 3 4 5 6 7 8 9 A B"
      "        |x x|   |x x|  "
      "          |-------|    "
      "        |x x x x x x|  ")

    (is (= [(cr [0 0] [1 1])
            (cr [1 1] [2 2])]
           (merge-cursor-ranges [(cr [0 0] [1 1])
                                 (cr [1 1] [2 2])])))

    (is (= [(cr [0 0] [2 2])]
           (merge-cursor-ranges [(cr [0 0] [1 2])
                                 (cr [1 1] [2 2])])))

    (is (= [(cr [2 2] [0 0])]
           (merge-cursor-ranges [(cr [1 2] [0 0])
                                 (cr [2 2] [1 1])])))

    (testing "Region merge"
      (testing "Same types"
        (let [a (assoc (cr [0 0] [0 3]) :type :same)
              b (assoc (cr [0 1] [0 2]) :type :same)]
          (is (= [a] (merge-cursor-ranges [a b])))))
      (testing "Different types"
        (let [a (assoc (cr [0 0] [0 3]) :type :a)
              b (assoc (cr [0 1] [0 2]) :type :b)]
          (is (= [a b] (merge-cursor-ranges [a b]))))))))

(deftest concat-cursor-ranges-test
  (let [concat-cursor-ranges #'data/concat-cursor-ranges]
    (is (= (str->ranges                       "|x x x|   |x x x x x x|")
           (concat-cursor-ranges (str->ranges "                |x x x|")
                                 (str->ranges "|x x|         |x|      ")
                                 (str->ranges "  |x x|   |x x|     |x|")
                                 (str->ranges "|x|         |x x x|    ")
                                 (str->ranges "          |x x x| |x|  "))))))

(deftest cut-paste-test
  (let [indent-level-pattern (data/indent-level-pattern 4)
        indent-string "    "
        grammar nil
        clipboard (make-test-clipboard {})
        cut! (fn [lines cursor-ranges] (data/cut! lines cursor-ranges nil (layout-info lines) clipboard))
        paste! (fn [lines cursor-ranges] (data/paste indent-level-pattern indent-string grammar lines cursor-ranges nil (layout-info lines) clipboard))
        mime-type (var-get #'data/clipboard-mime-type-multi-selection)
        clipboard-content (fn [] (data/get-content clipboard mime-type))]
    (is (= {:cursor-ranges [(cr [0 0] [0 0])
                            (cr [0 3] [0 3])]
            :invalidated-row 0
            :lines ["two"]}
           (cut! ["one"
                  "two"
                  "three"]
                 [(cr [0 0] [1 0])
                  (cr [1 3] [2 5])])))
    (is (= [["one"
             ""]
            [""
             "three"]]
           (clipboard-content)))
    (is (= {:cursor-ranges [(cr [1 0] [1 0])
                            (cr [2 5] [2 5])]
            :invalidated-row 0
            :lines ["one"
                    "two"
                    "three"]}
           (paste! ["two"]
                   [(cr [0 0] [0 0])
                    (cr [0 3] [0 3])])))))

(deftest word-cursor-range-at-cursor-test
  (let [word-cursor-range-at-cursor #'data/word-cursor-range-at-cursor
        cases [["one two"
                [0 3] [4 7]]
               ["UpperCamelCase.lowerCamelCase(snake_case, SCREAMING_SNAKE_CASE, 'kebab-case')"
                [0 14] [15 29] [30 40] [42 62] [65 70] [71 75]]]]
    (doseq [[line & col-ranges] cases
            [start-col end-col] col-ranges
            col (range start-col (inc end-col))]
      (is (= (cr [0 start-col] [0 end-col])
             (word-cursor-range-at-cursor [line] (->Cursor 0 col)))))

    (testing "Whitespace"
      (let [lines ["\t  \t  word  \t  \t"]]
        (doseq [col (range 6)]
          (is (= (cr [0 0] [0 6])
                 (word-cursor-range-at-cursor lines (->Cursor 0 col)))))
        (doseq [col (range 6 10)]
          (is (= (cr [0 6] [0 10])
                 (word-cursor-range-at-cursor lines (->Cursor 0 col)))))
        (doseq [col (range 11 16)] ;; 11 because the end of "word" has priority.
          (is (= (cr [0 10] [0 16])
                 (word-cursor-range-at-cursor lines (->Cursor 0 col)))))))))

(defn- find-prev-occurrence [haystack-lines needle-lines from-cursor]
  (data/find-prev-occurrence haystack-lines needle-lines from-cursor false false))

(deftest find-prev-occurrence-test
  (are [expected haystack-lines needle-lines]
    (= expected
       (find-prev-occurrence haystack-lines needle-lines (data/document-end-cursor haystack-lines)))

    nil
    ["one" "two"]
    ["not-found"]

    nil
    ["one" "two"]
    ["on" ""]

    nil
    ["one" "two three"]
    ["" "three"]

    (cr [0 0] [0 3])
    ["one" "two"]
    ["one"]

    (cr [0 1] [0 4])
    ["none" "to"]
    ["one"]

    (cr [1 0] [1 3])
    ["one" "two"]
    ["two"]

    (cr [0 0] [1 3])
    ["one" "two"]
    ["one" "two"]

    (cr [1 1] [2 2])
    ["one" "none" "tower"]
    ["one" "to"]

    (cr [0 4] [1 0])
    ["one one" "two"]
    ["one" ""])

  (is (= (cr [0 0] [0 3])
         (find-prev-occurrence ["one one"] ["one"] (->Cursor 0 4))))
  (is (= (cr [0 0] [0 3])
         (find-prev-occurrence ["one" "two" "one"] ["one"] (->Cursor 2 2)))))

(defn- find-next-occurrence [haystack-lines needle-lines from-cursor]
  (data/find-next-occurrence haystack-lines needle-lines from-cursor false false))

(deftest find-next-occurrence-test
  (is (= nil
         (find-next-occurrence ["one" "two"] ["not-found"] (->Cursor 0 0))))
  (is (= nil
         (find-next-occurrence ["one" "two"] ["on" ""] (->Cursor 0 0))))
  (is (= nil
         (find-next-occurrence ["one" "two three"] ["" "three"] (->Cursor 0 0))))
  (is (= (cr [0 0] [0 3])
         (find-next-occurrence ["one" "two"] ["one"] (->Cursor 0 0))))
  (is (= (cr [0 1] [0 4])
         (find-next-occurrence ["none" "to"] ["one"] (->Cursor 0 0))))
  (is (= (cr [1 0] [1 3])
         (find-next-occurrence ["one" "two"] ["two"] (->Cursor 0 0))))
  (is (= (cr [0 0] [1 3])
         (find-next-occurrence ["one" "two"] ["one" "two"] (->Cursor 0 0))))
  (is (= (cr [1 1] [2 2])
         (find-next-occurrence ["one" "none" "tower"] ["one" "to"] (->Cursor 0 0))))
  (is (= (cr [2 0] [2 3])
         (find-next-occurrence ["one" "two" "one"] ["one"] (->Cursor 0 1))))
  (is (= (cr [0 4] [1 0])
         (find-next-occurrence ["one one" "two"] ["one" ""] (->Cursor 0 0)))))

(deftest select-next-occurrence-test
  (let [select-next-occurrence (fn [lines cursor-ranges]
                                   (:cursor-ranges (data/select-next-occurrence lines cursor-ranges (layout-info lines))))]
    (testing "Selects word under cursor"
      (is (= [(cr [0 0] [0 3])] (select-next-occurrence ["one word"]
                                                        [(c 0 1)])))
      (is (= [(cr [0 0] [0 3])
              (cr [1 5] [1 10])] (select-next-occurrence ["one word"
                                                          "more words"]
                                                         [(c 0 1) (c 1 8)]))))
    (testing "Selects next occurrence"
      (is (= [(cr [0 0] [0 4])
              (cr [0 5] [0 9])]
             (select-next-occurrence ["word word word"]
                                     [(cr [0 0] [0 4])])))
      (is (= [(cr [0 0] [0 4])
              (cr [0 5] [0 9])
              (cr [0 10] [0 14])]
             (select-next-occurrence ["word word word"]
                                     [(cr [0 0] [0 4])
                                      (cr [0 5] [0 9])]))))
    (testing "Respects cursor range direction"
      (is (= [(cr [0 4] [0 0])
              (cr [0 9] [0 5])]
             (select-next-occurrence ["word word word"]
                                     [(cr [0 4] [0 0])]))))
    (testing "Loops around"
      (is (= [(cr [0 0] [0 4])
              (cr [0 5] [0 9])
              (cr [0 10] [0 14])]
             (->> [(cr [0 10] [0 14])]
                  (select-next-occurrence ["word word word"])
                  (select-next-occurrence ["word word word"])
                  (select-next-occurrence ["word word word"])))))))

(deftest find-matching-braces-test
  (is (nil? (data/find-matching-braces ["x"] (->Cursor 0 0))))
  (is (nil? (data/find-matching-braces ["x"] (->Cursor 0 1))))
  (is (nil? (data/find-matching-braces ["("] (->Cursor 0 0))))
  (is (nil? (data/find-matching-braces ["("] (->Cursor 0 1))))
  (is (nil? (data/find-matching-braces [")"] (->Cursor 0 0))))
  (is (nil? (data/find-matching-braces [")"] (->Cursor 0 1))))

  (is (= [(cr [0 0] [0 1]) (cr [0 1] [0 2])] (data/find-matching-braces ["()"] (->Cursor 0 0))))
  (is (= [(cr [0 1] [0 2]) (cr [0 0] [0 1])] (data/find-matching-braces ["()"] (->Cursor 0 1))))
  (is (= [(cr [0 1] [0 2]) (cr [0 0] [0 1])] (data/find-matching-braces ["()"] (->Cursor 0 2))))
  (is (= [(cr [0 0] [0 1]) (cr [0 1] [0 2])] (data/find-matching-braces ["[]"] (->Cursor 0 0))))
  (is (= [(cr [0 1] [0 2]) (cr [0 0] [0 1])] (data/find-matching-braces ["[]"] (->Cursor 0 1))))
  (is (= [(cr [0 1] [0 2]) (cr [0 0] [0 1])] (data/find-matching-braces ["[]"] (->Cursor 0 2))))
  (is (= [(cr [0 0] [0 1]) (cr [0 1] [0 2])] (data/find-matching-braces ["{}"] (->Cursor 0 0))))
  (is (= [(cr [0 1] [0 2]) (cr [0 0] [0 1])] (data/find-matching-braces ["{}"] (->Cursor 0 1))))
  (is (= [(cr [0 1] [0 2]) (cr [0 0] [0 1])] (data/find-matching-braces ["{}"] (->Cursor 0 2))))

  (is (= [(cr [0 0] [0 1]) (cr [0 3] [0 4])]
         (data/find-matching-braces ["(())"]
                                    (->Cursor 0 0))))
  (is (= [(cr [0 1] [0 2]) (cr [0 2] [0 3])]
         (data/find-matching-braces ["(())"]
                                    (->Cursor 0 1))))
  (is (= [(cr [0 2] [0 3]) (cr [0 1] [0 2])]
         (data/find-matching-braces ["(())"]
                                    (->Cursor 0 2))))
  (is (= [(cr [0 2] [0 3]) (cr [0 1] [0 2])]
         (data/find-matching-braces ["(())"]
                                    (->Cursor 0 3))))
  (is (= [(cr [0 3] [0 4]) (cr [0 0] [0 1])]
         (data/find-matching-braces ["(())"]
                                    (->Cursor 0 4))))
  (is (= [(cr [0 0] [0 1]) (cr [1 0] [1 1])]
         (data/find-matching-braces ["["
                                     "]"]
                                    (->Cursor 0 0))))
  (is (= [(cr [0 0] [0 1]) (cr [1 0] [1 1])]
         (data/find-matching-braces ["["
                                     "]"]
                                    (->Cursor 0 1))))
  (is (= [(cr [1 0] [1 1]) (cr [0 0] [0 1])]
         (data/find-matching-braces ["["
                                     "]"]
                                    (->Cursor 1 0))))
  (is (= [(cr [1 0] [1 1]) (cr [0 0] [0 1])]
         (data/find-matching-braces ["["
                                     "]"]
                                    (->Cursor 1 1))))

  (are [cursor matching-range]
    (= matching-range
       (second (data/find-matching-braces ["function foo() {"
                                           "    a[0] = {"
                                           "        rand(3)"
                                           "    }"
                                           "}"]
                                          cursor)))
    (->Cursor 0 0) nil
    (->Cursor 0 11) nil
    (->Cursor 0 12) (cr [0 13] [0 14])
    (->Cursor 0 13) (cr [0 12] [0 13])
    (->Cursor 0 14) (cr [0 12] [0 13])
    (->Cursor 0 15) (cr [4 0] [4 1])
    (->Cursor 0 16) (cr [4 0] [4 1])

    (->Cursor 1 0) nil
    (->Cursor 1 4) nil
    (->Cursor 1 5) (cr [1 7] [1 8])
    (->Cursor 1 6) (cr [1 7] [1 8])
    (->Cursor 1 7) (cr [1 5] [1 6])
    (->Cursor 1 8) (cr [1 5] [1 6])
    (->Cursor 1 9) nil
    (->Cursor 1 10) nil
    (->Cursor 1 11) (cr [3 4] [3 5])
    (->Cursor 1 12) (cr [3 4] [3 5])

    (->Cursor 2 0) nil
    (->Cursor 2 11) nil
    (->Cursor 2 12) (cr [2 14] [2 15])
    (->Cursor 2 13) (cr [2 14] [2 15])
    (->Cursor 2 14) (cr [2 12] [2 13])
    (->Cursor 2 15) (cr [2 12] [2 13])

    (->Cursor 3 0) nil
    (->Cursor 3 3) nil
    (->Cursor 3 4) (cr [1 11] [1 12])
    (->Cursor 3 5) (cr [1 11] [1 12])

    (->Cursor 4 0) (cr [0 15] [0 16])
    (->Cursor 4 1) (cr [0 15] [0 16])))
