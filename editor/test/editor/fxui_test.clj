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

(ns editor.fxui-test
  (:require [cljfx.api :as fx]
            [cljfx.lifecycle :as fx.lifecycle]
            [cljfx.mutator :as fx.mutator]
            [clojure.test :refer :all]
            [editor.fxui :as fxui]
            [editor.ui :as ui]
            [util.coll :as coll])
  (:import [javafx.beans.property SimpleStringProperty]))

;; Inputs reconcile by key and remain mounted independently of the dependent
;; description, which receives nil and false instances without filtering.
(deftest with-instances-test
  (let [events (atom [])

        input
        (fn input [value]
          {:fx/type fx/ext-effect
           :fn (fn mount-input []
                 (swap! events conj [:created value])
                 (fn dispose-input [] (swap! events conj [:deleted value])))
           :desc {:fx/type ui/ext-value :value value}})

        child
        (fn child [{:keys [instances]}]
          {:fx/type ui/ext-value :value instances})

        desc
        {:fx/type fxui/ext-with-instances
         :inputs {:a (input :a) :b nil :c {:fx/type ui/ext-value :value false}}
         :desc {:fx/type child}}

        component (volatile! (fx/create-component desc))]
    (try
      (testing "The complete map includes nil and false instances"
        (is (= {:a :a :b nil :c false} (fx/instance @component))))
      (testing "Equal descriptions preserve input resources"
        (vswap! component fx/advance-component desc)
        (is (= [[:created :a]] @events)))
      (testing "An absent dependent description preserves input resources"
        (vswap! component fx/advance-component (assoc desc :desc nil))
        (is (nil? (fx/instance @component)))
        (is (= [[:created :a]] @events))
        (vswap! component fx/advance-component desc)
        (is (= {:a :a :b nil :c false} (fx/instance @component))))
      (testing "Input replacement, removal and addition precede the child"
        (vswap! component fx/advance-component
                (assoc desc :inputs {:a (input :replacement) :d (input :d)}))
        (is (= {:a :replacement :d :d} (fx/instance @component)))
        (is (= 1 (get (frequencies @events) [:deleted :a]))))
      (testing "The injected prop name is configurable"
        (vswap! component fx/advance-component
                (assoc desc
                  :key :values
                  :desc {:fx/type (fn describe-values [{:keys [values]}]
                                    {:fx/type ui/ext-value :value values})}))
        (is (= {:a :a :b nil :c false} (fx/instance @component))))
      (finally
        (fx/delete-component @component)))
    (is (= (frequencies (coll/into-> @events [] (keep (fn [[event value]] (when (= :created event) value)))))
           (frequencies (coll/into-> @events [] (keep (fn [[event value]] (when (= :deleted event) value))))))))
  (testing "The dependent child is deleted before its inputs"
    (let [events (atom [])

          effect
          (fn effect [id desc]
            {:fx/type fx/ext-effect
             :fn (fn mount []
                   (fn dispose [] (swap! events conj id)))
             :desc desc})

          component
          (fx/create-component
            {:fx/type fxui/ext-with-instances
             :inputs {:a (effect :input {:fx/type ui/ext-value :value :a})}
             :desc (effect :child nil)})]
      (fx/delete-component component)
      (is (= [:child :input] @events)))))

(defn- recording-prop [calls k]
  (fx/make-prop
    (fx.mutator/setter
      (fn assign-value! [owner value]
        (swap! calls conj [k value])
        (swap! owner assoc k value)))
    fx.lifecycle/scalar))

;; Equal prop values are reapplied only when the key changes, without replacing
;; the wrapped instance. All props in the map must be reapplied.
(deftest reapply-props-on-key-change-test
  (let [calls (atom [])
        first-prop (recording-prop calls :first)
        second-prop (recording-prop calls :second)
        desc {:fx/type fxui/ext-reapply-props-on-key-changed
              :key :initial
              :props {first-prop 1 second-prop 2}
              :desc {:fx/type fx/ext-instance-factory
                     :create (fn create-owner [] (atom {}))}}
        component (volatile! (fx/create-component desc))
        owner (fx/instance @component)]
    (try
      (is (= {:first 1 :second 2} @owner))
      (reset! calls [])
      (vswap! component fx/advance-component desc)
      (is (= [] @calls))
      (vswap! component fx/advance-component (assoc desc :key :changed))
      (is (identical? owner (fx/instance @component)))
      (is (= {:first 1 :second 2} @owner))
      (is (= {[:first nil] 1 [:first 1] 1 [:second nil] 1 [:second 2] 1}
             (frequencies @calls)))
      (finally
        (fx/delete-component @component)))))

