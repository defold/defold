// Copyright 2020-2026 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

#include <string.h>
#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>

#include "../dlib/array.h"
#include "../dlib/sslsocket.h"
#include "cacert.pem.embed.h"

// Verifies that the full built-in CA bundle loads, including P-521 certificates
// that caused the entire bundle to be rejected when that curve was disabled.
TEST(dmSSLSocket, LoadBuiltinCertificates)
{
    dmArray<uint8_t> certificates;
    certificates.SetCapacity(CACERT_PEM_SIZE + 1);
    certificates.SetSize(CACERT_PEM_SIZE + 1);
    memcpy(certificates.Begin(), CACERT_PEM, CACERT_PEM_SIZE);
    certificates[CACERT_PEM_SIZE] = 0;

    ASSERT_EQ(dmSSLSocket::RESULT_OK, dmSSLSocket::SetSslPublicKeys(certificates.Begin(), CACERT_PEM_SIZE));
}

int main(int argc, char** argv)
{
    jc_test_init(&argc, argv);
    if (dmSSLSocket::Initialize() != dmSSLSocket::RESULT_OK)
        return 1;

    int ret = jc_test_run_all();
    dmSSLSocket::Finalize();
    return ret;
}
