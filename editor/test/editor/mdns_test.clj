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
  (:require [clojure.string :as str]
            [clojure.test :refer :all])
  (:import [com.dynamo.discovery MDNS MDNS$Logger MDNS$TestHooks MDNSServiceInfo]
           [java.io ByteArrayOutputStream]
           [java.util Arrays HashMap UUID]
           [java.net DatagramPacket InetAddress MulticastSocket NetworkInterface]))

(set! *warn-on-reflection* true)

(def ^:private dns-type-ptr 12)
(def ^:private dns-type-txt 16)
(def ^:private dns-type-a 1)
(def ^:private dns-type-srv 33)
(def ^:private dns-class-in 1)
(def ^:private default-txt-entries ["id=test-id" "name=Test Device" "log_port=7001" "schema=1"])

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

(defn- name-wire-length
  ^long [^String name]
  (inc (reduce + (map #(+ 1 (alength (utf8-bytes ^String %)))
                      (remove empty? (.split name "\\."))))))

(defn- patch-u16!
  [^bytes data ^long offset ^long n]
  (aset-byte data (int offset) (unchecked-byte (bit-and (bit-shift-right n 8) 0xff)))
  (aset-byte data (int (inc offset)) (unchecked-byte (bit-and n 0xff))))

(defn- write-name-compressed!
  [^ByteArrayOutputStream out offsets ^String name]
  (loop [labels (vec (remove empty? (.split name "\\.")))]
    (if (empty? labels)
      (.write out 0)
      (let [suffix (str/join "." labels)]
        (if-let [offset (get @offsets suffix)]
          (write-u16! out (bit-or 0xC000 offset))
          (do
            (swap! offsets #(if (contains? % suffix) % (assoc % suffix (.size out))))
            (let [^bytes b (utf8-bytes (first labels))]
              (.write out (alength b))
              (.write out b 0 (alength b))
              (recur (subvec labels 1)))))))))

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

(defn- write-txt-entry!
  [^ByteArrayOutputStream out ^String entry]
  (let [^bytes b (utf8-bytes entry)]
    (.write out (alength b))
    (.write out b 0 (alength b))))

(defn- make-a-rdata ^bytes [a b c d]
  (byte-array [(byte a) (byte b) (byte c) (byte d)]))

(defn- write-compressed-record!
  [^ByteArrayOutputStream out offsets patches ^String name record-type ttl write-rdata!]
  (write-name-compressed! out offsets name)
  (write-u16! out record-type)
  (write-u16! out dns-class-in)
  (write-u32! out ttl)
  (let [rdlength-offset (.size out)]
    (write-u16! out 0)
    (let [rdata-start (.size out)]
      (write-rdata! out offsets)
      (swap! patches conj [rdlength-offset (- (.size out) rdata-start)]))))

(defn- ptr-record-writer [^String service-type ^String full-service-name ^long ttl]
  (fn [^ByteArrayOutputStream out offsets patches]
    (write-compressed-record! out offsets patches service-type dns-type-ptr ttl
                              (fn [^ByteArrayOutputStream out offsets]
                                (write-name-compressed! out offsets full-service-name)))))

(defn- srv-record-writer [^String full-name ^String host-name ^long port ^long ttl]
  (fn [^ByteArrayOutputStream out offsets patches]
    (write-compressed-record! out offsets patches full-name dns-type-srv ttl
                              (fn [^ByteArrayOutputStream out offsets]
                                (write-u16! out 0)
                                (write-u16! out 0)
                                (write-u16! out port)
                                (write-name-compressed! out offsets host-name)))))

(defn- txt-record-writer [^String full-name entries ^long ttl]
  (fn [^ByteArrayOutputStream out offsets patches]
    (write-compressed-record! out offsets patches full-name dns-type-txt ttl
                              (fn [^ByteArrayOutputStream out _]
                                (doseq [^String entry entries]
                                  (let [^bytes b (utf8-bytes entry)]
                                    (.write out (alength b))
                                    (.write out b 0 (alength b))))))))

(defn- a-record-writer [^String host-name address ^long ttl]
  (fn [^ByteArrayOutputStream out offsets patches]
    (write-compressed-record! out offsets patches host-name dns-type-a ttl
                              (fn [^ByteArrayOutputStream out _]
                                (let [^bytes rdata (apply make-a-rdata address)]
                                  (.write out rdata 0 (alength rdata)))))))

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

(defn- make-response-packet-sections
  ^bytes [answers authorities additionals]
  (let [out (ByteArrayOutputStream.)]
    (write-u16! out 0)
    (write-u16! out 0x8400)
    (write-u16! out 0)
    (write-u16! out (count answers))
    (write-u16! out (count authorities))
    (write-u16! out (count additionals))
    (doseq [^bytes record (concat answers authorities additionals)]
      (.write out record 0 (alength record)))
    (.toByteArray out)))

(defn- make-response-packet-compressed
  ^bytes [record-writers]
  (let [out (ByteArrayOutputStream.)
        offsets (atom {})
        patches (atom [])]
    (write-u16! out 0)
    (write-u16! out 0x8400)
    (write-u16! out 0)
    (write-u16! out (count record-writers))
    (write-u16! out 0)
    (write-u16! out 0)
    (doseq [record-writer record-writers]
      (record-writer out offsets patches))
    (let [packet (.toByteArray out)]
      (doseq [[offset len] @patches]
        (patch-u16! packet offset len))
      packet)))

(defn- make-query-packet
  ^bytes [^String qname ^long qtype]
  (let [out (ByteArrayOutputStream.)]
    (write-u16! out 0)
    (write-u16! out 0)
    (write-u16! out 1)
    (write-u16! out 0)
    (write-u16! out 0)
    (write-u16! out 0)
    (write-name! out qname)
    (write-u16! out qtype)
    (write-u16! out dns-class-in)
    (.toByteArray out)))

(defn- make-pointer-loop-packet
  ^bytes []
  (let [out (ByteArrayOutputStream.)]
    (write-u16! out 0)
    (write-u16! out 0x8400)
    (write-u16! out 0)
    (write-u16! out 1)
    (write-u16! out 0)
    (write-u16! out 0)
    (write-u16! out 0xC00C)
    (write-u16! out dns-type-ptr)
    (write-u16! out dns-class-in)
    (write-u32! out 120)
    (write-u16! out 2)
    (write-u16! out 0xC00C)
    (.toByteArray out)))

(defn- make-bad-rdlength-packet
  ^bytes [^String service-type ^String full-name]
  (let [packet (make-response-packet
                 [(make-record service-type dns-type-ptr 120 (make-ptr-rdata full-name))])
        rdlength-offset (+ 12 (name-wire-length service-type) 8)]
    (patch-u16! packet rdlength-offset 0x00ff)
    packet))

(defn- parse-and-rebuild!
  ([^MDNS mdns ^bytes packet]
   (parse-and-rebuild! mdns packet "127.0.0.1" "127.0.0.1"))
  ([^MDNS mdns ^bytes packet ^String local-address ^String remote-address]
   (MDNS$TestHooks/parsePacket mdns packet local-address remote-address)
   (.update mdns false)))

(defn- expire-and-rebuild!
  [^MDNS mdns]
  (.update mdns false))

(defn- packet-bytes
  ^bytes [^DatagramPacket packet]
  (Arrays/copyOfRange (.getData packet)
                      (.getOffset packet)
                      (+ (.getOffset packet) (.getLength packet))))

(defn- service-records
  [service-type full-name host-name port ttl-or-options]
  (let [{:keys [ptr-ttl srv-ttl txt-ttl a-ttl txt-entries a-address]}
        (if (map? ttl-or-options)
          ttl-or-options
          {:ptr-ttl ttl-or-options
           :srv-ttl ttl-or-options
           :txt-ttl ttl-or-options
           :a-ttl ttl-or-options})
        ptr-ttl (or ptr-ttl 120)
        srv-ttl (or srv-ttl 120)
        txt-ttl (or txt-ttl 120)
        a-ttl (or a-ttl 120)
        txt-entries (or txt-entries default-txt-entries)
        a-address (or a-address [127 0 0 1])]
    [(make-record service-type dns-type-ptr ptr-ttl (make-ptr-rdata full-name))
     (make-record full-name dns-type-srv srv-ttl (make-srv-rdata port host-name))
     (make-record full-name dns-type-txt txt-ttl (make-txt-rdata txt-entries))
     (apply make-record host-name dns-type-a a-ttl [(apply make-a-rdata a-address)])]))

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

;; Verifies the happy-path DNS-SD parse so the suite has a baseline resolved device shape.
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

;; Verifies record accumulation is order-independent because multicast responses are not ordered.
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

;; Verifies name compression is decoded correctly because real mDNS packets commonly compress suffixes.
(deftest mdns-discovers-service-from-compressed-dns-sd-records
  (let [mdns (MDNS. (dummy-logger))
        service-type MDNS/MDNS_SERVICE_TYPE
        full-name (str "TargetCompressed." service-type)
        host-name "target-compressed.local"
        packet (make-response-packet-compressed
                 [(ptr-record-writer service-type full-name 120)
                  (srv-record-writer full-name host-name 8127 120)
                  (txt-record-writer full-name default-txt-entries 120)
                  (a-record-writer host-name [127 0 0 1] 120)])]
    (parse-and-rebuild! mdns packet)
    (let [devices ^com.dynamo.discovery.MDNSServiceInfo/1 (.getDevices mdns)]
      (is (= 1 (alength devices)))
      (let [^MDNSServiceInfo d (aget devices 0)]
        (is (= full-name (.serviceName d)))
        (is (= host-name (.host d)))
        (is (= "test-id" (.id d)))
        (is (= 8127 (.port d)))
        (is (= "127.0.0.1" (.address d)))))))

;; Verifies a device is published only after PTR, SRV, TXT, and A data has been accumulated.
(deftest mdns-discovers-service-only-after-all-records-arrive-across-packets
  (let [mdns (MDNS. (dummy-logger))
        service-type MDNS/MDNS_SERVICE_TYPE
        full-name (str "TargetPartial." service-type)
        host-name "target-partial.local"
        [ptr-record srv-record txt-record a-record] (service-records service-type full-name host-name 8128 120)]
    (doseq [packet [(make-response-packet [srv-record])
                    (make-response-packet [txt-record])
                    (make-response-packet [ptr-record])]]
      (parse-and-rebuild! mdns packet "127.0.0.1" nil)
      (is (= 0 (alength (.getDevices mdns)))))
    (parse-and-rebuild! mdns (make-response-packet [a-record]) "127.0.0.1" nil)
    (let [devices ^com.dynamo.discovery.MDNSServiceInfo/1 (.getDevices mdns)]
      (is (= 1 (alength devices)))
      (is (= full-name (.serviceName ^MDNSServiceInfo (aget devices 0)))))))

;; Verifies UTF-8 TXT payloads survive parsing because instance names are sourced from TXT metadata.
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
    (let [devices ^com.dynamo.discovery.MDNSServiceInfo/1 (.getDevices mdns)]
      (is (= 1 (alength devices)))
      (let [^MDNSServiceInfo d (aget devices 0)]
        (is (= utf8-name (.instanceName d)))
        (is (= utf8-name (get (.txt d) "name")))))))

