# BufferStream
A header-only C++20 library to quickly read objects from a buffer.

## Usage

### Open

Create a stream from any contiguous STL type:
```cpp
// Vector
std::vector buffer<unsigned char>;
BufferStream stream{buffer};

// String
std::string chars;
BufferStream stream{chars};
```

Or create a stream from a C array:
```cpp
// 1D
std::byte buffer[10] {};
BufferStream stream{buffer};

// 2D
std::byte buffer[1][1] {{{}}};
BufferStream stream{buffer};
```

Or create a stream from a raw pointer:
```cpp
std::byte* buffer = ...;
std::size_t length = ...;
BufferStream stream{buffer, length};
```

### Read

Reading can be done by value:
```cpp
auto value = stream.read<std::uint32_t>();
```

Or by reference:
```cpp
std::uint32_t value;

stream.read(value);
// -- OR --
stream >> value;
```

It's possible to read STL containers by value or reference:
```cpp
auto ints = stream.read<std::array<int, 4>>();
auto floats = stream.read<std::deque<float>>(4);

std::vector<char> chars;
stream.read(chars, 3);
// The following line pushes a char to the end of chars
stream >> chars;
```

Strings are supported:
```cpp
// Read string up to null terminator
std::string str = stream.read_string();

// Read string up to null terminator or char length
// If null terminator is encountered, stop processing chars and skip the rest of the length of the string
std::string str_specific_length = stream.read_string(12);

// Read string up to char length, including null terminators
std::string str_null_terms = stream.read_string(12, false);

// All of those functions also work by reference
std::string str_ref;
stream.read(str_ref, 12);

// Equivalent to stream.read(str_ref)
stream >> str_ref;
```

It's possible to read arrays and vectors of `std::byte` values with the earlier functions,
but convenience functions are provided since this is such a common operation:
```cpp
// Equivalent to:         bytesArray = stream.read<std::array<std::byte, 10>>();
std::array<std::byte, 10> bytesArray = stream.read_bytes<10>();

// Equivalent to:      bytesVector = stream.read<std::vector<std::byte>>(10);
std::vector<std::byte> bytesVector = stream.read_bytes(10);
```

### Seek

Seeking works similarly to how it works in an istream:
```cpp
stream.seek(0); // Seek to beginning of the stream
stream.seek(1, std::ios::beg); // Seek 1 byte ahead of the beginning of the stream
stream.seek(1, std::ios::cur); // Seek 1 byte forward
stream.seek(1, std::ios::end); // Seek 1 byte behind the end of the stream
```

Tell reports the cursor's position from the beginning of the stream:
```cpp
stream.seek(2);
stream.tell(); // Returns 2
```

Peek can read a byte from an offset relative to the cursor without advancing the cursor:
```cpp
stream.peek(); // Returns byte just ahead of the cursor
stream.peek(2); // Returns byte 2 bytes ahead of the cursor
```

Skip can skip over a given type:
```cpp
stream.skip(); // Skips 1 byte
stream.skip(2); // Skips 2 bytes

stream.skip<std::uint16_t>(); // Skips 2 bytes (uint16 is 2 bytes wide)
stream.skip<std::uint16_t>(2); // Skips 4 bytes (uint16 is 2 bytes wide)
```

### Miscellaneous

Methods that accept a reference or otherwise have no useful return value may be chained:
```cpp
stream
	.read(x)
	.skip<std::uint32_t>()
	.read(y)
	.seek(4, std::ios::end)
	.read(z);

// When read is the only method being called in the chain, this works too
stream >> x >> y >> z;
```

Attempting to read OOB will result in a `std::overflow_error` exception by default.
If an exception is thrown during a call to a method on a stream, the stream state will
not be modified. This behavior can be disabled:
```cpp
std::vector<std::byte> buffer{{}}; // size 1
BufferStream stream{buffer};

stream.setExceptionsEnabled(false);
stream.read<std::int32_t>(); // Won't throw an exception, but don't ever do this please - it's UB
```

Big-Endian conversion may be enabled as follows. Keep in mind that if big-endian support is enabled,
POD types composed of other types such as structs will need to be decomposed when reading them, or
`std::invalid_argument` will be thrown:
```cpp
const int x = 0xAB'CD'EF'00;
BufferStream stream{reinterpret_cast<const std::byte*>(&x), sizeof(int)};
stream.setBigEndian(true);
stream.read<int>(); // 0x00'EF'CD'AB

struct Vector {
	std::int64_t x;
	std::int64_t y;
};

auto vec = stream.read<Vector>(); // INCORRECT - Will throw exception!

Vector vec{};
stream >> vec.x >> vec.y; // Correct
```
