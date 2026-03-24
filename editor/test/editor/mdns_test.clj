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

(ns editor.mdns-test
  (:require [clojure.test :refer :all])
  (:import [com.dynamo.discovery MDNS MDNS$Logger MDNS$TestHooks MDNSServiceInfo]
           [java.io ByteArrayOutputStream]
           [java.util HashMap UUID]
           [java.net DatagramPacket InetAddress MulticastSocket]))

(set! *warn-on-reflection* true)

(def ^:private dns-type-ptr 12)
(def ^:private dns-type-txt 16)
(def ^:private dns-type-a 1)
(def ^:private dns-type-srv 33)
(def ^:private dns-class-in 1)

(defn- utf8-bytes
  ^bytes [^String s]
  (.getBytes s "UTF-8"))

(defn- write-u16! [^ByteArrayOutputStream out ^long n]
  (.write out (bit-and (bit-shift-right n 8) 0xff))
  (.write out (bit-and n 0xff)))

(defn- write-u32! [^ByteArrayOutputStream out ^long n]
  (.write out (bit-and (bit-shift-right n 24) 0xff))
  (.write out (bit-and (bit-shift-right n 16) 0xff))
  (.write out (bit-and (bit-shift-right n 8) 0xff))
  (.write out (bit-and n 0xff)))

(defn- write-name! [^ByteArrayOutputStream out ^String name]
  (doseq [^String label (.split name "\\.")]
    (when-not (.isEmpty label)
      (let [^bytes b (utf8-bytes label)]
        (.write out (alength b))
        (.write out b 0 (alength b)))))
  (.write out 0))

(defn- make-record
  ^bytes [^String name ^long record-type ^long ttl ^bytes rdata]
  (let [out (ByteArrayOutputStream.)]
    (write-name! out name)
    (write-u16! out record-type)
    (write-u16! out dns-class-in)
    (write-u32! out ttl)
    (write-u16! out (alength rdata))
    (.write out rdata 0 (alength rdata))
    (.toByteArray out)))

(defn- make-ptr-rdata ^bytes [^String full-service-name]
  (let [out (ByteArrayOutputStream.)]
    (write-name! out full-service-name)
    (.toByteArray out)))

(defn- make-srv-rdata ^bytes [^long port ^String host-name]
  (let [out (ByteArrayOutputStream.)]
    (write-u16! out 0) ; priority
    (write-u16! out 0) ; weight
    (write-u16! out port)
    (write-name! out host-name)
    (.toByteArray out)))

(defn- make-txt-rdata ^bytes [entries]
  (let [out (ByteArrayOutputStream.)]
    (doseq [^String entry entries]
      (let [^bytes b (utf8-bytes entry)]
        (.write out (alength b))
        (.write out b 0 (alength b))))
    (.toByteArray out)))

(defn- make-a-rdata ^bytes [a b c d]
  (byte-array [(byte a) (byte b) (byte c) (byte d)]))

(defn- make-response-packet
  ^bytes [records]
  (let [out (ByteArrayOutputStream.)]
    (write-u16! out 0) ; id
    (write-u16! out 0x8400) ; response
    (write-u16! out 0) ; questions
    (write-u16! out (count records)) ; answers
    (write-u16! out 0) ; ns
    (write-u16! out 0) ; ar
    (doseq [^bytes record records]
      (.write out record 0 (alength record)))
    (.toByteArray out)))

(defn- parse-and-rebuild!
  ([^MDNS mdns ^bytes packet]
   (parse-and-rebuild! mdns packet "127.0.0.1" "127.0.0.1"))
  ([^MDNS mdns ^bytes packet ^String local-address ^String remote-address]
   (MDNS$TestHooks/parsePacket mdns packet local-address remote-address)
   (.update mdns false)))

(defn- expire-and-rebuild!
  [^MDNS mdns]
  (.update mdns false))

(defn- service-records
  [service-type full-name host-name port ttl]
  [(make-record service-type dns-type-ptr ttl (make-ptr-rdata full-name))
   (make-record full-name dns-type-srv ttl (make-srv-rdata port host-name))
   (make-record full-name dns-type-txt ttl (make-txt-rdata ["id=test-id" "name=Test Device" "log_port=7001" "schema=1"]))
   (make-record host-name dns-type-a ttl (make-a-rdata 127 0 0 1))])

(defn- dummy-logger
  ^MDNS$Logger []
  (reify MDNS$Logger
    (log [_ _] nil)))