;; Verifies the response source address stays authoritative even if later A records change the host cache.
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

;; Verifies id falls back to the full service name so discovery still yields a stable key without TXT id.
(deftest mdns-falls-back-to-full-service-name-when-id-is-missing
  (let [mdns (MDNS. (dummy-logger))
        service-type MDNS/MDNS_SERVICE_TYPE
        full-name (str "TargetNoId." service-type)
        host-name "target-no-id.local"
        packet (make-response-packet
                 (service-records service-type full-name host-name 8129 {:txt-entries ["name=Fallback Device"
                                                                                       "log_port=7001"
                                                                                       "schema=1"]}))]
    (parse-and-rebuild! mdns packet)
    (let [devices ^com.dynamo.discovery.MDNSServiceInfo/1 (.getDevices mdns)]
      (is (= 1 (alength devices)))
      (let [^MDNSServiceInfo d (aget devices 0)]
        (is (= full-name (.id d)))
        (is (= "Fallback Device" (.instanceName d)))))))

;; Verifies display name falls back to the PTR instance label when TXT omits an explicit name.
(deftest mdns-falls-back-to-instance-label-when-name-is-missing
  (let [mdns (MDNS. (dummy-logger))
        service-type MDNS/MDNS_SERVICE_TYPE
        instance-name "TargetNoName"
        full-name (str instance-name "." service-type)
        host-name "target-no-name.local"
        packet (make-response-packet
                 (service-records service-type full-name host-name 8130 {:txt-entries ["id=test-id"
                                                                                       "log_port=7001"
                                                                                       "schema=1"]}))]
    (parse-and-rebuild! mdns packet)
    (let [devices ^com.dynamo.discovery.MDNSServiceInfo/1 (.getDevices mdns)]
      (is (= 1 (alength devices)))
      (let [^MDNSServiceInfo d (aget devices 0)]
        (is (= "test-id" (.id d)))
        (is (= instance-name (.instanceName d)))))))

