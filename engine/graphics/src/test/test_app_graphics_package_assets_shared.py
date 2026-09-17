
HEADER_TEMPLATE = """
#ifndef DM_TEST_APP_%s
#define DM_TEST_APP_%s
namespace graphics_assets
{
%s
}
#endif
"""

def get_file_contents(file_path):
    f = open(file_path, "rb")
    src = f.read()
    buf = []
    for s in src:
        buf.append(hex(s))
    return buf
