# MangoHud tests

`mangoapp_wire_time_test.cpp` is a standalone raw-wire regression check for the
MangoApp packed message decode. It writes raw bytes at the numeric wire offsets
of the 95-byte `mangoapp_msg_v1` message (87-byte SysV payload), `memcpy`s them
into the actual MangoHud header struct, and asserts the decoded fields.

The Gamescope `0037-fifo-display-identity-hud` sender publishes the paired
layout: offset 16 is the visible/app-presented frame time and offset 26 is the
application/debug frame time. The test pins that pairing against the receiver
names in `src/app/mangoapp_proto.h` of the upstream MangoHud 0.8.4 packaged
here, which uses the published field order without the removed `0010` receiver
field-swap; the other MangoHud patches are unchanged.

It uses only the standard library plus the real header; there is no test
framework, no new dependency and no mirrored protocol struct. The asserts
assume LP64 (`long` is 8 bytes), as in the Linux Docker test environment.

## Build and run

Compile against the actual applied MangoHud header, not a copy. `MANGOHUD_SOURCE`
is that checkout and `TASK_OUTPUT` an existing output directory.

```sh
MANGOHUD_SOURCE=/path/to/applied/MangoHud
TASK_OUTPUT=/path/to/task/output

g++ -std=c++17 \
    -I "$MANGOHUD_SOURCE/src/app" \
    packages/mangohud/tests/mangoapp_wire_time_test.cpp \
    -o "$TASK_OUTPUT/mangoapp_wire_time_test"
"$TASK_OUTPUT/mangoapp_wire_time_test"
```

Run from the repository root so the source path above resolves. No download,
build system or extra tooling is required.

This is a raw-wire check only; it does not validate rendering, HUD output or
performance.

## Battery percentage

`battery_percent_test.cpp` links the real `src/battery.cpp` and creates
sysfs-shaped fixtures below an explicit output directory. It checks the
single-battery capacity reading, raw charge/energy fallbacks, percentage bounds,
invalid denominators, and the existing two-battery aggregation.

```sh
g++ -std=c++17 -include vector \
    -I "$MANGOHUD_SOURCE/src" -I "$MANGOHUD_SOURCE/include" \
    packages/mangohud/tests/battery_percent_test.cpp \
    "$MANGOHUD_SOURCE/src/battery.cpp" -lspdlog -lfmt \
    -o "$TASK_OUTPUT/battery_percent_test"
"$TASK_OUTPUT/battery_percent_test" "$TASK_OUTPUT"
```

Use a disposable output directory; the test owns its
`battery_percent_fixture` subdirectory. It does not read the host battery.
