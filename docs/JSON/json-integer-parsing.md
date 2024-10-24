# JSON Integer Parsing

JSON integer parsing in Glaze rejects negative exponents and decimal values.

Valid examples for a `uint8_t`:

```
0
123
255
1e1
1e+2
```

Invalid examples for a `uint8_t`:

```
0.0 ** decimal
256 ** out of range
1.0 ** decimal
1e-2 ** negative exponent
```

This applies for larger integer types as well, such as `int64_t`.

Glaze rejects negative exponents and decimal values for the following reasons:

- Fractional values cannot be stored exactly in integers, so data loss can be unknown to the developer.
- Fractional values should be rounded in different ways or truncated when converted to integers based on the application. It is better to parse these values as floating point values and then apply specific conversions.
- The parser runs faster
- We are able to disambiguate between values that can be stored in integer types and floating point types, which allows auto variant deduction for variants containing both integers and floating point values.