;; Verifies a full zero-TTL deannouncement removes a device immediately instead of waiting for expiry.
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

;; Verifies a standalone PTR ttl=0 removes the service because that branch is separate from SRV/TXT removal.
(deftest mdns-removes-service-on-zero-ttl-ptr-record
  (let [mdns (MDNS. (dummy-logger))
        service-type MDNS/MDNS_SERVICE_TYPE
        full-name (str "TargetPtrZero." service-type)
        host-name "target-ptr-zero.local"
        add-packet (make-response-packet (service-records service-type full-name host-name 9030 120))
        remove-packet (make-response-packet [(make-record service-type dns-type-ptr 0 (make-ptr-rdata full-name))])]
    (parse-and-rebuild! mdns add-packet)
    (is (= 1 (alength (.getDevices mdns))))
    (parse-and-rebuild! mdns remove-packet)
    (is (= 0 (alength (.getDevices mdns))))))

;; Verifies a standalone SRV ttl=0 clears endpoint data and removes the resolved device.
(deftest mdns-removes-service-on-zero-ttl-srv-record
  (let [mdns (MDNS. (dummy-logger))
        service-type MDNS/MDNS_SERVICE_TYPE
        full-name (str "TargetSrvZero." service-type)
        host-name "target-srv-zero.local"
        add-packet (make-response-packet (service-records service-type full-name host-name 9031 120))
        remove-packet (make-response-packet [(make-record full-name dns-type-srv 0 (make-srv-rdata 9031 host-name))])]
    (parse-and-rebuild! mdns add-packet)
    (is (= 1 (alength (.getDevices mdns))))
    (parse-and-rebuild! mdns remove-packet)
    (is (= 0 (alength (.getDevices mdns))))))