;; Ordinary updates, additions and removals retain prop coercion and defaults,
;; including when a prop is removed at the same time as the key changes.
(deftest reapply-props-update-and-removal-test
  (let [calls (atom [])
        prop (fx/make-prop
               (fx.mutator/setter
                 (fn assign-value! [owner value]
                   (swap! calls conj value)
                   (reset! owner value)))
               fx.lifecycle/scalar :coerce str :default :removed)
        desc {:fx/type fxui/ext-reapply-props-on-key-changed
              :key :initial
              :props {prop 1}
              :desc {:fx/type fx/ext-instance-factory
                     :create (fn create-owner [] (atom nil))}}
        component (volatile! (fx/create-component desc))
        owner (fx/instance @component)]
    (try
      (is (= "1" @owner))
      (reset! calls [])
      (vswap! component fx/advance-component (assoc desc :props {prop 2}))
      (is (= ["2"] @calls))
      (vswap! component fx/advance-component (assoc desc :props {}))
      (is (= ":removed" @owner))
      (vswap! component fx/advance-component (assoc desc :props {prop 3}))
      (is (= "3" @owner))
      (vswap! component fx/advance-component (assoc desc :key :changed :props {}))
      (is (= ":removed" @owner))
      (is (= ["2" ":removed" "3" ":removed"] @calls))
      (finally
        (fx/delete-component @component)))))

;; Reapplication preserves prop value resources. Removing a prop disposes its
;; value, while deleting the wrapper disposes remaining values and its child.
(deftest reapply-props-value-lifecycle-test
  (let [calls (atom [])
        resources (atom [])
        prop (fx/make-prop
               (fx.mutator/setter
                 (fn assign-value! [owner value]
                   (swap! calls conj value)
                   (reset! owner value)))
               fx.lifecycle/dynamic)
        value-desc {:fx/type fx/ext-effect
                    :fn (fn start-value! []
                          (swap! resources conj :value-created)
                          (fn stop-value! [] (swap! resources conj :value-deleted)))
                    :args []
                    :desc {:fx/type ui/ext-value :value :value}}
        desc {:fx/type fxui/ext-reapply-props-on-key-changed
              :key :initial
              :props {prop value-desc}
              :desc {:fx/type fx/ext-effect
                     :fn (fn start-owner! []
                           (swap! resources conj :owner-created)
                           (fn stop-owner! [] (swap! resources conj :owner-deleted)))
                     :args []
                     :desc {:fx/type fx/ext-instance-factory
                            :create (fn create-owner [] (atom nil))}}}
        component (volatile! (fx/create-component desc))]
    (try
      (vswap! component fx/advance-component (assoc desc :key :changed))
      (is (= [:owner-created :value-created] @resources))
      (vswap! component fx/advance-component (assoc desc :key :changed :props {}))
      (is (= [:owner-created :value-created :value-deleted] @resources))
      (vswap! component fx/advance-component (assoc desc :key :changed))
      (is (= [:owner-created :value-created :value-deleted :value-created] @resources))
      (finally
        (reset! calls [])
        (fx/delete-component @component)))
    (is (= [] @calls))
    (is (= [:owner-created :value-created :value-deleted :value-created
            :value-deleted :owner-deleted]
           @resources))))

;; Replacing the child applies props to the new instance even with an unchanged
;; key and disposes prop value resources belonging to the old instance.
(deftest reapply-props-child-replacement-test
  (let [resources (atom [])
        calls (atom [])
        prop (fx/make-prop
               (fx.mutator/setter
                 (fn assign-value! [owner value]
                   (swap! calls conj value)
                   (reset! owner value)))
               fx.lifecycle/dynamic)
        value-desc {:fx/type fx/ext-effect
                    :fn (fn start-value! []
                          (swap! resources conj :created)
                          (fn stop-value! [] (swap! resources conj :deleted)))
                    :args []
                    :desc {:fx/type ui/ext-value :value :value}}
        desc {:fx/type fxui/ext-reapply-props-on-key-changed
              :key :unchanged
              :props {prop value-desc}
              :desc {:fx/type fx/ext-recreate-on-key-changed
                     :key :first
                     :desc {:fx/type fx/ext-instance-factory
                            :create (fn create-owner [] (atom nil))}}}
        component (volatile! (fx/create-component desc))
        first-owner (fx/instance @component)]
    (try
      (reset! calls [])
      (vswap! component fx/advance-component (assoc-in desc [:desc :key] :second))
      (is (not (identical? first-owner (fx/instance @component))))
      (is (= :value @(fx/instance @component)))
      (is (= [:value] @calls))
      (is (= [:created :deleted :created] @resources))
      (finally
        (fx/delete-component @component)))
    (is (= [:created :deleted :created :deleted] @resources))))

;; A listener prop must be rebound without duplicates when the key changes,
;; and removing it must detach the listener from the live owner.
(deftest reapply-props-listener-test
  (let [calls (atom [])
        prop (fx/make-prop (fx.mutator/property-change-listener identity)
                           fx.lifecycle/change-listener)
        desc {:fx/type fxui/ext-reapply-props-on-key-changed
              :key :initial
              :props {prop (fn on-value-changed [value] (swap! calls conj value))}
              :desc {:fx/type fx/ext-instance-factory
                     :create (fn create-owner [] (SimpleStringProperty.))}}
        component (volatile! (fx/create-component desc))
        ^SimpleStringProperty owner (fx/instance @component)]
    (try
      (.set owner "first")
      (vswap! component fx/advance-component (assoc desc :key :changed))
      (.set owner "second")
      (is (= ["first" "second"] @calls))
      (vswap! component fx/advance-component (assoc desc :key :changed :props {}))
      (.set owner "third")
      (is (= ["first" "second"] @calls))
      (finally
        (fx/delete-component @component)))))
