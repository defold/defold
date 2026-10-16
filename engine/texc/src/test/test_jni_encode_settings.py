# Copyright 2020-2026 The Defold Foundation
# Copyright 2014-2020 King
# Copyright 2009-2014 Ragnar Svensson, Christian Murray
# Licensed under the Defold License version 1.0 (the "License"); you may not use
# this file except in compliance with the License.
#
# You may obtain a copy of the License, together with FAQs at
# https://www.defold.com/license
#
# Unless required by applicable law or agreed to in writing, software distributed
# under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
# CONDITIONS OF ANY KIND, either express or implied. See the License for the
# specific language governing permissions and limitations under the License.

"""Check native input ownership in the three JNI texture-encoder wrappers.

Run with Python 3 and Clang/AddressSanitizer. The test compiles the actual
wrappers and scoped owner, with controlled JNI conversion/encoder stubs, then
checks success, partial conversion, encoder failure, null Java output and
exception exits. It checks allocation lifetime, not codec output quality.
Use --expect-leak with an unpatched source as the negative control.
"""

from pathlib import Path
from tempfile import TemporaryDirectory
import argparse
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("source", nargs="?", type=Path,
                    default=Path(__file__).resolve().parents[1] / "texclibrary_jni.cpp")
parser.add_argument("--expect-leak", action="store_true",
                    help="Verify that an unpatched source fails the ownership check")
args = parser.parse_args()
source = args.source.read_text()
helper=source[source.index('    // The generated JNI converters'):source.index('    bool GetPixelFormat')] if '    // The generated JNI converters' in source else ''
wrappers=[]
for name in ['BasisU','ASTC','Default']:
 start=source.index('JNIEXPORT jobject JNICALL Java_TexcLibraryJni_'+name+'Encode(')
 end=source.index('\n}',start)+2
 wrappers.append(source[start:end])
prefix=r'''
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <sanitizer/asan_interface.h>
#define JNIEXPORT
#define JNICALL
#define dmLogError(...) ((void)0)
#define DM_JNI_GUARD_SCOPE_BEGIN() {
#define DM_JNI_GUARD_SCOPE_END(...) }
struct JNIEnv {};
typedef void* jclass;
typedef void* jobject;
static unsigned mode, encodes, conversions, results;
static uint8_t* allocated_data;
static char* allocated_path;
static uint8_t* allocated_output;
static unsigned checksum;
namespace dmTexc {
struct ASTCEncodeSettings {const char* m_Path; uint8_t* m_Data; unsigned m_DataCount;};
struct BasisUEncodeSettings {const char* m_Path; uint8_t* m_Data; unsigned m_DataCount;};
struct DefaultEncodeSettings {const char* m_Path; uint8_t* m_Data; unsigned m_DataCount;};
namespace jni {
struct TypeInfos {};
struct ScopedContext {TypeInfos m_TypeInfos; explicit ScopedContext(JNIEnv*) {}};
template<typename T> bool convert(T* s) {
 ++conversions;
 if(mode==1) return false;
 allocated_path=strdup("test-owned-encode-input"); s->m_Path=allocated_path;
 if(mode==2) return false;
 allocated_data=new uint8_t[65536]; s->m_Data=allocated_data; s->m_DataCount=65536;
 for(unsigned k=0;k<65536;++k) s->m_Data[k]=uint8_t(k*17+3);
 if(mode==3) return false;
 if(mode==4) throw std::runtime_error("converter");
 return true;
}
#define CONVERTER(Name) bool J2C_Create##Name##EncodeSettings(JNIEnv*,TypeInfos*,jobject,Name##EncodeSettings* s) {return convert(s);}
CONVERTER(ASTC) CONVERTER(BasisU) CONVERTER(Default)
}
template<typename T> bool encode(T* s,uint8_t** out,uint32_t* count) {
 ++encodes;
 assert(s->m_Data==allocated_data && s->m_Path==allocated_path && s->m_DataCount==65536);
 assert(!__asan_address_is_poisoned(s->m_Data) && !__asan_address_is_poisoned(s->m_Path));
 checksum=0;for(unsigned k=0;k<s->m_DataCount;++k) {assert(s->m_Data[k]==uint8_t(k*17+3));checksum+=s->m_Data[k];}
 if(mode==5) return false;
 if(mode==6) throw std::runtime_error("encoder");
 allocated_output=(uint8_t*)malloc(4);memcpy(allocated_output,&checksum,4);
 assert(allocated_output!=allocated_data);*out=allocated_output;*count=4;return true;
}
#define ENCODER(Name) bool Name##Encode(Name##EncodeSettings* s,uint8_t** o,uint32_t* n) {return encode(s,o,n);}
ENCODER(ASTC) ENCODER(BasisU) ENCODER(Default)
}
namespace dmJNI {
jobject C2J_CreateUByteArray(JNIEnv*,uint8_t* out,uint32_t n) {
 ++results;assert(n==4);unsigned got;memcpy(&got,out,4);assert(got==checksum);
 assert(!__asan_address_is_poisoned(allocated_data));
 return mode==7?0:(jobject)0x1234;
}
}
using namespace dmTexc::jni;
'''
suffix=r'''
// Verify cleanup after success and early exits; reject retained JNI input copies.
int main(int argc,char**) {
 typedef jobject (*Fn)(JNIEnv*,jclass,jobject);
 Fn functions[]={Java_TexcLibraryJni_ASTCEncode,Java_TexcLibraryJni_BasisUEncode,Java_TexcLibraryJni_DefaultEncode};
 const bool negative=argc>1;
 for(unsigned f=0;f<3;++f) for(unsigned m=0;m<(negative?1U:8U);++m) for(unsigned repeat=0;repeat<20;++repeat) {
  mode=m;allocated_data=0;allocated_path=0;allocated_output=0;encodes=conversions=results=0;
  jobject value=0;bool threw=false;
  try {value=functions[f](0,0,(jobject)1);} catch(const std::runtime_error&) {threw=true;}
  assert(conversions==1);assert(threw==(m==4 || m==6));
  assert(encodes==unsigned(m==0 || m>=5));assert(results==unsigned(m==0 || m==7));
  assert(value==(m==0?(jobject)0x1234:0));
  bool clean=(!allocated_data || __asan_address_is_poisoned(allocated_data)) && (!allocated_path || __asan_address_is_poisoned(allocated_path));
  if(!clean) {if(negative) {delete[] allocated_data;free(allocated_path);puts("EXPECTED_REJECT: retained owned JNI input");return 19;} assert(clean);}
  assert(!allocated_output || __asan_address_is_poisoned(allocated_output));
 }
 puts("PASS: 3 actual wrappers x 8 success/error/exception paths x 20 repeats; owned input/path released, encoding inputs/output unchanged");
}
'''
with TemporaryDirectory(prefix="texc-jni-ownership-") as directory:
    source_file = Path(directory) / "test.cpp"
    source_file.write_text(prefix + "\nnamespace {\n" + helper + "\n}\n" +
                           "\n".join(wrappers) + suffix)
    binary = source_file.with_suffix("")
    subprocess.run([
        "clang++", "-std=c++11", "-O1", "-g", "-fsanitize=address",
        "-fno-omit-frame-pointer", "-Wall", "-Wextra", "-Werror",
        "-Wno-unused-parameter", str(source_file), "-o", str(binary),
    ], check=True)
    result = subprocess.run([str(binary)] + (["negative"] if args.expect_leak else []))
    expected = 19 if args.expect_leak else 0
    if result.returncode != expected:
        raise SystemExit("Ownership regression check failed: exit %d (expected %d)" %
                         (result.returncode, expected))
