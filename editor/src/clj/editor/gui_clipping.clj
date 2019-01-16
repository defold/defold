(ns editor.gui-clipping
  (:require [dynamo.graph :as g]
            [clojure.pprint :refer [cl-format]])
  (:import [com.jogamp.opengl GL GL2]
           [clojure.lang ExceptionInfo]))

;;
;; Clipping
;;
;; Clipping uses the stencil buffer. Similar to the depth buffer, the
;; stencil buffer is used to determine whether or not to draw by
;; comparing a value from the buffer with a value from what you're
;; drawing. For the depth buffer we're roughly comparing the z component
;; of what you're drawing with the value in the depth buffer. If the z
;; value is < (or > depending on axis direction?) we draw, and update the
;; depth buffer with the new z.
;;
;; For the stencil buffer the api is slightly different. The functions we
;; use are:
;;
;; * glStencilOp sfail dpfail dppass
;; * glStencilFunc func ref mask
;; * glStencilMask stencil-mask
;; * glEnable/glDisable with GL_STENCIL_TEST
;; * glClear with GL_STENCIL_BUFFER_BIT
;;
;; When the stencil buffer is enabled, the following test is performed
;; before the depth test:
;;
;;     (ref & mask) func (<value from stencil buffer> & mask)
;;
;; If this test fails, we update the stencil buffer according to sfail.
;; If it succeeds and the depth test fails, we update according to dpfail.
;; If both tests pass, we update according to dppass and draw.
;;
;; The stencil buffer can be updated by incrementing or decrementing
;; the value, setting it to zero, inverting the bits, keeping the
;; original value or replacing it with ref. Only the bits masked by
;; stencil-mask are updated.
;;
;; The parameters we use are:
;;
;; * glStencilOp GL_KEEP GL_REPLACE GL_REPLACE
;;   The stencil buffer is updated whenever the stencil test
;;   succeeds. If the test fails, we keep the old value.
;;
;; * glStencilFunc GL_EQUAL ref mask
;;   With ref and mask set depending on where we are in the gui scene.
;;
;; * glStencilMask 0xff or 0
;;   Depending on if we're drawing a clipper or a normal node.
;;
;; So for us, the test is:
;;
;;     (ref & mask) == (<value from stencil buffer> & mask)
;;
;; And since we replace the value on success, the value written will
;; effectively be:
;; 
;;     (<old-stencil-value> & ~stencil-mask) | (ref & stencil-mask)
;;
;; But since we always set stencil-mask to 0xff or 0, we either leave
;; the bits as-is or replace it entirely.
;;
;; Note that ref is used both in the test and for setting the new
;; value.
;;
;; In Defold we assume the stencil buffer depth is 8 bits, so ref and
;; mask are 8 bits. We support inverted- and non-inverted clippers,
;; and clippers can be nested. Think of the clippers in a scene as
;; nested scopes. All nodes within a scope are drawn with a ref and
;; mask that identifies the scope in order to clip it properly. One
;; pseudo-scope covers the whole scene and any node drawn in that
;; scope (not under an actual clipper) is drawn without clipping
;; enabled. To identify nested non-inverted clipper scopes we split
;; the 8 bits of the ref value into groups. The value of each group is
;; roughly the steps in a path you take along the clipper hierarchy to
;; reach a particular non-inverted clipper scope. The nodes drawn
;; within that scope use as ref all the bits from the groups. The mask
;; is simply enough 1's to cover the group bits.
;;
;; In the example below, the clipper will be drawn using ref and mask set
;; accordingly, and any (non-clipper) child nodes with child ref and
;; child mask.
;;              clipper type     ref      mask     child ref  child mask
;; - scene
;;   - A        non-inverted     00000001 00000000 00000001   00000011
;;     - B      non-inverted     00000101 00000011 00000101   00001111
;;     - C      non-inverted     00001001 00000011 00001001   00001111
;;       - D    non-inverted     00011001 00001111 00011001   00011111
;;         - E  non-inverted     00111001 00011111 00111001   01111111
;;         - F  non-inverted     01011001 00011111 01011001   01111111
;;     - G      non-inverted     00001101 00000011 00001101   00001111
;;   - H        non-inverted     00000010 00000000 00000010   00000011
;;
;; In the scene scope, we have the non-inverted clippers A and H. Since
;; these are top level clippers we always want to draw them, so the mask
;; is 0. In order to draw their child nodes properly we need to be able
;; to distinguish the two scopes, so they each get assigned a unique ref
;; value within this scope: 01 and 10. We cannot use the value 0 when
;; identifying a scope, because that value is also used used "outside"
;; the clipper. So: 00 is off limits, 01 and 10 are taken. We have three
;; values and must use two bits for our first bit group. The child mask
;; is unsurprisingly 00000011.
;;
;; Looking below A we have the non-inverted clippers B, C and G. The refs
;; of each have 01 in the two lowest bits, and we use the mask 11 when
;; drawing to confine them to A. By the same reasoning as earlier we need
;; two more bits to distinguish their scopes.
;;
;; Looking at C, there is only one non-inverted clipper D in scope so
;; this time we only need one bit. Finally below D we need two bits.
;;
;; Effectively the ref bits are used as follows:
;;
;; Bits:     7          6 5      4   3 2         1 0
;; Meaning: [not used] [E or F] [D] [B, C or G] [A or H]
;;
;; If in this example there had been more non-inverted clippers under H,
;; the bit patterns in bit 7->2 could have been reused. There would be no
;; risk of confusing the scopes since bits 0 and 1 (A or H) would differ.
;;
;; Notice that the mask gets filled as we go deeper in the clipper
;; hierarchy.
;;
;; Inverted clippers doesn't fit the bit group scheme. We want to draw
;; the clipper using a ref that identifies the clipper. But when drawing
;; the nodes in its scope, we want to *not* draw where the inverted
;; clipper is drawn. We must also still respect the scopes of clippers
;; higher up in the hierarchy.
;;
;; It's like we want the stencil test for nodes in scope to check:
;; "We are drawing in the parent of the inverted clipper" AND
;; "We are not drawing in the inverted clipper"
;;
;; (inverted-clipper-parent-ref & inverted-clipper-parent-child-mask) ==
;; (<value from stencil-buffer> & inverted-clipper-parent-child-mask)
;; AND
;; inverted-clipper-bit-value != (<value from stencil buffer> & inverted-clipper-bit-mask)
;;
;; We can't do this literally with the stencil functions, but we can
;; express it differently:
;;
;; (inverted-clipper-parent-ref & inverted-clipper-parent-child-mask) ==
;; (<value from stencil-buffer> & inverted-clipper-parent-child-mask)
;; AND
;; 0 == (<value from stencil buffer> & inverted-clipper-bit-mask)
;;
;; Then we do the two comparisons simultaneously:
;;
;; ((0 | inverted-clipper-parent-ref) & (inverted-clipper-bit-mask | inverted-clipper-parent-child-mask)) ==
;; (<value from stencil-buffer> & (inverted-clipper-bit-mask | inverted-clipper-parent-child-mask))
;;
;; We allocate the non-inverted group bits from lowest to highest. Then
;; for each inverted clipper within a non-inverted clipper scope (or the
;; top level pseudo scope), we allocate and set 1 bit from highest ->
;; lowest. The nodes in scope of an inverted clipper then includes that
;; clippers bit in its mask, but uses a ref value with the bit set to 0 -
;; effectively the child ref value is unchanged.
;;
;; An example:
;;
;;              clipper type     ref      mask     child ref  child mask
;; - scene
;;   - A        non-inverted     00000001 00000000 00000001   00000011
;;     - B      non-inverted     00000101 00000011 00000101   00000111
;;   - C        non-inverted     00000010 00000000 00000010   00000011
;;     - D      inverted         10000010 00000011 00000010   10000011
;;       - E    inverted         01000010 10000011 00000010   11000011
;;     - F      inverted         00100010 00000011 00000010   00100011
;;       - G    non-inverted     00000110 00100011 00000110   00100111
;;         - H  inverted         00010110 00100111 00000110   00110111
;;   - I        inverted         10000000 00000000 00000000   10000000
;;     - J      non-inverted     00000011 10000000 00000011   10000011
;;
;; In the top level scene scope we have the obvious non-inverted clippers
;; A, C but now we also count J. The reason we include J in the scene
;; scope even though I lies between is that I is an inverted node. We
;; cannot use the bit we assign to I for distinguishing between A, C and
;; J. So, we have the off limits 00 + 01, 10, 11 for A, C and J requiring
;; 2 bits for the top level group.
;;
;; We assign bit 7 for I. We draw J with 0 in I's bit but with a mask
;; covering it. This makes sure we don't draw J on I. Notice that the
;; child mask of J must include I's bit.
;;
;; In the scope of C we have one non-inverted clipper G, requiring 1 bit,
;; and three inverted clippers D, E and F. Since we cannot use the bit
;; assigned to clippers to distinguish scopes, they get assigned one bit
;; each. We can not for instance use bit 6 for both E and F: when drawing
;; G we would be unable to draw where E was drawn.
;;
;; The implementation is mostly concerned with counting different types
;; of clippers in the scopes, assigning ref and mask values, and
;; transfering these to the scene nodes so the clipping is applied when
;; drawing.
;;
;; Visible Clippers
;;
;; Clippers can be marked as visible, meaning we draw them with the
;; color or texture assigned.
;;
;; We implement this by injecting a new scene node drawing the visible
;; part of the clipper. For inverted clippers, this representation must
;; still be drawn inside the clipper as opposed to the other children
;; being drawn only outside.
;;
;; Layers and rendering order
;;
;; For clipping to work properly, the clipper must be drawn before its
;; children. Normally we can change the draw order of nodes (starting out
;; as depth first, in order traversal) by assigning layers to nodes. But
;; doing that for clipper child nodes to draw it before the clipper would
;; be pointless. Instead the meaning of layers within clippers is changed
;; to only affect the relative draw order of the nodes in its scope. The
;; layer set on the clipper node itself does not affect its draw order -
;; but it *is* inherited by any child node with no layer set (like
;; outside clippers) and especially by the node representing a visible
;; clipper. The clipper is effectively drawn in the "null" layer.
;;
;; Bit overflow
;;
;; The 8 bits available creates some restrictions on how deep we can
;; nest clippers and how many clipper siblings we can have within the
;; scopes. If too many bits are needed the engine will start printing
;; warnings and clipping will simply break. In the editor we try to
;; recover from this by clearing the stencil buffer between rendering
;; each "top level" clipper in the scene. This helps if we have too
;; many siblings on the top level, but we can't recover from an
;; individual top level clipper tree requiring too many bits. It's
;; debatable whether we should even try to recover since the engine
;; can't.
;;

