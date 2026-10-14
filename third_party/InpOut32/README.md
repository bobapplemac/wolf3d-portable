# InpOut32 prebuilt runtime

Unmodified Win32 DLL from the official Highrez InpOut32 1.5.0.1 binary archive.
DLL file version: 1.5.0.0 (the 1.5.0.1 release changed the license).
Size: 98304 bytes.
SHA-256: `01bcec6ddb4964e1f5b69ba1bd3876221d8de7ae17cacfac66f095013434a78f`.

- Upstream: https://vps.highrez.co.uk/downloads/InpOut32/default.htm
- Binaries: https://www.highrez.co.uk/scripts/download.asp?package=InpOutBinaries
- DLL source: https://www.highrez.co.uk/scripts/download.asp?package=Inpout32_dll_source
- Mirror: https://github.com/ellysh/InpOut32/blob/master/bin/Win32/inpout32.dll

LICENSE.txt is copied verbatim from the official binary archive. No local
binary modifications. The DLL embeds kernel drivers; do not load it on build
or test machines. Tests use a separate fake DLL for loader integration.

Vendoring this small legacy binary keeps builds offline and reproducible without
requiring the upstream Windows toolchain. Normal builds never download updates.
To update, obtain an official release, inspect its license, architecture, imports
and provenance without loading it, then update this record and the pinned SHA-256
in cmake/W3PPackageInpOut32.cmake together. Run tests/WG_INPOUT32_PACKAGE_TEST.py.
The checksum is an integrity pin, not an upstream digital signature.
