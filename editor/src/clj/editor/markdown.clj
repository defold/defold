;; Copyright 2020-2024 The Defold Foundation
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

(ns editor.markdown
  (:require [cljfx.api :as fx]
            [cljfx.fx.column-constraints :as fx.column-constraints]
            [cljfx.fx.grid-pane :as fx.grid-pane]
            [cljfx.fx.h-box :as fx.h-box]
            [cljfx.fx.region :as fx.region]
            [cljfx.fx.scroll-pane :as fx.scroll-pane]
            [cljfx.fx.stack-pane :as fx.stack-pane]
            [cljfx.fx.svg-path :as fx.svg-path]
            [cljfx.fx.text :as fx.text]
            [cljfx.fx.text-flow :as fx.text-flow]
            [cljfx.fx.v-box :as fx.v-box]
            [cljfx.lifecycle :as fx.lifecycle]
            [cljfx.mutator :as fx.mutator]
            [cljfx.prop :as fx.prop]
            [clojure.string :as string]
            [dynamo.graph :as g]
            [editor.defold-project :as project]
            [editor.fxui :as fxui]
            [editor.html-view :as html-view]
            [editor.resource-io :as resource-io]
            [editor.resource-node :as resource-node]
            [editor.ui :as ui]
            [editor.workspace :as workspace]
            [util.fn :as fn])
  (:import [java.net URI]
           [javafx.scene.control ScrollPane]
           [org.commonmark.ext.autolink AutolinkExtension]
           [org.commonmark.internal CustomHtmlBlockParser$Factory]
           [org.commonmark.parser Parser]
           [org.commonmark.renderer.html HtmlRenderer]
           [org.jsoup Jsoup]
           [org.jsoup.helper StringUtil]
           [org.jsoup.nodes Element Node TextNode]))

(set! *warn-on-reflection* true)

(defn- child-nodes [node]
  (when (instance? Element node)
    (.childNodes ^Element node)))

(defn- tag-name [^Node node]
  (.nodeName node))

(defn- attr [node name]
  (when (instance? Element node)
    (.attr ^Element node name)))

(defn- open-link! [^URI base-url project ^String url _event]
  (when-let [^URI resolved-url (try
                                 (if base-url (.resolve base-url url) (URI. url))
                                 (catch Exception _ nil))]
    (case (.getScheme resolved-url)
      "defold"
      (html-view/dispatch-url! project resolved-url)

      "file"
      (if-let [resource (g/with-auto-evaluation-context evaluation-context
                          (let [workspace (project/workspace project evaluation-context)]
                            (when-let [proj-path (workspace/as-proj-path workspace resolved-url evaluation-context)]
                              (workspace/find-resource workspace proj-path))))]
        (ui/execute-command (ui/contexts (ui/main-scene)) :open {:resources [resource]})
        (ui/open-url resolved-url))

      (ui/open-url resolved-url))))

(defn- text-view
  ([ctx]
   (-> ctx
       (dissoc :paragraph :base-url :project)
       (assoc :fx/type fx.text/lifecycle)))
  ([text ctx]
   (text-view (assoc ctx :text text))))

(defn- style [ctx style-class]
  (update ctx :style-class (fnil conj []) (str "md-" style-class)))

;; region Layout logic

;; Layout:
;; Html layout is implemented as a VBox of TextFlows, interposed with separator
;; Regions. Similarly to web browsers, we hide blank content as much as
;; possible. We support 2 modes of text layout:
;; - paragraph (<p>, default) - normalize all whitespace, don't start content
;;   with whitespace, etc.
;; - pre-formatted (<pre>) - insert whitespace and empty lines as defined, but
;;   still try to remove trailing/leading empty lines
;; Layout process is a sequential recursive reduction over html tree with acc
;; and ctx, where acc is a map with the following keys:
;;   :content              vector of content items, an interposition of either:
;;                         - non-empty text flow children vector
;;                         - number in range 1..6 (separator size)
;;                         Invariants: content is either empty, or begins and
;;                         end with a text flow children vector
;;   :whitespace-prefix    ctx with preset text that should be appended to text
;;                         flow children before another non-empty element, can
;;                         be nil
;; ...and ctx is a map of Text cljfx props with following extra keys:
;;   :paragraph    boolean text layout mode indicator
;;   :base-url     URI used for resolving clicked links
;;   :project      defold project node id, used for opening defold:// links
;; acc content is then used to build VBox of TextFlows and Regions.
;; Note that many views may return nil to indicate they are empty: we use nils
;; to skip empty content.

