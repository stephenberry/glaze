# glz::ordered_small_map vs glz::ordered_map vs std::map

## Lookup (MB/s)

| n | glz::ordered_small_map | glz::ordered_map | std::map | winner |
|--:|----------------------:|-----------------:|---------:|--------|
| 8 | 311 | 247 | 141 | glz_small_map |
| 16 | 277 | 243 | 121 | glz_small_map |
| 32 | 277 | 242 | 102 | glz_small_map |
| 64 | 253 | 218 | 87 | glz_small_map |
| 128 | 250 | 225 | 77 | glz_small_map |
| 256 | 226 | 259 | 71 | glz_map |

## Insert (MB/s)

| n | glz::ordered_small_map | glz::ordered_map | std::map | winner |
|--:|----------------------:|-----------------:|---------:|--------|
| 8 | 244 | 122 | 105 | glz_small_map |
| 16 | 209 | 163 | 92 | glz_small_map |
| 32 | 266 | 183 | 81 | glz_small_map |
| 64 | 195 | 225 | 71 | glz_map |
| 128 | 161 | 255 | 58 | glz_map |
| 256 | 96 | 276 | 46 | glz_map |

## Iteration (MB/s)

| n | glz::ordered_small_map | glz::ordered_map | std::map | winner |
|--:|----------------------:|-----------------:|---------:|--------|
| 8 | 0 | 0 | 0 | glz_small_map |
| 16 | 0 | 0 | 0 | glz_small_map |
| 32 | 0 | 0 | 20345 | std_map |
| 64 | 0 | 0 | 13672 | std_map |
| 128 | 81380 | 81380 | 13672 | glz_small_map |
| 256 | 162760 | 162760 | 12612 | glz_small_map |