(set! *warn-on-reflection* true)

(def ^:private stencil-bits 8)

(defn- visible-clipper? [scene]
  (get-in scene [:renderable :user-data :clipping :visible]))

(defn- clipper-scene? [scene]
  (contains? (-> scene :renderable :user-data) :clipping))

(defn- clipper-trees
  ([scene]
   (clipper-trees scene []))
  ([scene path]
   (let [child-clippers (into []
                              (comp (map-indexed (fn [i e] (clipper-trees e (conj path i))))
                                    cat)
                              (:children scene))]
     (if-let [clipping (get-in scene [:renderable :user-data :clipping])]
       [{:node-id (:node-id scene) ; for error values
         ;; add for debugging
         ;; :id (g/node-value (:node-id scene) :id)
         :clipping clipping
         :path path
         :children child-clippers}]
       child-clippers))))

(defn- wrap-trees [clipper-trees]
  {:children clipper-trees})

(defn- inverted? [clipper]
  (= true (get-in clipper [:clipping :inverted])))

(defn- non-inverted? [clipper]
  (= false (get-in clipper [:clipping :inverted])))

(defn- wrapper? [clipper]
  (and (not (inverted? clipper))
       (not (non-inverted? clipper))))

(defn- count-inverted-clipper-scope [clipper-tree]
  (assert (inverted? clipper-tree))
  (let [successor-non-inverted (apply + (keep :successor-non-inverted (:children clipper-tree)))
        inverted-since-non-inverted-or-bottom (apply + (keep :inverted-since-non-inverted-or-bottom (:children clipper-tree)))
        max-successor-non-inverted-allocated-bits (apply max 0 (map :max-successor-non-inverted-allocated-bits (:children clipper-tree)))]
    (assoc clipper-tree
           ;; Number of paths down from this clipper that lead to a
           ;; non-inverted clipper (via only inverted clippers!).
           ;; We use this to determine how many bits to allocate for
           ;; distinguishinging successor non-inverted clippers in the
           ;; ancestor non-inverted clipper.
           :successor-non-inverted successor-non-inverted
           ;; Number of inverted clippers along the paths down to a
           ;; non-inverted clipper (or bottom) + this inverted clipper.
           ;; We use this to determine how many individual bits to
           ;; allocate for inverted clippers between the ancestor
           ;; non-inverted clipper and successor non-inverted
           ;; clippers (or bottom).
           :inverted-since-non-inverted-or-bottom (inc inverted-since-non-inverted-or-bottom)
           ;; On every non-inverted clipper we accumulate a required
           ;; bit count from the required bit counts of successor
           ;; non-inverted scopes, the number of successor
           ;; non-inverted scopes, and the number of inverted
           ;; scopes passed between.
           :max-successor-non-inverted-allocated-bits max-successor-non-inverted-allocated-bits)))