;; Verifies a standalone TXT ttl=0 clears metadata and removes the resolved device.
(deftest mdns-removes-service-on-zero-ttl-txt-record
  (let [mdns (MDNS. (dummy-logger))
        service-type MDNS/MDNS_SERVICE_TYPE
        full-name (str "TargetTxtZero." service-type)
        host-name "target-txt-zero.local"
        add-packet (make-response-packet (service-records service-type full-name host-name 9032 120))
        remove-packet (make-response-packet [(make-record full-name dns-type-txt 0 (make-txt-rdata default-txt-entries))])]
    (parse-and-rebuild! mdns add-packet)
    (is (= 1 (alength (.getDevices mdns))))
    (parse-and-rebuild! mdns remove-packet)
    (is (= 0 (alength (.getDevices mdns))))))

;; Verifies a standalone A ttl=0 removes the cached host address and hides the device.
(deftest mdns-removes-service-on-zero-ttl-a-record
  (let [mdns (MDNS. (dummy-logger))
        service-type MDNS/MDNS_SERVICE_TYPE
        full-name (str "TargetAZero." service-type)
        host-name "target-a-zero.local"
        add-packet (make-response-packet (service-records service-type full-name host-name 9035 120))
        remove-packet (make-response-packet [(make-record host-name dns-type-a 0 (make-a-rdata 127 0 0 1))])]
    (parse-and-rebuild! mdns add-packet "192.168.0.10" nil)
    (is (= 1 (alength (.getDevices mdns))))
    (parse-and-rebuild! mdns remove-packet "192.168.0.10" nil)
    (is (= 0 (alength (.getDevices mdns))))))

