# Data Recorder

`record/recorder.hpp` provides an efficient recorder for mixed data types. The template argument takes all the supported types. The recorder stores the data as a variant of deques of those types. `std::deque` is used to avoid the cost of reallocating when a `std::vector` would grow, and typically a recorder is used in cases when the length is unknown.

```c++
glz::recorder<double, float> rec;

double x = 0.0;
float y = 0.f;

rec["x"] = x;
rec["y"] = y;

for (int i = 0; i < 100; ++i) {
   x += 1.5;
   y += static_cast<float>(i);
   rec.update(); // saves the current state of x and y
}

glz::write_file_json(rec, "recorder_out.json");
```