(defn- log2 [n] (/ (Math/log n) (Math/log 2)))
(defn- pow2 [n] (bit-shift-left 1 n))
(defn- n->bits [n] (int (+ 1 (log2 n))))
(defn- bits->mask [bits] (dec (pow2 bits)))

(defn- count-non-inverted-clipper-scope [clipper-tree]
  (assert (non-inverted? clipper-tree))
  (let [successor-non-inverted (apply + (keep :successor-non-inverted (:children clipper-tree)))
        inverted-since-non-inverted-or-bottom (apply + (keep :inverted-since-non-inverted-or-bottom (:children clipper-tree)))
        non-inverted-ref-bits (if (= successor-non-inverted 0) 0 (n->bits successor-non-inverted))
        max-successor-non-inverted-allocated-bits (apply max 0 (map :max-successor-non-inverted-allocated-bits (:children clipper-tree)))]
    (assoc clipper-tree
           ;; Contributes 1 successor to the ancestor non-inverted
           ;; scope.
           :successor-non-inverted 1
           ;; Accumulated bits needed to distinguish successor
           ;; non-inverted clipper scopes. Used during assignment of
           ;; ref-val and mask.
           :non-inverted-ref-bits non-inverted-ref-bits
           ;; see count-inverted-clipper-scope
           :max-successor-non-inverted-allocated-bits (+ non-inverted-ref-bits
                                                         inverted-since-non-inverted-or-bottom
                                                         max-successor-non-inverted-allocated-bits))))

