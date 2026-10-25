"""Apply Mini3 USB controller fixes to a private SDK boot build copy."""
import pathlib
import sys
root, sdk = map(pathlib.Path, sys.argv[1:])
config = root/'cmake/boot_standalone.cmake'
s = config.read_text()
needle = 'set(_BOOT_STANDALONE_BOOT_ADB_PAYLOAD_SIZE "65536")'
assert s.count(needle) == 1
config.write_text(s.replace(needle, 'set(_BOOT_STANDALONE_BOOT_ADB_PAYLOAD_SIZE "4096")'))
backend=root/'src/boot_adb_backend_tinyusb.c'
s=backend.read_text()
s=s.replace('#include "boot_adb_backend.h"','#include "boot_adb_backend.h"\n#include "arcs_ap.h"')
needle='    return tusb_init();'
assert s.count(needle)==1
s=s.replace(needle,'''    bool ok=tusb_init();
    /* ARCS reset does not preset endpoint/reset interrupt enables. */
    *(volatile uint8_t *)(USBC_BASE+0x0b)=0x07; /* suspend/resume/reset */
    *(volatile uint16_t *)(USBC_BASE+0x06)=0x01; /* endpoint 0 */
    return ok;''')
backend.write_text(s)
source=sdk/'modules/tinyusb/src/portable/mentor/musb/dcd_musb.c'
s=source.read_text()
needle='i < TUP_DCD_ENDPOINT_MAX'
assert s.count(needle)==2
s=s.replace(needle,'i < 8 /* ARCS has endpoints 0..7 */')
(root/'src/dcd_musb_mini3.c').write_text(s)
source = sdk/'components/tinyusb-appclass/adb/adb_sync.c'
s = source.read_text()
needle = '\tadb_sync_try_resolve_existing_path(&full_path, false);'
assert s.count(needle) == 2
s = s.replace(needle, '''\t/* Raw recovery works without a mounted/initialized filesystem. */
\tif (!adb_sync_is_ext_disk_access(full_path)) {
\t\tadb_sync_try_resolve_existing_path(&full_path, false);
\t}''')
(root/'src/adb_sync_mini3.c').write_text(s)
source = sdk/'components/tinyusb-appclass/adb/adb.c'
s = source.read_text()
for field, old in [('name', 'mido'), ('model', 'listenai'), ('device', 'mido')]:
    needle = f'ro.product.{field}={old};'
    assert s.count(needle) == 1
    s = s.replace(needle, f'ro.product.{field}=arcs_mini3;')
(root/'src/adb_mini3.c').write_text(s)
p=root/'CMakeLists.txt'
p.write_text(p.read_text()+'''
# Build the ARCS endpoint-count correction without changing the SDK checkout.
get_target_property(_tiny_sources psram_tinyusb SOURCES)
list(FILTER _tiny_sources EXCLUDE REGEX "/dcd_musb\\\\.c$")
set_property(TARGET psram_tinyusb PROPERTY SOURCES "${_tiny_sources}")
target_sources(psram_tinyusb PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/dcd_musb_mini3.c")
get_target_property(_adb_sources adb SOURCES)
list(FILTER _adb_sources EXCLUDE REGEX "(^|/)adb_sync\\\\.c$")
list(FILTER _adb_sources EXCLUDE REGEX "(^|/)adb\\\\.c$")
set_property(TARGET adb PROPERTY SOURCES "${_adb_sources}")
target_include_directories(adb PRIVATE "${ARCS_BASE}/components/tinyusb-appclass/adb")
target_sources(adb PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/adb_sync_mini3.c")
target_sources(adb PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/adb_mini3.c")
''')
