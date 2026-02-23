# glz::ordered_small_map vs glz::ordered_map vs std::map

## Lookup (MB/s)

| n | glz::ordered_small_map | glz::ordered_map | std::map | winner |
|--:|----------------------:|-----------------:|---------:|--------|
| 8 | 304 | 256 | 141 | glz_small_map |
| 16 | 276 | 238 | 121 | glz_small_map |
| 32 | 277 | 241 | 103 | glz_small_map |
| 64 | 250 | 228 | 87 | glz_small_map |
| 128 | 251 | 224 | 77 | glz_small_map |
| 256 | 226 | 256 | 71 | glz_map |

## Insert (MB/s)

| n | glz::ordered_small_map | glz::ordered_map | std::map | winner |
|--:|----------------------:|-----------------:|---------:|--------|
| 8 | 244 | 122 | 105 | glz_small_map |
| 16 | 209 | 163 | 86 | glz_small_map |
| 32 | 267 | 195 | 79 | glz_small_map |
| 64 | 189 | 217 | 64 | glz_map |
| 128 | 161 | 249 | 58 | glz_map |
| 256 | 95 | 276 | 45 | glz_map |

## Iteration (MB/s)

| n | glz::ordered_small_map | glz::ordered_map | std::map | winner |
|--:|----------------------:|-----------------:|---------:|--------|
| 8 | 0 | 0 | 0 | glz_small_map |
| 16 | 0 | 0 | 10173 | std_map |
| 32 | 0 | 0 | 20345 | std_map |
| 64 | 0 | 0 | 13672 | std_map |
| 128 | 81380 | 81380 | 13672 | glz_small_map |
| 256 | 162760 | 162760 | 12636 | glz_small_map |