;; Verifies the shortest required record TTL wins because service validity depends on all required records.
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

;; Verifies host-address expiry also removes the device even when the service records are still valid.
(deftest mdns-expires-service-when-host-address-expires
  (let [mdns (MDNS. (dummy-logger))
        service-type MDNS/MDNS_SERVICE_TYPE
        full-name (str "TargetHostExpiry." service-type)
        host-name "target-host-expiry.local"
        packet (make-response-packet
                 (service-records service-type full-name host-name 9033 {:ptr-ttl 120
                                                                         :srv-ttl 120
                                                                         :txt-ttl 120
                                                                         :a-ttl 1}))]
    (parse-and-rebuild! mdns packet "192.168.0.10" nil)
    (is (= 1 (alength (.getDevices mdns))))
    (Thread/sleep 1100)
    (expire-and-rebuild! mdns)
    (is (= 0 (alength (.getDevices mdns))))))

;; Verifies the parser walks answer, authority, and additional sections because discovery data can be split across them.
(deftest mdns-parses-records-from-answer-authority-and-additional-sections
  (let [mdns (MDNS. (dummy-logger))
        service-type MDNS/MDNS_SERVICE_TYPE
        full-name (str "TargetSections." service-type)
        host-name "target-sections.local"
        packet (make-response-packet-sections
                 [(make-record service-type dns-type-ptr 120 (make-ptr-rdata full-name))]
                 [(make-record full-name dns-type-srv 120 (make-srv-rdata 9036 host-name))]
                 [(make-record full-name dns-type-txt 120 (make-txt-rdata default-txt-entries))
                  (make-record host-name dns-type-a 120 (make-a-rdata 127 0 0 1))])]
    (parse-and-rebuild! mdns packet)
    (let [devices ^com.dynamo.discovery.MDNSServiceInfo/1 (.getDevices mdns)]
      (is (= 1 (alength devices)))
      (let [^MDNSServiceInfo d (aget devices 0)]
        (is (= full-name (.serviceName d)))
        (is (= 9036 (.port d)))
        (is (= "127.0.0.1" (.address d)))))))

;; Verifies TXT entries without '=' become empty values and zero-length entries stop parsing as intended.
(deftest mdns-parses-txt-entry-without-value-and-stops-at-zero-length-entry
  (let [mdns (MDNS. (dummy-logger))
        service-type MDNS/MDNS_SERVICE_TYPE
        instance-name "TargetTxtZeroLength"
        full-name (str instance-name "." service-type)
        host-name "target-txt-zero-length.local"
        txt-rdata (doto (ByteArrayOutputStream.)
                    (write-txt-entry! "id=test-id")
                    (write-txt-entry! "flag")
                    (.write 0)
                    (write-txt-entry! "name=Ignored"))
        packet (make-response-packet
                 [(make-record service-type dns-type-ptr 120 (make-ptr-rdata full-name))
                  (make-record full-name dns-type-srv 120 (make-srv-rdata 9037 host-name))
                  (make-record full-name dns-type-txt 120 (.toByteArray txt-rdata))
                  (make-record host-name dns-type-a 120 (make-a-rdata 127 0 0 1))])]
    (parse-and-rebuild! mdns packet)
    (let [devices ^com.dynamo.discovery.MDNSServiceInfo/1 (.getDevices mdns)]
      (is (= 1 (alength devices)))
      (let [^MDNSServiceInfo d (aget devices 0)]
        (is (= instance-name (.instanceName d)))
        (is (= "" (get (.txt d) "flag")))
        (is (nil? (get (.txt d) "name")))))))

