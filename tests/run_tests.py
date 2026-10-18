"""Host tests: Python 3 + GCC/G++; no radio, Arduino or STM32 required."""
import json
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix="sensor-tests-") as tmp:
    for compiler, source, standard in [("gcc", "protocol_test.c", "c99"),
                                       ("g++", "receiver_test.cpp", "c++11")]:
        exe = Path(tmp) / (source + ".exe")
        subprocess.run([compiler, f"-std={standard}", "-Wall", "-Wextra", "-Werror",
                        "-Wno-missing-field-initializers", "-I", str(ROOT / "tests/stubs"),
                        str(ROOT / "tests" / source), "-o", str(exe)], check=True)
        result = subprocess.run([str(exe)], check=True, capture_output=True, text=True)
        if source.endswith(".cpp"):
            rows = dict(line.split(" ", 1) for line in result.stdout.splitlines())
            rows = {name: json.loads(body) for name, body in rows.items()}

fields = ("temperature_c", "humidity_pct", "pressure_pa", "light_lux", "eco2_ppm")
for name in ("boot", "stale", "legacy", "wrap_stale", "latched_stale", "saturated_age", "all_failed"):
    assert all(rows[name][field] is None and not rows[name]["valid"][field] for field in fields), name
assert rows["boot"]["has_sample"] is False and rows["boot"]["age_ms"] is None
assert rows["valid"]["temperature_c"] == 25.3 and rows["valid"]["pressure_pa"] == 101325
assert rows["valid"]["light_lux"] == 0 and rows["valid"]["valid"]["light_lux"]
assert rows["valid"]["sequence"] == 7 and rows["valid"]["age_ms"] == 100
assert rows["boundary"]["age_ms"] == 10000 and not rows["boundary"]["stale"]
assert rows["stale"]["stale"] and rows["stale"]["source_valid_mask"] == 31
assert rows["partial"]["light_lux"] == 0 and rows["partial"]["temperature_c"] is None
assert rows["malformed"]["valid_packets"] == 2 and rows["malformed"]["invalid_packets"] == 1
assert rows["legacy"]["protocol_version"] == 0 and rows["legacy"]["source_valid_mask"] is None
assert rows["wrap"]["age_ms"] == 151 and not rows["wrap"]["stale"]
assert rows["saturated_age"]["age_ms"] == 4294967295
assert not rows["all_failed"]["stale"] and rows["all_failed"]["source_valid_mask"] == 0
print(f"PASS: C protocol tests and {len(rows)} receiver/API scenarios")
