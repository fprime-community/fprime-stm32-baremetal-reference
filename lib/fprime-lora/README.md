# fprime-lora

Experimental, hardware-independent radio transport for F Prime deployments.
The radio hardware driver remains separate; the current reference deployments
use the RFM9X driver from `fprime-sensors`.

The [RadioTransport](fprime-lora/RadioTransport/README.md) library defines
the current fragmentation protocol, telemetry queue, and experimental
command transport. The command uplink is not integrated and this library is
not yet a complete or operational radio link. Its protocol is experimental.

## Integration

Add the library's root directory to the consuming project's `library_locations`
in `settings.ini`. The F Prime build loads [library.cmake](library.cmake);
consumers depend on `fprime-lora_RadioTransport` and include headers using
`fprime-lora/RadioTransport/...`. Regenerate the build cache when changing
library locations.

For the current local setup, this directory is inside the STM32 reference
project. The SAMD reference project's `settings.ini` points to it through a
sibling checkout. After this directory becomes an independent repository,
update both projects' library locations to their respective library checkouts.

## Tests

The transport unit tests are registered with F Prime. From a consuming
project configured for native unit tests, build `fprime-lora_RadioTransport_ut_exe`
and run it with CTest:

```sh
cmake --build <native-ut-build-directory> --target fprime-lora_RadioTransport_ut_exe
ctest --test-dir <native-ut-build-directory> -R '^fprime-lora_RadioTransport_ut_exe$' --output-on-failure
```

The consuming project's Python environment must supply the F Prime build tools.

The transport supports a 1,024-byte frame in five radio packets with its
current 255-byte packet limit. Buffers are statically sized; measure the
linked application and stack on each target. Only link the functionality a
deployment needs, and do not assume that use of a shared library reduces RAM.