;; Verifies truncated TXT tails are ignored after valid entries so malformed metadata does not corrupt state.
(deftest mdns-ignores-truncated-tail-in-txt-record
  (let [mdns (MDNS. (dummy-logger))
        service-type MDNS/MDNS_SERVICE_TYPE
        instance-name "TargetTxtTruncated"
        full-name (str instance-name "." service-type)
        host-name "target-txt-truncated.local"
        truncated-tail (utf8-bytes "oops")
        txt-rdata (doto (ByteArrayOutputStream.)
                    (write-txt-entry! "id=test-id")
                    (.write 8)
                    (.write truncated-tail 0 (alength truncated-tail)))
        packet (make-response-packet
                 [(make-record service-type dns-type-ptr 120 (make-ptr-rdata full-name))
                  (make-record full-name dns-type-srv 120 (make-srv-rdata 9038 host-name))
                  (make-record full-name dns-type-txt 120 (.toByteArray txt-rdata))
                  (make-record host-name dns-type-a 120 (make-a-rdata 127 0 0 1))])]
    (parse-and-rebuild! mdns packet)
    (let [devices ^com.dynamo.discovery.MDNSServiceInfo/1 (.getDevices mdns)]
      (is (= 1 (alength devices)))
      (let [^MDNSServiceInfo d (aget devices 0)]
        (is (= instance-name (.instanceName d)))
        (is (= "test-id" (get (.txt d) "id")))
        (is (= 1 (count (.txt d))))))))

;; Verifies malformed packets are ignored without dropping already discovered devices.
(deftest mdns-ignores-malformed-packets-without-dropping-existing-services
  (let [mdns (MDNS. (dummy-logger))
        service-type MDNS/MDNS_SERVICE_TYPE
        full-name (str "TargetMalformed." service-type)
        host-name "target-malformed.local"
        add-packet (make-response-packet (service-records service-type full-name host-name 9034 120))
        truncated-packet (Arrays/copyOf add-packet (dec (alength add-packet)))
        bad-rdlength-packet (make-bad-rdlength-packet service-type full-name)
        pointer-loop-packet (make-pointer-loop-packet)]
    (parse-and-rebuild! mdns add-packet)
    (doseq [packet [truncated-packet bad-rdlength-packet pointer-loop-packet]]
      (parse-and-rebuild! mdns packet)
      (let [devices ^com.dynamo.discovery.MDNSServiceInfo/1 (.getDevices mdns)]
        (is (= 1 (alength devices)))
        (is (= full-name (.serviceName ^MDNSServiceInfo (aget devices 0))))))))

;; Verifies DNS questions are not mistaken for answers so query traffic never creates phantom devices.
(deftest mdns-query-packets-do-not-create-devices
  (let [mdns (MDNS. (dummy-logger))
        packet (make-query-packet MDNS/MDNS_SERVICE_TYPE dns-type-ptr)]
    (parse-and-rebuild! mdns packet)
    (is (= 0 (alength (.getDevices mdns))))))

