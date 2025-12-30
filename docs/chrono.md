# std::chrono Support

Glaze provides first-class support for `std::chrono` types, enabling seamless serialization and deserialization of durations and time points.

## Supported Types

### Durations

All `std::chrono::duration` types are supported and serialize as their numeric count value:

```cpp
std::chrono::milliseconds ms{12345};
std::string json = glz::write_json(ms).value();  // "12345"

std::chrono::milliseconds parsed{};
glz::read_json(parsed, json);  // parsed.count() == 12345
```

This works with any duration type, including custom periods:

```cpp
std::chrono::seconds s{3600};           // "3600"
std::chrono::nanoseconds ns{123456789}; // "123456789"
std::chrono::hours h{24};               // "24"

// Custom period (e.g., 60fps frames)
using frames = std::chrono::duration<int64_t, std::ratio<1, 60>>;
frames f{120};  // "120"

// Floating-point rep
std::chrono::duration<double, std::milli> ms{123.456};  // "123.456"
```

### System Clock Time Points

`std::chrono::system_clock::time_point` serializes as an ISO 8601 string in UTC:

```cpp
auto now = std::chrono::system_clock::now();
std::string json = glz::write_json(now).value();
// "2024-12-13T15:30:45.123456789Z"
```

The precision of fractional seconds matches the time point's duration type:

```cpp
using namespace std::chrono;

sys_time<seconds> tp_sec{...};      // "2024-12-13T15:30:45Z"
sys_time<milliseconds> tp_ms{...};  // "2024-12-13T15:30:45.123Z"
sys_time<microseconds> tp_us{...};  // "2024-12-13T15:30:45.123456Z"
sys_time<nanoseconds> tp_ns{...};   // "2024-12-13T15:30:45.123456789Z"
```

Parsing supports multiple ISO 8601 formats:

```cpp
std::chrono::system_clock::time_point tp;

// UTC with Z suffix
glz::read_json(tp, "\"2024-12-13T15:30:45Z\"");

// With timezone offset
glz::read_json(tp, "\"2024-12-13T15:30:45+05:00\"");
glz::read_json(tp, "\"2024-12-13T15:30:45-08:00\"");

// With fractional seconds
glz::read_json(tp, "\"2024-12-13T15:30:45.123456789Z\"");
```

### Steady Clock Time Points

`std::chrono::steady_clock::time_point` serializes as a numeric count (time since epoch), since steady clock's epoch is implementation-defined and not meaningful as a calendar time:

```cpp
auto start = std::chrono::steady_clock::now();
std::string json = glz::write_json(start).value();  // numeric count

std::chrono::steady_clock::time_point parsed;
glz::read_json(parsed, json);
// Exact roundtrip - no precision loss
```

## Epoch Time Wrappers

For APIs that expect Unix timestamps (numeric epoch time) instead of ISO 8601 strings, Glaze provides `epoch_time<Duration>` wrappers:

```cpp
#include "glaze/glaze.hpp"

// Convenience aliases
glz::epoch_seconds  // Unix timestamp in seconds
glz::epoch_millis   // Unix timestamp in milliseconds
glz::epoch_micros   // Unix timestamp in microseconds
glz::epoch_nanos    // Unix timestamp in nanoseconds
```

### Usage

```cpp
glz::epoch_seconds ts;
ts.value = std::chrono::system_clock::now();

std::string json = glz::write_json(ts).value();  // "1702481400"

glz::epoch_seconds parsed;
glz::read_json(parsed, json);
```

### Implicit Conversion

`epoch_time` wrappers implicitly convert to/from `system_clock::time_point`:

```cpp
glz::epoch_millis ts = std::chrono::system_clock::now();
std::chrono::system_clock::time_point tp = ts;
```

### In Structs

Use epoch wrappers when your API requires numeric timestamps:

```cpp
struct ApiResponse {
    std::string data;
    glz::epoch_millis created_at;  // Serializes as: 1702481400123
    glz::epoch_millis updated_at;
};

struct Event {
    std::string name;
    std::chrono::system_clock::time_point timestamp;  // ISO 8601 string
    std::chrono::milliseconds duration;               // Numeric count
};
```

## Complete Example

```cpp
#include "glaze/glaze.hpp"
#include <chrono>

struct LogEntry {
    std::string message;
    std::chrono::system_clock::time_point timestamp;
    std::chrono::microseconds processing_time;
};

struct MetricsReport {
    glz::epoch_millis generated_at;
    std::chrono::steady_clock::time_point uptime_start;
    std::vector<LogEntry> entries;
};

int main() {
    MetricsReport report;
    report.generated_at = std::chrono::system_clock::now();
    report.uptime_start = std::chrono::steady_clock::now();
    report.entries.push_back({
        "Request processed",
        std::chrono::system_clock::now(),
        std::chrono::microseconds{1234}
    });

    // Serialize
    std::string json = glz::write_json(report).value();

    // Deserialize
    MetricsReport parsed;
    glz::read_json(parsed, json);
}
```

Output:

```json
{
    "generated_at": 1702481400123,
    "uptime_start": 123456789012345,
    "entries": [
        {
            "message": "Request processed",
            "timestamp": "2024-12-13T15:30:45.123456Z",
            "processing_time": 1234
        }
    ]
}
```

## Summary Table

| Type | JSON Format | Example |
|------|-------------|---------|
| `std::chrono::duration<Rep, Period>` | Numeric count | `12345` |
| `std::chrono::system_clock::time_point` | ISO 8601 string | `"2024-12-13T15:30:45Z"` |
| `std::chrono::steady_clock::time_point` | Numeric count | `123456789012345` |
| `glz::epoch_seconds` | Unix seconds | `1702481400` |
| `glz::epoch_millis` | Unix milliseconds | `1702481400123` |
| `glz::epoch_micros` | Unix microseconds | `1702481400123456` |
| `glz::epoch_nanos` | Unix nanoseconds | `1702481400123456789` |

## Notes

- **Thread Safety**: All chrono serialization is fully thread-safe with no static buffers
- **Precision**: Roundtrip serialization preserves full precision for all supported types
- **Validation**: Invalid ISO 8601 strings return `glz::error_code::parse_error`
- **Timezone Handling**: Time points are always converted to/from UTC; timezone offsets in input are properly applied
- **Leap Seconds**: Not supported (`23:59:60` will return a parse error). `std::chrono::system_clock` uses Unix time which does not account for leap seconds

## TOML Datetime Support

TOML has native datetime types (not quoted strings). When using `glz::write_toml` / `glz::read_toml`, chrono types use TOML's native datetime format:

- `system_clock::time_point` → TOML Offset Date-Time (`2024-06-15T10:30:45Z`)
- `year_month_day` → TOML Local Date (`2024-06-15`)
- `hh_mm_ss<Duration>` → TOML Local Time (`10:30:45.123`)
- Durations and other time points → Numeric values

See [TOML Documentation](./toml.md#datetime-support) for full details.