(defn- count-wrapper-scope [clipper-tree]
  (assert (wrapper? clipper-tree))
  (let [successor-non-inverted (apply + (keep :successor-non-inverted (:children clipper-tree)))
        inverted-since-non-inverted-or-bottom (apply + (keep :inverted-since-non-inverted-or-bottom (:children clipper-tree)))
        non-inverted-ref-bits (if (= successor-non-inverted 0) 0 (n->bits successor-non-inverted))
        max-successor-non-inverted-allocated-bits (apply max 0 (map :max-successor-non-inverted-allocated-bits (:children clipper-tree)))]
    (assoc clipper-tree
           :non-inverted-ref-bits non-inverted-ref-bits
           :allocated-bits (+ non-inverted-ref-bits inverted-since-non-inverted-or-bottom max-successor-non-inverted-allocated-bits))))

(defn- count-clipper-scopes [clipper-tree]
  (let [clipper-tree' (update clipper-tree :children #(map count-clipper-scopes %))]
    (cond
      (inverted? clipper-tree')
      (count-inverted-clipper-scope clipper-tree')

      (non-inverted? clipper-tree')
      (count-non-inverted-clipper-scope clipper-tree')

      true
      (count-wrapper-scope clipper-tree'))))

(declare assign-clipper-bits)

(defn- assign-inverted-clipper-bits [clipper-tree context]
  (let [inverted-clipper-bit-mask (bit-shift-left 1 (- (dec stencil-bits) (:inverted-clipper-bits-used context)))
        ref-val (bit-or (:ref-val context) inverted-clipper-bit-mask)
        mask (:mask context)
        child-ref-val (:ref-val context) ; bit for this clipper is already 0 in context ref
        child-mask (bit-or mask inverted-clipper-bit-mask)
        child-context (-> context
                          (update :inverted-clipper-bits-used inc)
                          (assoc :ref-val child-ref-val
                                 :mask child-mask))
        [children' child-context'] (reduce (fn [[new-children context] child]
                                             (let [[child' context'] (assign-clipper-bits child context)]
                                               [(conj new-children child') context']))
                                           [[] child-context]
                                           (:children clipper-tree))
        clipper-tree' (assoc clipper-tree
                             :ref-val ref-val
                             :mask mask
                             :child-ref-val child-ref-val
                             :child-mask child-mask
                             :children children')
        context' (assoc context
                        :non-inverted-ref-val-counter (:non-inverted-ref-val-counter child-context')
                        :inverted-clipper-bits-used (:inverted-clipper-bits-used child-context'))]
    [clipper-tree' context']))

(defn- assign-non-inverted-clipper-bits [clipper-tree context]
  (let [ref-val (bit-or (:ref-val context) (bit-shift-left (:non-inverted-ref-val-counter context) (:non-inverted-ref-bits-used context)))
        mask (:mask context)
        child-ref-val ref-val
        child-mask (bit-or mask (bit-shift-left (bits->mask (:non-inverted-ref-bits context)) (:non-inverted-ref-bits-used context)))
        child-context {:ref-val child-ref-val
                       :mask child-mask
                       :inverted-clipper-bits-used (:inverted-clipper-bits-used context)
                       :non-inverted-ref-val-counter 1
                       :non-inverted-ref-bits (:non-inverted-ref-bits clipper-tree)
                       :non-inverted-ref-bits-used (+ (:non-inverted-ref-bits-used context) (:non-inverted-ref-bits context))}
        [children' child-context'] (reduce (fn [[new-children context] child]
                                             (let [[child' context'] (assign-clipper-bits child context)]
                                               [(conj new-children child') context']))
                                           [[] child-context]
                                           (:children clipper-tree))
        clipper-tree' (assoc clipper-tree
                             :ref-val ref-val
                             :mask mask
                             :child-ref-val child-ref-val
                             :child-mask child-mask
                             :children children')
        context' (update context :non-inverted-ref-val-counter inc)]
    [clipper-tree' context']))

(defn- assign-wrapper-clipper-bits [clipper-tree]
  (assert (wrapper? clipper-tree))
  (let [allocated (:allocated-bits clipper-tree)]
    (if (<= allocated stencil-bits)
      (let [child-context {:ref-val 0
                           :mask 0
                           :inverted-clipper-bits-used 0
                           :non-inverted-ref-val-counter 1
                           :non-inverted-ref-bits (:non-inverted-ref-bits clipper-tree)
                           :non-inverted-ref-bits-used 0}
            [children' child-context'] (reduce (fn [[new-children context] child]
                                                 (let [[child' context'] (assign-clipper-bits child context)]
                                                   [(conj new-children child') context']))
                                               [[] child-context]
                                               (:children clipper-tree))]
        (assoc clipper-tree :children children'))
      (let [wrapped-child-clipper-trees (map (comp count-wrapper-scope wrap-trees vector) (:children clipper-tree))]
        ;; If the clipper-tree (wrapper) requires more bits than
        ;; available in the stencil buffer, we try to recover
        ;; by inserting a clear betweeen each top level clipper.
        ;; This is only possible if each of the top level clippers
        ;; would fit on its own. Otherwise, we bail with an error.
        (if-let [overflowing-child-clipper-tree (first (filter #(> (:allocated-bits %) stencil-bits) wrapped-child-clipper-trees))]
          (g/error-fatal "bit overflow" {:type :bit-overflow :source-id (-> overflowing-child-clipper-tree :children first :node-id)})
          ;; Try recovering from the overflow by adding clear
          ;; flags. Note that the engine does not do this, so you
          ;; might be misled into building a gui in the editor that
          ;; will not work during runtime. Apparently the engine only
          ;; inserts clears between rendering different gui scenes.
          (let [children' (into []
                                (comp
                                  (mapcat (fn [wrapped-child-clipper-tree]
                                            ;; Each wrapped-child-clipper-tree has one child, and assign-wrapper-clipper-bits
                                            ;; leaves the tree structure unchanged.
                                            (let [{:keys [children]} (assign-wrapper-clipper-bits wrapped-child-clipper-tree)]
                                              (assert (= 1 (count children)))
                                              children)))
                                  (map #(assoc % :clear true)))
                                wrapped-child-clipper-trees)]
            (assoc clipper-tree :children children')))))))

(defn- assign-clipper-bits
  ([clipper-tree]
   (assign-wrapper-clipper-bits clipper-tree))
  ([clipper-tree context]
   (cond
     (inverted? clipper-tree)
     (assign-inverted-clipper-bits clipper-tree context)

     (non-inverted? clipper-tree)
     (assign-non-inverted-clipper-bits clipper-tree context))))

(defn- make-visible-clipper-scene [scene]
  ;; The visible clipper scene for an inverted clipper needs a
  ;; different mask & ref than other nodes in the clippers scope to
  ;; make it draw *inside* rather than outside the clipper. For this
  ;; we tag it with :visible-clipper-scene?
  (-> scene
      (update-in [:renderable :user-data] dissoc :clipping) ; don't want to treat this node as a clipper
      (assoc-in [:renderable :user-data :visible-clipper-scene?] true) ; tag it
      (dissoc :children :transform :aabb)))

(defn- visible-clipper-scene? [scene]
  (get-in scene [:renderable :user-data :visible-clipper-scene?]))

(defn- inject-visible-clipper-scenes [scene]
  (cond-> (update scene :children #(map inject-visible-clipper-scenes %))
    (visible-clipper? scene)
    (update :children #(into [(make-visible-clipper-scene scene)] %))))

(defn- remove-clipper-renderable-tags
  "We remove the renderable tags for all clipper scenes to make sure
  the clipping state is drawn. Any visible clipper representation
  retains the tags and can be hidden."
  [scene]
  (cond-> (update scene :children #(map remove-clipper-renderable-tags %))
    (clipper-scene? scene)
    (update-in [:renderable] dissoc :tags)))

(defn- clipper-tree->trie [clipper-tree]
  (reduce (fn [trie clipper-tree]
            (assoc-in trie (:path clipper-tree) (select-keys clipper-tree [:ref-val :mask :child-ref-val :child-mask :clear])))
          (select-keys clipper-tree [:ref-val :mask :child-ref-val :child-mask :clear])
          (rest (tree-seq :children :children clipper-tree))))

(defn- descendant-clipping-state [last-clipper-trie visible-clipper-scene?]
  ;; If this is the injected scene for showing the clipper contents, it should be drawn using the same
  ;; ref and mask as the clipper itself.
  {:ref-val (if visible-clipper-scene? (:ref-val last-clipper-trie) (:child-ref-val last-clipper-trie))
   :mask (if visible-clipper-scene? (:mask last-clipper-trie) (:child-mask last-clipper-trie))
   :write-mask 0
   :color-mask [true true true true]})

(defn- clipper-clipping-state [clipper-trie]
  (cond-> {:ref-val (:ref-val clipper-trie)
           :mask (:mask clipper-trie)
           :write-mask 0xff
           :color-mask [false false false false]}
    (:clear clipper-trie)
    (assoc :clear true)))

(defn- clipper-trie? [trie]
  (contains? trie :ref-val))

(defn- lookup-clipping-state [trie last-clipper-trie step visible-clipper-scene?]
  (if-let [next-trie (get trie step)]
    (if (clipper-trie? next-trie)
      ;; trie represents clipper -> this is a clipper node
      [next-trie next-trie (clipper-clipping-state next-trie)]
      ;; trie does not represent a clipper -> this is a descendant node of some earlier clipper
      [next-trie last-clipper-trie (when last-clipper-trie (descendant-clipping-state last-clipper-trie visible-clipper-scene?))])
    ;; stepping outside the tree of clippers -> this is a descendant node of some earlier clipper
    [nil last-clipper-trie (when last-clipper-trie (descendant-clipping-state last-clipper-trie visible-clipper-scene?))]))

(defn- add-clipping-state [scene clipping-state]
  (-> scene
      (assoc-in [:renderable :user-data :clipping-state] clipping-state)
      (assoc-in [:renderable :batch-key :clipping-state] clipping-state)))

(defn- apply-clippers [clipper-tree scene]
  (let [trie (clipper-tree->trie clipper-tree)]
    (letfn [(apply-clipping [scene [trie last-clipper-trie clipping-state]]
              (let [new-children (map-indexed (fn [index child]
                                                (let [child-lookup-result (lookup-clipping-state trie last-clipper-trie index (visible-clipper-scene? child))]
                                                  (apply-clipping child child-lookup-result)))
                                              (:children scene))]
                (cond-> (assoc scene :children new-children)
                  (contains? scene :renderable)
                  (add-clipping-state clipping-state)

                  :because-gui-clipping-tests-need-it
                  (assoc-in [:renderable :user-data :clipping-child-state]
                            (nth (lookup-clipping-state trie last-clipper-trie :fake-child-step false) 2)))))]
      (assert (not (clipper-trie? trie)))
      (apply-clipping scene [trie nil nil]))))

#_(defn- bitstring
  ([n] (bitstring n stencil-bits))
  ([n total-bits] (cl-format nil (str "~" total-bits ",'0',B") n)))

#_(defn- print-clipper-tree
    ([clipper-tree] (print-clipper-tree clipper-tree ""))
    ([clipper-tree prefix]
     (let [{:keys [id node-id clear max-successor-non-inverted-allocated-bits path ref-val mask child-ref-val child-mask]} clipper-tree]
       (println prefix :id id
                (cond (inverted? clipper-tree) :inverted
                      (non-inverted? clipper-tree) :non-inverted
                      :else :not-a-clipper)
                :node-id node-id
                :clear clear
                :bits-allocated max-successor-non-inverted-allocated-bits
                :path path
                :ref-val (when ref-val (bitstring ref-val))
                :mask (when mask (bitstring mask))
                :child-ref-val (when child-ref-val (bitstring child-ref-val))
                :child-mask (when child-mask (bitstring child-mask))))
     (run! #(print-clipper-tree % (str prefix "  ")) (:children clipper-tree))))

(defn setup-states [scene]
  (let [scene-with-visible-clippers (-> scene
                                        inject-visible-clipper-scenes
                                        remove-clipper-renderable-tags)
        clipper-tree (-> scene-with-visible-clippers
                         clipper-trees
                         wrap-trees
                         count-clipper-scopes
                         assign-clipper-bits)]
    (cond
      (g/error? clipper-tree)
      clipper-tree

      true
      (apply-clippers clipper-tree scene-with-visible-clippers))))

(defn scene-key [scene]
  [(:node-id scene) (get-in scene [:renderable :user-data :clipping-state])])

(defn- trickle-down-default-layers
  "Perform inheritance of parent node layer to child nodes with no
  layer assigned, and strip layer from clipper nodes."
  ([scene]
   (trickle-down-default-layers nil scene))
  ([default-layer scene]
   (let [layer (get-in scene [:renderable :layer-index])]
     (if (and layer (>= layer 0))
       (cond-> (update scene :children #(mapv (partial trickle-down-default-layers layer) %))
         (clipper-scene? scene)
         (assoc-in [:renderable :layer-index] nil)) ; clipper nodes render in the null layer (first)
       (cond-> (update scene :children #(mapv (partial trickle-down-default-layers default-layer) %))
         (not (clipper-scene? scene))
         (assoc-in [:renderable :layer-index] default-layer))))))

(defn- scope-scenes
  "Collect scenes in depth first pre order (parent visited before children), stopping at clipper nodes."
  [scene]
  (loop [children-stack (list (:children scene))
         scenes []]
    (cond
      (not (seq children-stack))
      scenes

      (not (seq (first children-stack)))
      (recur (rest children-stack) scenes)

      true
      (let [child (ffirst children-stack)
            next-children (if (clipper-scene? child) [] (:children child))]
        (recur (cons next-children (cons (rest (first children-stack)) (rest children-stack)))
               (conj scenes child))))))

(defn- render-keys
  ([scene]
   (render-keys {} (scope-scenes scene)))
  ([assignment unsorted-scenes]
   (let [scenes (sort-by #(get-in % [:renderable :layer-index]) unsorted-scenes)]
     (reduce (fn [assignment scene]
               (if (clipper-scene? scene)
                 (-> assignment
                     (assoc (scene-key scene) (count assignment))
                     (render-keys (scope-scenes scene)))
                 (assoc assignment (scene-key scene) (count assignment))))
             assignment
             scenes))))

(defn scene->render-keys [scene]
  (-> scene
      trickle-down-default-layers
      render-keys))

(defn setup-gl [^GL2 gl state]
  (when state
    (.glEnable gl GL/GL_STENCIL_TEST)
    (.glStencilOp gl GL/GL_KEEP GL/GL_REPLACE GL/GL_REPLACE)
    (.glStencilFunc gl GL2/GL_EQUAL (:ref-val state) (:mask state))
    (.glStencilMask gl (:write-mask state))
    (when (:clear state)
      (.glClear gl GL/GL_STENCIL_BUFFER_BIT))
    (let [[c0 c1 c2 c3] (:color-mask state)]
      (.glColorMask gl c0 c1 c2 c3))))

(defn restore-gl [^GL2 gl state]
  (when state
    (.glDisable gl GL/GL_STENCIL_TEST)
    (.glColorMask gl true true true true)))
