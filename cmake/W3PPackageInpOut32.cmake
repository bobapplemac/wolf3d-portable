# Portable Windows package finalizer. Run after WGPackageDocs.cmake.
if(NOT ENABLED OR NOT ADLIB)
    return()
endif()
file(READ "${DIST}/DOCS/BUILD.TXT" info)
if(NOT info MATCHES "Platform baseline: (win2k|winxp)\n"
   OR NOT info MATCHES "Architecture: x86\n"
   OR NOT info MATCHES "Backend: (gdi|sdl1)\n")
    return()
endif()
set(source "${ROOT}/third_party/InpOut32")
set(expected "01bcec6ddb4964e1f5b69ba1bd3876221d8de7ae17cacfac66f095013434a78f")
file(SHA256 "${source}/inpout32.dll" actual)
if(NOT actual STREQUAL expected)
    message(FATAL_ERROR "InpOut32 SHA-256 mismatch; refusing to package unverified DLL")
endif()
# Read all required documentation before copying any payload.
file(READ "${source}/NOTICE.txt" notice)
file(READ "${source}/LICENSE.txt" license)
configure_file("${source}/inpout32.dll" "${DIST}/inpout32.dll" COPYONLY)
configure_file("${source}/LICENSE.txt" "${DIST}/DOCS/LICENSES/INPOUT32.TXT" COPYONLY)
file(APPEND "${DIST}/DOCS/NOTICES.TXT" "${notice}")
file(APPEND "${DIST}/DOCS/BUILD.TXT"
    "\nInpOut32 release: 1.5.0.1\nInpOut32 DLL file version: 1.5.0.0\nInpOut32 SHA-256: ${actual}\nInpOut32 origin: official Highrez Win32 binary, unmodified\n")
file(APPEND "${DIST}/README.TXT"
    "\nNative AdLib runtime\n--------------------\nThis package includes inpout32.dll. Keep it beside wolf3d.exe.\nUse --opl adlib to select native FM; Auto is the default.\nInitial use can install/start its embedded kernel driver and requires\nAdministrator privileges on the destination machine. No separate installer\nor SYS file is needed. See DOCS/NOTICES.TXT and DOCS/LICENSES/INPOUT32.TXT.\n")
