import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]


@unittest.skipUnless(shutil.which("cmake") and (os.name == "nt" or shutil.which("ninja")),
                     "CMake and a build tool are required")
class BinaryEmbedTests(unittest.TestCase):
    def run_command(self, *command):
        result = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        self.assertEqual(0, result.returncode, result.stdout)

    # Compiled fixtures must retain every byte, their exact size and alignment,
    # including empty files, partial words and changes after the first build.
    def test_embedded_bytes_survive_compilation_and_rebuild(self):
        with tempfile.TemporaryDirectory(prefix="defold embed ") as directory:
            root = Path(directory)
            payload = root / "payload.bin"
            data = bytes(range(256)) * 257 + b"\xff\0\x81\r\n"
            payload.write_bytes(data)
            (root / "empty.bin").write_bytes(b"")
            (root / "legacy.bin").write_bytes(b"\0\xfflegacy\n")
            (root / "CMakeLists.txt").write_text('''
cmake_minimum_required(VERSION 3.20)
project(embed_test LANGUAGES C CXX)
list(APPEND CMAKE_MODULE_PATH "${DEFOLD_HOME}/scripts/cmake")
include(functions)
foreach(name IN ITEMS payload empty legacy)
  set(options WORD_ARRAY)
  if(name STREQUAL "legacy")
    set(options)
  endif()
  defold_embed_to_header(INPUT "${CMAKE_CURRENT_SOURCE_DIR}/${name}.bin"
    OUTPUT_HEADER "${CMAKE_CURRENT_BINARY_DIR}/generated headers/${name}.h"
    OUTPUT_CPP "${CMAKE_CURRENT_BINARY_DIR}/generated sources/${name}.cpp"
    SYMBOL ${name} OUT_CPP_VAR source ${options})
  list(APPEND sources "${source}")
endforeach()
add_executable(embed_test main.cpp ${sources})
target_include_directories(embed_test PRIVATE
  "${CMAKE_CURRENT_BINARY_DIR}/generated headers" "${DEFOLD_HOME}/engine/dlib/src")
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/executable-$<CONFIG>.txt"
  CONTENT "$<TARGET_FILE:embed_test>")
''')
            (root / "main.cpp").write_text('''
#include <stdio.h>
#include <stdint.h>
#include "payload.h"
#include "empty.h"
#include "legacy.h"
int main(int argc, char** argv)
{
    if (argc != 2 || empty_SIZE != 0 || (uintptr_t)payload % 16 != 0 || (uintptr_t)empty % 16 != 0)
    {
        return 1;
    }
    FILE* file = fopen(argv[1], "wb");
    if (!file)
    {
        return 2;
    }
    if (fwrite(payload, 1, payload_SIZE, file) != payload_SIZE ||
        fwrite(legacy, 1, legacy_SIZE, file) != legacy_SIZE)
    {
        return 3;
    }
    return fclose(file);
}
''')
            build = root / "build"
            # Visual Studio finds MSVC without requiring a developer shell.
            generator = [] if os.name == "nt" else ["-G", "Ninja"]
            self.run_command("cmake", "-S", str(root), "-B", str(build), *generator,
                             "-DCMAKE_BUILD_TYPE=Release", "-DDEFOLD_HOME=" + ROOT.as_posix())
            output = root / "roundtrip.bin"
            for expected in (data, b"changed\0\xff" + data + b"end"):
                payload.write_bytes(expected)
                self.run_command("cmake", "--build", str(build), "--config", "Release")
                executable = (build / "executable-Release.txt").read_text()
                self.run_command(executable, str(output))
                self.assertEqual(expected + b"\0\xfflegacy\n", output.read_bytes())


if __name__ == "__main__":
    unittest.main()