(defn- add-view
  "Add (nilable) view into the text-flow children vector, adding accumulated
  whitespace if it exists"
  [{:keys [content whitespace-prefix] :as acc} view]
  (if view
    (let [separator (number? (or (peek content) 0))
          content (cond-> content separator (conj []))]
      (assoc acc
        :content
        (update content (dec (count content))
                (fn [views]
                  (-> views
                      (cond-> whitespace-prefix (conj (text-view whitespace-prefix)))
                      (conj view))))
        :whitespace-prefix nil))
    acc))

(defn- add-text
  "Add text (if not blank) with the formatting specified in ctx"
  [{:keys [content whitespace-prefix] :as acc} ^String text {:keys [paragraph] :as ctx}]
  (let [newline (number? (or (peek content) 0))
        ^String left-normalized-text
        (cond-> text
                paragraph StringUtil/normaliseWhitespace    ;; \s+ => \s
                (and newline paragraph) string/triml
                (and newline (not paragraph)) (string/replace #"^(\s)+\n" ""))
        ;; on the right, we don't add the trailing whitespace, but instead we save it
        ;; to insert later
        normalized-text (string/trimr left-normalized-text)
        trailing-whitespace (if paragraph
                              (when-not (= (.length left-normalized-text) (.length normalized-text))
                                " ")
                              (let [trimmed-whitespace (subs left-normalized-text (count normalized-text))]
                                (when-not (.isEmpty trimmed-whitespace)
                                  trimmed-whitespace)))]
    (if (pos? (.length normalized-text))
      (-> acc
          (add-view (text-view normalized-text ctx))
          (assoc :whitespace-prefix (when trailing-whitespace (assoc ctx :text trailing-whitespace))))
      (cond-> acc
              trailing-whitespace
              (assoc :whitespace-prefix (if paragraph
                                          (or whitespace-prefix (assoc ctx :text trailing-whitespace))
                                          (if whitespace-prefix
                                            (assoc ctx :text (str (:text whitespace-prefix) trailing-whitespace))
                                            (assoc ctx :text trailing-whitespace))))))))

(defn- add-separator
  "Add separator after text-flow-children vector; if there is already a
  separator, keep the highest separator"
  [{:keys [content] :as acc} n]
  (let [separator (when-let [end (or (peek content) ##Inf)]
                    (when (number? end) end))]
    (cond
      (not separator) (assoc acc :content (conj content n) :whitespace-prefix nil)
      (< separator n) (assoc acc :content (assoc content (dec (count content)) n) :whitespace-prefix nil)
      :else acc)))

(defn- with-separators
  "Layout content within before/after separators (numbers in 1..6 range)

  If the content turns out to be empty, no separators are added in the end"
  [acc before after f & args]
  (let [before-acc (add-separator acc before)
        f-acc (apply f before-acc args)]
    (if (identical? (:content before-acc) (:content f-acc))
      acc
      (add-separator f-acc after))))

;; endregion

(declare children-section-view)

(defn- indent-view [indent view]
  {:pre [indent]}
  (when view
    {:fx/type fx.h-box/lifecycle
     :children [indent view]}))

(defn- indent-with-text-view [text ctx view]
  (when view
    (indent-view {:fx/type fx.stack-pane/lifecycle
                  :alignment :top-left
                  :style-class "md-paragraph"
                  :children [(text-view text ctx)]}
                 view)))

(defn- unordered-list-view [node ctx]
  (let [lis (into []
                  (comp
                    (filter #(= "li" (tag-name %)))
                    (keep #(indent-with-text-view "\u00A0\u00A0\u2022\u00A0" ctx
                                                  (children-section-view % ctx))))
                  (child-nodes node))]
    (when (pos? (count lis))
      {:fx/type fx.v-box/lifecycle
       :children lis})))

(defn- code-block-view [node ctx]
  (when-let [view (children-section-view node (-> ctx (style "code") (assoc :paragraph false)))]
    {:fx/type fx.v-box/lifecycle
     :style-class "md-code-block"
     :children [view]}))

(defn- icon-view [icon]
  {:fx/type fx.region/lifecycle
   :style-class "md-icon"
   :pref-width (case icon
                 ("alert" "attention") 15
                 "android" 12
                 "html5" 12
                 "ios" 12
                 "linux" 12
                 ("macos" "osx") 14
                 "windows" 13)
   :scale-y -1
   :shape {:fx/type fx.svg-path/lifecycle
           :content (case icon
                      ;; All icon paths are extracted from fontello.svg font used on our site (MIT-licensed)
                      ;; see https://github.com/defold/defold.github.io/blob/master/fonts
                      "android" "M386-134c-3 1-7 1-11 3a53 53 0 0 0-36 51q0 64 0 128c0 6 0 6-6 6h-47a54 54 0 0 0-54 46 64 64 0 0 0 0 11q0 197 0 395c0 6 0 6 6 6h525c6 0 6 0 6-6 0-132 0-264 0-396a55 55 0 0 0-43-55 76 76 0 0 0-13-1h-46c-6 0-6 0-6-5 0-42 0-85 0-127a55 55 0 0 0-46-55l-2 0h-13l-1 0a55 55 0 0 0-47 57c1 41 0 82 0 123 0 7 0 7-7 7h-92c-8 0-7 1-7-7 0-42 0-84 0-126a52 52 0 0 0-32-51 110 110 0 0 0-16-4z m280 968a64 64 0 0 0 13-6 27 27 0 0 0 4-37c-10-14-19-28-28-43l-3-4 4-3a227 227 0 0 0 88-91 222 222 0 0 0 24-81c0-3 0-5-4-4h-526c-7 0-7 0-6 7a218 218 0 0 0 52 120 230 230 0 0 0 59 49c4 2 4 2 1 7-9 15-18 29-28 44a27 27 0 0 0 9 38 58 58 0 0 0 10 4h7a33 33 0 0 0 23-17c10-16 20-31 31-48 1-2 2-3 5-3a320 320 0 0 0 75 14 349 349 0 0 0 122-13 5 5 0 0 1 6 2c11 16 21 32 32 47a38 38 0 0 0 22 18z m-58-150a27 27 0 1 1 26-27 27 27 0 0 1-26 27z m-216-54a27 27 0 1 1-26 27 27 27 0 0 1 26-27z m-322-293c0 40 0 80 0 120a54 54 0 0 0 107 2c0-18 0-37 0-56q0-93 0-187a54 54 0 0 0-107-3c0 41 0 82 0 123z m861 0c0-41 0-81 0-121a53 53 0 0 0-46-53 54 54 0 0 0-62 50c0 3 0 6 0 9q0 117 0 234a54 54 0 0 0 107 3c0-41 0-82 0-123z"
                      ("alert" "attention") "M571 83v106q0 8-5 13t-12 5h-108q-7 0-12-5t-5-13v-106q0-8 5-13t12-6h108q7 0 12 6t5 13z m-1 208l10 257q0 6-5 10-7 6-14 6h-122q-6 0-14-6-5-4-5-12l9-255q0-5 6-9t13-3h103q8 0 14 3t5 9z m-7 522l428-786q20-35-1-70-9-17-26-26t-35-10h-858q-18 0-35 10t-26 26q-21 35-1 70l429 786q9 17 26 27t36 10 36-10 27-27z"
                      "ios" "M669 821a182 182 0 0 0 0-45 206 206 0 0 0-76-133 155 155 0 0 0-110-38 132 132 0 0 0 1 48 209 209 0 0 0 91 135 212 212 0 0 0 82 31 98 98 0 0 0 12 2z m197-690c-11-24-20-49-31-73a596 596 0 0 0-74-113 188 188 0 0 0-52-47 118 118 0 0 0-102-6c-11 4-22 8-31 13a186 186 0 0 1-157-4 200 200 0 0 0-63-18 104 104 0 0 0-78 27 442 442 0 0 0-83 100 547 547 0 0 0-62 128 503 503 0 0 0-29 220 273 273 0 0 0 59 148 221 221 0 0 0 179 85 188 188 0 0 0 61-13c17-6 34-12 51-20a98 98 0 0 1 79 4 382 382 0 0 0 94 30 231 231 0 0 0 132-22 205 205 0 0 0 77-62c1-2 2-4 4-7-67-47-106-108-100-192s51-140 126-178z"
                      "html5" "M502-134h-5a29 29 0 0 1-3 1l-339 96-3 1a4 4 0 0 0 0 1l-4 43-4 47-5 48-3 37-4 47-4 38-4 47-3 38-5 46-3 38-4 47-3 38-5 47-4 46-4 47-3 38-4 46-4 39-4 48c-1 12-2 24-3 36a22 22 0 0 1-1 5v3h853v-3c0-2 0-3 0-4s0-6 0-9q-1-14-3-29l-4-37-4-47-5-46-4-47-3-37-4-47-4-46-5-47-4-46q-2-23-4-47c-1-16-2-31-4-47-1-18-3-37-5-55s-3-37-5-55c-2-21-4-42-6-63s-3-43-5-64q-4-42-8-84c0-3 0-6-1-8l-3-2-333-93z m-269 790c0-1 0-2 0-3 1-12 1-23 2-35 2-18 4-36 5-54 2-16 3-31 4-47 2-18 4-36 5-55s4-36 5-54q4-37 7-73c0-1 0-2 0-3h371l-13-138-4-1-113-31a11 11 0 0 0-6 0l-112 32-4 1-7 85h-106l13-168 4-1 210-58a20 20 0 0 1 11 0l190 52 24 6c0 2 0 4 0 5q1 10 2 20 2 24 4 48c1 15 3 31 4 47s3 31 4 47 3 32 4 47c2 19 4 38 5 56 2 16 3 33 5 49 0 3 0 6 1 9h-389c-1 9-9 107-8 110h407l1 9c2 18 4 35 5 52 2 13 3 27 4 41a26 26 0 0 1 0 3z"
                      "linux" "M674-134a72 72 0 0 0-48 38 13 13 0 0 1-8 5 465 465 0 0 1-166 5c-14-2-29-4-42-7a19 19 0 0 1-11-8 56 56 0 0 0-56-26 455 455 0 0 0-59 14c-34 10-68 21-102 31-20 5-41 8-63 12a57 57 0 0 0-13 6 27 27 0 0 0-16 38c3 10 6 20 9 30a67 67 0 0 1 0 41 106 106 0 0 0-5 21 31 31 0 0 0 26 36c13 4 26 7 39 11a66 66 0 0 1 38 33 206 206 0 0 0 19 24 11 11 0 0 1 3 8 94 94 0 0 0 8 63c14 31 26 62 37 95a339 339 0 0 0 54 97c11 14 21 30 33 43a90 90 0 0 1 21 65c-1 44-5 87-5 130a363 363 0 0 0 6 72 98 98 0 0 0 58 72 163 163 0 0 0 142 6 133 133 0 0 0 68-73 323 323 0 0 0 22-115c1-21 2-42 4-62a157 157 0 0 1 29-76q52-78 103-156a215 215 0 0 0 27-177 14 14 0 0 1 3-11 73 73 0 0 0 18-49 53 53 0 0 1 27-49 156 156 0 0 1 15-8c29-13 32-42 6-61a226 226 0 0 0-20-12l-113-68a38 38 0 0 1-7-5 107 107 0 0 0-57-33z m-300 152c12-14 24-28 36-41a19 19 0 0 1 11-6c25 0 49 0 74 0a189 189 0 0 1 141 73 24 24 0 0 1 5 14c2 28 3 57 4 86a16 16 0 0 0 16 17 214 214 0 0 0 23 0 44 44 0 0 1 2 5 269 269 0 0 1-4 139 1586 1586 0 0 1-91 219c-11 22-13 22-34 11s-47-25-70-39a49 49 0 0 0-60 5 168 168 0 0 0-17 14c-5 6-10 12-15 18a163 163 0 0 0-30-94 231 231 0 0 1-21-38c-12-29-22-60-36-88a242 242 0 0 1-27-86c-3-33 2-64 30-86 4-4 7-8 12-11 19-16 39-31 57-47a71 71 0 0 0 19-24c7-17-2-31-25-41z m6-66a109 109 0 0 1-9 23c-12 18-26 35-38 53-16 25-31 51-47 76a223 223 0 0 1-37 40c-9 8-18 5-25-6-5-8-9-17-14-25a38 38 0 0 0-26-22c-13-3-26-5-38-9-18-5-21-10-18-29a37 37 0 0 1 0-3 88 88 0 0 0-3-52 139 139 0 0 1-5-23c-2-13 1-17 13-21a162 162 0 0 1 19-4 468 468 0 0 0 119-31 281 281 0 0 1 58-18c24-4 51 20 52 51z m311 179c-14 2-18-1-18-12a473 473 0 0 0 2-53c-3-33-8-65-13-98a73 73 0 0 1 6-52 31 31 0 0 1 42-14 140 140 0 0 1 36 22 283 283 0 0 0 62 45c16 8 30 16 45 23a213 213 0 0 1 21 13c8 6 9 12 0 19a114 114 0 0 1-21 14 52 52 0 0 0-29 36 223 223 0 0 0-3 26 26 26 0 0 1-10 20c-1 0-1 0-2-1a31 31 0 0 1-2-4 77 77 0 0 0-59-42c-34-7-55 9-57 44 0 4 0 9 0 14z m-219 479a44 44 0 0 1-12-4c-15-11-31-23-47-35-11-9-12-14-2-24a422 422 0 0 1 34-27 31 31 0 0 1 39-2c20 12 41 23 62 36a97 97 0 0 1 16 14l-16 15-1 0-64 24a73 73 0 0 1-9 3z m43 10a183 183 0 0 0 2 30 22 22 0 0 0 22 18 24 24 0 0 0 21-18 38 38 0 0 0-11-44c8-7 13-5 20 1a67 67 0 0 1 3 85 41 41 0 0 1-68-3 63 63 0 0 1-11-46c2-18 5-21 22-24z m-80-1c7 4 12 7 17 10a7 7 0 0 1 3 4 64 64 0 0 1-13 54 28 28 0 0 1-46 0 71 71 0 0 1 0-83c11-13 13-13 24 1-18 3-26 30-21 45 2 8 6 16 14 15a28 28 0 0 0 17-10 42 42 0 0 0 5-37z"
                      ("macos" "osx") "M287 459a23 23 0 0 1 23 24v67a23 23 0 0 1-47 0v-67a23 23 0 0 1 24-24z m-49-230a23 23 0 1 1-35-31c65-75 176-120 297-120a487 487 0 0 1 68 5c-2 15-3 31-4 47a413 413 0 0 0-64-5c-108 1-206 39-262 104z m681 578h-838a67 67 0 0 1-66-66v-782a67 67 0 0 1 66-67h838a67 67 0 0 1 67 67v782a67 67 0 0 1-67 66z m-838-890a43 43 0 0 0-43 42v782a43 43 0 0 0 43 43h435a1383 1383 0 0 1-90-495 8 8 0 0 1 9-8l115 0a11 11 0 0 0 12-12c0-13 0-27 0-41q0-50 3-98c81 12 152 47 196 99a23 23 0 1 0 36-31c-52-61-135-102-228-116a1395 1395 0 0 1 25-167z m651 633v-67a23 23 0 0 0-47 0v67a23 23 0 0 0 47 0z"
                      "windows" "M17 695l394 57v-379h-394z m0-690l394-57v379h-394z m441 754v-386h525v461z m0-818l525-75v461h-525z")}})

(defn- table-view [^Element node ctx]
  (let [head-views (into []
                         (keep-indexed
                           (fn [col th]
                             (when-let [view (children-section-view th (style ctx "strong"))]
                               (assoc view :grid-pane/column col
                                           :grid-pane/row 0))))
                         (.select node "table>thead>tr>th"))
        content-row-offset (if (pos? (count head-views)) 1 0)
        rows-views (into []
                         (comp
                           (keep (fn [^Element tr]
                                   (let [row-views (into []
                                                         (keep-indexed
                                                           (fn [col td]
                                                             (some-> (children-section-view td ctx)
                                                                     (assoc :grid-pane/column col))))
                                                         (.select tr "tr>td"))]
                                     (when (pos? (count row-views))
                                       row-views))))
                           (map-indexed
                             (fn [row views]
                               (mapv #(assoc % :grid-pane/row (+ content-row-offset row)) views))))
                         (.select node "table>tbody>tr"))]
    (when (pos? (count rows-views))
      (let [views (into head-views cat rows-views)
            cols (transduce (map count) max (count head-views) rows-views)]
        {:fx/type fx.grid-pane/lifecycle
         :column-constraints (vec (repeat cols
                                          {:fx/type fx.column-constraints/lifecycle
                                           :min-width 75}))
         :hgap 4
         :children views}))))

(declare layout-section-node)

(defn- layout-children [acc node ctx]
  (reduce #(layout-section-node %1 %2 ctx) acc (child-nodes node)))

(defn- layout-section-node [acc node ctx]
  (let [tag (tag-name node)]
    (case tag
      "#text" (add-text acc (.getWholeText ^TextNode node) ctx)
      ("body" "html") (layout-children acc node ctx)
      "br" (add-separator acc 0)
      "p" (with-separators acc 3 0 layout-children node ctx)
      "div" (with-separators acc 0 0 layout-children node ctx)
      ("strong" "em" "small" "code" "sub" "sup") (layout-children acc node (style ctx tag))
      "super" (layout-children acc node (style ctx "sup"))
      ("dl" "dt") (with-separators acc 0 0 layout-children node ctx)
      "dd" (with-separators acc 0 0 add-view (indent-view
                                               {:fx/type fx.region/lifecycle :min-width 25 :max-height 0}
                                               (children-section-view node ctx)))
      "table" (with-separators acc 2 1 add-view (table-view node ctx))
      "h1" (with-separators acc 6 4 layout-children node (style ctx tag))
      "h2" (with-separators acc 5 3 layout-children node (style ctx tag))
      "h3" (with-separators acc 4 2 layout-children node (style ctx tag))
      "h4" (with-separators acc 3 1 layout-children node (style ctx tag))
      "h5" (with-separators acc 2 1 layout-children node (style ctx tag))
      "h6" (with-separators acc 1 1 layout-children node (style ctx tag))
      "a" (layout-children
            acc
            node
            (let [href (attr node "href")]
              (-> ctx
                  (style tag)
                  (cond-> (pos? (count href))
                          (assoc :on-mouse-clicked (fn/partial #'open-link! (:base-url ctx) (:project ctx) href))))))
      "span" (let [class (attr node "class")]
               (case class
                 ("icon-alert" "icon-attention" "icon-android" "icon-html5"
                   "icon-ios" "icon-linux" "icon-macos" "icon-osx" "icon-windows")
                 (-> acc
                     (add-view (icon-view (subs class (count "icon-"))))
                     (layout-children node ctx))

                 "type"
                 (layout-children acc node (-> ctx (style "code") (style "small")))

                 "mark"
                 (layout-children acc node (style ctx "strong"))

                 ("se" "s1" "s2" "n" "m" "mi" "mf" "mh" "kd" "kr" "kc" "p" "o" "ow" "na" "nb" "nf" "nt" "c1" "k" "s")
                 (layout-children acc node (style ctx class))

                 (layout-children acc node ctx)))
      "pre" (with-separators acc 1 1 add-view (code-block-view node ctx))
      "ul" (with-separators acc 0 0 add-view (unordered-list-view node ctx))
      "hr" (with-separators acc 3 3 add-view {:fx/type fx.region/lifecycle :style-class "md-hr"})
      "head" acc
      (-> acc
          (add-text (str "<" tag ">") (style ctx "error"))
          (layout-children node ctx)
          (add-text (str "</" tag ">") (style ctx "error"))))))

(defn- children-section-view [node ctx]
  (let [{:keys [content]} (layout-children {:content [] :whitespace-prefix nil} node ctx)]
    (when (pos? (count content))
      {:fx/type fx.v-box/lifecycle
       :children (into []
                       (keep (fn [text-flow-children-or-separator]
                               (cond
                                 (number? text-flow-children-or-separator)
                                 (when (pos? text-flow-children-or-separator)
                                   {:fx/type fx.region/lifecycle
                                    :style-class (str "md-separator-" text-flow-children-or-separator)})

                                 (and (= 1 (count text-flow-children-or-separator))
                                      (not= fx.text/lifecycle (:fx/type (text-flow-children-or-separator 0))))
                                 (text-flow-children-or-separator 0)

                                 :else {:fx/type fx.text-flow/lifecycle
                                        :style-class "md-paragraph"
                                        :children text-flow-children-or-separator})))
                       content)})))

(def ^:private ext-with-pref-height-defining-max-width
  (fx/make-ext-with-props
    {:max-width (fx.prop/make
                  (fx.mutator/setter
                    (fn [^ScrollPane pane [_key max-width]]
                      (.setMaxWidth pane max-width)
                      (.applyCss pane)
                      (.setPrefViewportHeight pane (.prefHeight (.getContent pane) max-width))
                      ;; this is needed in some rare cases T_T
                      ;; e.g. type "vmath", then type "."
                      (ui/run-later
                        (.setPrefViewportHeight pane (.prefHeight (.getContent pane) max-width)))))
                  fx.lifecycle/scalar)}))

(defn view
  "Cljfx component that defines a markdown viewer

  Supported props:
    :content     required, markdown string
    :project     required, project node id (for opening urls with defold scheme)
    :base-url    optional, URI used for resolving relative urls in links
    ...the rest of ScrollPane lifecycle props"
  [{:keys [content base-url project max-width pref-viewport-height] :as props}]
  (let [doc (-> (Parser/builder)
                ;; We need to use Custom HTML block parser to support Defold's
                ;; code highlight which uses <div><pre>...</pre></div>. Built-in
                ;; Markdown parser uses different rules when parsing <pre> and
                ;; <div> blocks when they have newlines inside:
                ;; - <pre> parser parses until the matching closing tag,
                ;;   preserving newlines;
                ;; - <div> parser parses until the end of line, then it treats
                ;;   remaining content until the closing tag as a markdown,
                ;;   which is a subject to whitespace normalizing.
                ;; Since Defold docs start code blocks with <div> and not <pre>,
                ;; Markdown parser will remove necessary whitespace by default,
                ;; garbling the multiline code blocks.
                ;; Our custom HTML block parser fixes the issue by treating
                ;; <div> tags the same way it treats <pre>.
                (.customBlockParserFactory (CustomHtmlBlockParser$Factory.))
                (.extensions [(AutolinkExtension/create)])
                (.build)
                (.parse content))
        html-string (.render (.build (HtmlRenderer/builder)) doc)
        html-ast (Jsoup/parseBodyFragment html-string)
        view (or (some-> (children-section-view
                           html-ast
                           {:paragraph true :base-url base-url :project project})
                         (assoc :style-class "md-root"))
                 {:fx/type fx.region/lifecycle})
        scroll-pane-view (-> props
                             (dissoc :content :base-url :project)
                             (fxui/provide-defaults :fit-to-width true
                                                    :hbar-policy :never)
                             (assoc :fx/type fx.scroll-pane/lifecycle
                                    :style-class "md-scroll-pane"
                                    :content view))]
    (if (and max-width (not pref-viewport-height))
      {:fx/type ext-with-pref-height-defining-max-width
       :props {:max-width [html-string max-width]}
       :desc scroll-pane-view}
      scroll-pane-view)))

(defn markdown->html
  [markdown]
  (let [parser (.build (Parser/builder))
        doc (.parse parser markdown)
        renderer (.build (.softbreak (HtmlRenderer/builder) " "))]
    (.render renderer doc)))

(g/defnode MarkdownNode
  (inherits resource-node/ResourceNode)

  (output markdown g/Str :cached (g/fnk [_node-id resource]
                                   (resource-io/with-error-translation resource _node-id :markdown
                                     (slurp resource :encoding "UTF-8"))))

  (output html g/Str :cached (g/fnk [markdown]
                               (str "<!DOCTYPE html>"
                                    "<html><head></head><body>"
                                    (markdown->html markdown)
                                    "</body></html>"))))

(defn register-resource-types [workspace]
  (workspace/register-resource-type workspace
    :ext "md"
    :label "Markdown"
    :textual? true
    :node-type MarkdownNode
    :view-types [:html :text]
    :view-opts nil))