;; Verifies buildQuery and sendQuery emit the expected PTR query bytes because query plumbing is unit-tested here.
(deftest mdns-builds-and-sends-standard-ptr-query
  (let [mdns (MDNS. (dummy-logger))
        ^bytes expected (make-query-packet MDNS/MDNS_SERVICE_TYPE dns-type-ptr)
        sent-packet (atom nil)
        socket (proxy [MulticastSocket] [0]
                 (send [^DatagramPacket packet]
                   (reset! sent-packet {:address (.getHostAddress (.getAddress packet))
                                        :port (.getPort packet)
                                        :data (packet-bytes packet)})))]
    (MDNS$TestHooks/setMulticastAddress mdns (InetAddress/getByName "224.0.0.251"))
    (MDNS$TestHooks/addConnection mdns nil "192.168.0.10" socket)
    (let [built ^bytes (MDNS$TestHooks/buildQuery mdns)]
      (is (Arrays/equals expected built))
      (MDNS$TestHooks/sendQuery mdns)
      (is (= "224.0.0.251" (:address @sent-packet)))
      (is (= 5353 (:port @sent-packet)))
      (is (Arrays/equals built ^bytes (:data @sent-packet))))))

;; Verifies readPackets feeds datagrams through parsing with the connection local and remote addresses preserved.
(deftest mdns-read-packets-parses-service-announcements-from-connections
  (let [mdns (MDNS. (dummy-logger))
        service-type MDNS/MDNS_SERVICE_TYPE
        full-name (str "TargetReadPackets." service-type)
        host-name "target-read-packets.local"
        queued-packets (atom [{:payload (make-response-packet (service-records service-type full-name host-name 9039 120))
                               :remote-address "192.168.0.42"}])
        socket (proxy [MulticastSocket] [0]
                 (receive [^DatagramPacket packet]
                   (if-let [{:keys [payload remote-address]} (first @queued-packets)]
                     (let [^bytes payload payload]
                       (swap! queued-packets #(vec (rest %)))
                       (System/arraycopy payload 0 (.getData packet) 0 (alength payload))
                       (.setLength packet (alength payload))
                       (.setAddress packet (InetAddress/getByName remote-address))
                       (.setPort packet 5353))
                     (throw (java.net.SocketTimeoutException.)))))]
    (MDNS$TestHooks/addConnection mdns nil "192.168.0.10" socket)
    (MDNS$TestHooks/readPackets mdns)
    (MDNS$TestHooks/rebuildDiscovered mdns)
    (let [devices ^com.dynamo.discovery.MDNSServiceInfo/1 (.getDevices mdns)]
      (is (= 1 (alength devices)))
      (let [^MDNSServiceInfo d (aget devices 0)]
        (is (= full-name (.serviceName d)))
        (is (= "192.168.0.42" (.address d)))
        (is (= "192.168.0.10" (.localAddress d)))))))

;; Verifies refreshNetworks clears stale discovery state on interface changes because cached data is interface-bound.
(deftest mdns-refresh-networks-clears-state-on-interface-change
  (let [mdns (MDNS. (dummy-logger))
        service-type MDNS/MDNS_SERVICE_TYPE
        full-name (str "TargetRefreshNetworks." service-type)
        host-name "target-refresh-networks.local"
        loopback-interface (NetworkInterface/getByInetAddress (InetAddress/getLoopbackAddress))
        socket (MulticastSocket. 0)]
    (parse-and-rebuild! mdns (make-response-packet (service-records service-type full-name host-name 9040 120)) "192.168.0.10" nil)
    (is (= 1 (alength (.getDevices mdns))))
    (MDNS$TestHooks/setMulticastAddress mdns (InetAddress/getByName "224.0.0.251"))
    (MDNS$TestHooks/setInterfaces mdns [loopback-interface])
    (MDNS$TestHooks/addConnection mdns loopback-interface "192.168.0.10" socket)
    (MDNS$TestHooks/refreshNetworks mdns)
    (is (.isClosed socket))
    (is (= 0 (alength (.getDevices mdns))))
    (is (= 0 (MDNS$TestHooks/serviceCount mdns)))
    (is (= 0 (MDNS$TestHooks/hostCount mdns)))))

;; Verifies equality includes host and local address so endpoint moves are visible to change detection.
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

;; Verifies TXT maps exposed by MDNSServiceInfo are immutable to protect cached discovery state.
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

;; Smoke-tests the real multicast path end-to-end because socket integration still matters beyond packet unit tests.
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
