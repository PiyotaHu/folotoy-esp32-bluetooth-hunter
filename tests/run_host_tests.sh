#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p /tmp/ble-finder-tests
cc -std=c11 -Wall -Wextra -Werror -Imain \
    tests/test_signal_filter.c main/signal_filter.c -o /tmp/ble-finder-tests/signal_filter
/tmp/ble-finder-tests/signal_filter
cc -std=c11 -Wall -Wextra -Werror -Imain \
    tests/test_finder_model.c main/finder_model.c main/signal_filter.c -o /tmp/ble-finder-tests/model
/tmp/ble-finder-tests/model
cc -std=c11 -Wall -Wextra -Werror -Imain \
    tests/test_finder_stability.c main/finder_model.c main/signal_filter.c -o /tmp/ble-finder-tests/stability
/tmp/ble-finder-tests/stability
cc -std=c11 -Wall -Wextra -Werror -Imain \
    tests/test_device_label.c main/device_label.c -o /tmp/ble-finder-tests/device_label
/tmp/ble-finder-tests/device_label
cc -std=c11 -Wall -Wextra -Werror -Imain \
    tests/test_finder_audio_synth.c main/finder_audio_synth.c -o /tmp/ble-finder-tests/audio
/tmp/ble-finder-tests/audio
cc -std=c11 -Wall -Wextra -Werror -Imain \
    simulator/simulator.c main/finder_model.c main/signal_filter.c -o /tmp/ble-finder-tests/simulator
/tmp/ble-finder-tests/simulator > simulator/simulation.log
python3 tools/check_glyphs.py
echo "host tests: PASS"