(defn- device-with-service-name [devices service-name]
  (some #(when (= service-name (.serviceName ^MDNSServiceInfo %)) %) devices))

(defn- await-device-state [^MDNS mdns service-name present?]
  (loop [i 0]
    (let [found? (boolean (device-with-service-name (.getDevices mdns) service-name))]
      (if (or (= found? present?) (>= i 200))
        found?
        (do
          (.update mdns false)
          (Thread/sleep 10)
          (recur (inc i)))))))

(deftest mdns-discovers-service-from-dns-sd-records
  (let [mdns (MDNS. (dummy-logger))
        service-type MDNS/MDNS_SERVICE_TYPE
        full-name (str "TargetOne." service-type)
        host-name "target-host.local"
        packet (make-response-packet (service-records service-type full-name host-name 8123 120))]
    (parse-and-rebuild! mdns packet)
    (let [devices ^com.dynamo.discovery.MDNSServiceInfo/1 (.getDevices mdns)]
      (is (= 1 (alength devices)))
      (let [^MDNSServiceInfo d (aget devices 0)]
        (is (= "test-id" (.id d)))
        (is (= "Test Device" (.instanceName d)))
        (is (= full-name (.serviceName d)))
        (is (= host-name (.host d)))
        (is (= "127.0.0.1" (.address d)))
        (is (= 8123 (.port d)))
        (is (= "7001" (.logPort d)))
        (is (= "1" (get (.txt d) "schema")))))))

(deftest mdns-discovers-service-when-records-arrive-out-of-order
  (let [mdns (MDNS. (dummy-logger))
        service-type MDNS/MDNS_SERVICE_TYPE
        full-name (str "TargetReordered." service-type)
        host-name "target-reordered.local"
        records (service-records service-type full-name host-name 8124 120)
        packet (make-response-packet [(second records) (nth records 2) (nth records 3) (first records)])]
    (parse-and-rebuild! mdns packet)
    (let [devices ^com.dynamo.discovery.MDNSServiceInfo/1 (.getDevices mdns)]
      (is (= 1 (alength devices)))
      (let [^MDNSServiceInfo d (aget devices 0)]
        (is (= full-name (.serviceName d)))
        (is (= 8124 (.port d)))
        (is (= "127.0.0.1" (.address d)))))))

(deftest mdns-discovers-utf8-name-from-txt-record
  (let [mdns (MDNS. (dummy-logger))
        service-type MDNS/MDNS_SERVICE_TYPE
        full-name (str "TargetUtf8." service-type)
        host-name "target-utf8.local"
        utf8-name "Tést Näme"
        packet (make-response-packet
                 [(make-record service-type dns-type-ptr 120 (make-ptr-rdata full-name))
                  (make-record full-name dns-type-srv 120 (make-srv-rdata 8126 host-name))
                  (make-record full-name dns-type-txt 120 (make-txt-rdata [(str "id=test-id")
                                                                           (str "name=" utf8-name)
                                                                           "log_port=7001"
                                                                           "schema=1"]))
                  (make-record host-name dns-type-a 120 (make-a-rdata 127 0 0 1))])]
    (parse-and-rebuild! mdns packet)
    (let [devices ^"[Lcom.dynamo.discovery.MDNSServiceInfo;" (.getDevices mdns)]
      (is (= 1 (alength devices)))
      (let [^MDNSServiceInfo d (aget devices 0)]
        (is (= utf8-name (.instanceName d)))
        (is (= utf8-name (get (.txt d) "name")))))))

(deftest mdns-keeps-packet-source-address-when-host-address-flaps
  (let [mdns (MDNS. (dummy-logger))
        service-type MDNS/MDNS_SERVICE_TYPE
        full-name (str "TargetSourceAddress." service-type)
        host-name "target-source-address.local"
        service-packet (make-response-packet (service-records service-type full-name host-name 8125 120))
        host-only-packet (make-response-packet [(make-record host-name dns-type-a 120 (make-a-rdata 10 0 0 99))])]
    (parse-and-rebuild! mdns service-packet "192.168.0.10" "192.168.0.42")
    (parse-and-rebuild! mdns host-only-packet "10.0.0.10" "10.0.0.99")
    (let [devices ^com.dynamo.discovery.MDNSServiceInfo/1 (.getDevices mdns)]
      (is (= 1 (alength devices)))
      (let [^MDNSServiceInfo d (aget devices 0)]
        (is (= "192.168.0.42" (.address d)))
        (is (= "192.168.0.10" (.localAddress d)))))))

(deftest mdns-removes-service-on-zero-ttl-records
  (let [mdns (MDNS. (dummy-logger))
        service-type MDNS/MDNS_SERVICE_TYPE
        full-name (str "TargetTwo." service-type)
        host-name "target-host-2.local"
        add-packet (make-response-packet (service-records service-type full-name host-name 9001 120))
        remove-packet (make-response-packet (service-records service-type full-name host-name 9001 0))]
    (parse-and-rebuild! mdns add-packet)
    (is (= 1 (alength (.getDevices mdns))))
    (parse-and-rebuild! mdns remove-packet)
    (is (= 0 (alength (.getDevices mdns))))))

(deftest mdns-expires-service-using-shortest-required-record-ttl
  (let [mdns (MDNS. (dummy-logger))
        service-type MDNS/MDNS_SERVICE_TYPE
        full-name (str "TargetShortTtl." service-type)
        host-name "target-short-ttl.local"
        ptr-record (make-record service-type dns-type-ptr 120 (make-ptr-rdata full-name))
        srv-record (make-record full-name dns-type-srv 1 (make-srv-rdata 9021 host-name))
        txt-record (make-record full-name dns-type-txt 1 (make-txt-rdata ["id=test-id" "name=Test Device" "log_port=7001" "schema=1"]))
        a-record (make-record host-name dns-type-a 120 (make-a-rdata 127 0 0 1))
        packet (make-response-packet [srv-record txt-record a-record ptr-record])]
    (parse-and-rebuild! mdns packet)
    (is (= 1 (alength (.getDevices mdns))))
    (Thread/sleep 1100)
    (expire-and-rebuild! mdns)
    (is (= 0 (alength (.getDevices mdns))))))

(deftest mdns-service-info-equality-includes-host-and-local-address
  (let [base (MDNSServiceInfo. 120
                               "id"
                               "instance"
                               "service"
                               "host-one.local"
                               "10.0.0.1"
                               "192.168.0.10"
                               8123
                               "7001"
                               {"schema" "1"})
        same (MDNSServiceInfo. 240
                               "id"
                               "instance"
                               "service"
                               "host-one.local"
                               "10.0.0.1"
                               "192.168.0.10"
                               8123
                               "7001"
                               {"schema" "1"})
        host-changed (MDNSServiceInfo. 120
                                      "id"
                                      "instance"
                                      "service"
                                      "host-two.local"
                                      "10.0.0.1"
                                      "192.168.0.10"
                                      8123
                                      "7001"
                                      {"schema" "1"})
        local-address-changed (MDNSServiceInfo. 120
                                               "id"
                                               "instance"
                                               "service"
                                               "host-one.local"
                                               "10.0.0.1"
                                               "192.168.0.11"
                                               8123
                                               "7001"
                                               {"schema" "1"})]
    (is (= base same))
    (is (not= base host-changed))
    (is (not= base local-address-changed))))

(deftest mdns-service-info-txt-is-unmodifiable
  (let [txt (doto (HashMap.)
              (.put "schema" "1"))
        info (MDNSServiceInfo. 120
                               "id"
                               "instance"
                               "service"
                               "host.local"
                               "10.0.0.1"
                               "192.168.0.10"
                               8123
                               "7001"
                               txt)]
    (is (thrown? UnsupportedOperationException
                 (.put (.txt info) "name" "changed")))))

(deftest mdns-live-multicast-integration
  (let [mdns (MDNS. (dummy-logger))]
    (try
      (when (and (.setup mdns)
                 (.isConnected mdns))
        (let [service-type MDNS/MDNS_SERVICE_TYPE
              suffix (str "-" (UUID/randomUUID))
              full-name (str "TargetLive" suffix "." service-type)
              host-name (str "target-live" suffix ".local")
              add-packet (make-response-packet (service-records service-type full-name host-name 9011 120))
              remove-packet (make-response-packet (service-records service-type full-name host-name 9011 0))
              group (InetAddress/getByName "224.0.0.251")]
          (try
            (with-open [socket (MulticastSocket.)]
              (.send socket (DatagramPacket. add-packet (alength add-packet) group 5353))
              (is (true? (await-device-state mdns full-name true)))

              (.send socket (DatagramPacket. remove-packet (alength remove-packet) group 5353))
              (is (false? (await-device-state mdns full-name false))))
            (catch java.io.IOException _ nil))))
      (catch Exception _ nil)
      (finally
        (.dispose mdns)))))
