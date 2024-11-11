# BufferStream
A header-only C++20 library to quickly read/write objects from/to a buffer.

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
std::uint64_t length = ...;
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

### Write

Writing is done much the same way as reading:
```cpp
std::uint32_t value;

stream.write(value);
// -- OR --
stream << value;
```

It's possible to write from STL containers:
```cpp
std::array<int, 4> ints;
std::deque<float> floats;
...
stream << ints;
stream << floats;
```

Strings are supported:
```cpp
// Write string and null terminator
std::string str = "Hello world";
stream << str;

// Write string without null terminator
stream.write(str, false);

// Write string with null terminator, with a maximum length of 6 characters
stream.write(str, true, 6);
// Stream stores "Hello\0"
```

If writing is not desired, or creating the stream with a const pointer is required,
use the BufferStreamReadOnly class to avoid this potential impasse. It hides all
the functions that write, allowing the code to compile alright.
```cpp
const std::byte* buffer = ...;
std::uint64_t size = ...;
BufferStreamReadOnly stream{buffer, size}; // Works!
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

Peek can read an object directly after the cursor without advancing:
```cpp
stream.peek(); // Returns byte after the cursor
stream.peek<uint16_t>(); // Returns 2-byte integer after the cursor
```

At is a more advanced version of peek that can read an object from anywhere in the stream:
```cpp
stream.at(0); // Returns the 0-th byte
stream.at(1, std::ios::cur); // Returns the byte 1 byte after the cursor
stream.at<uint16_t>(0); // Returns the 2-byte integer starting at the start of the stream
stream.at<uint16_t>(-1, std::ios::cur); // Returns the 2-byte integer starting 1 byte before the cursor
```

Skip can skip over a given type:
```cpp
stream.skip(); // Skips 1 byte
stream.skip(2); // Skips 2 bytes

stream.skip<std::uint16_t>(); // Skips 2 bytes (uint16 is 2 bytes wide)
stream.skip<std::uint16_t>(2); // Skips 4 bytes (uint16 is 2 bytes wide)
```

Every seek method has a corresponding unsigned variant for convenience.
Use these methods if you are getting warnings implicitly casting a `uint64_t` to `int64_t`:

```cpp
stream.seek(-1, std::ios::cur); // Seek 1 byte backward
stream.seek_u(1u, std::ios::cur); // Seek 1 byte forward

stream.peek(-2); // Returns byte 2 bytes before the cursor
stream.peek_u(2u); // Returns byte 2 bytes after the cursor

stream.skip(-2); // Skips 2 bytes backward (equivalent to stream.seek(-2, std::ios::cur))
stream.skip_u(2u); // Skips 2 bytes forward (equivalent to stream.seek_u(2u, std::ios::cur))
```

### Miscellaneous

Methods that accept a reference or otherwise have no useful return value may be chained:
```cpp
stream
	.read(x)
	.skip<std::uint32_t>()
	.read(y)
	.seek(4, std::ios::end)
	.write(z);

// When read or write are the only methods being called in the chain, this works too
stream >> x >> y >> z << w;
```

Attempting to read OOB will result in a `std::overflow_error` exception by default. If a resize
callback is not set (it's set automatically for resizable std container types), writing OOB will
result in this exception as well. If an exception is thrown during a call to a method on a stream,
the stream state will not be modified. This behavior can be disabled:
```cpp
std::vector<std::byte> buffer{{}}; // size 1
BufferStream stream{buffer};

stream.set_exceptions_enabled(false);
stream.read<std::int32_t>(); // Won't throw an exception, but don't ever do this please - it's UB
```

Big-Endian conversion may be enabled as follows. Keep in mind that if big-endian support is enabled,
POD types composed of other types such as structs will need to be decomposed when reading or writing
them. If they are not decomposed, the data will not be modified to fit the desired endianness, and
if exceptions are enabled `std::invalid_argument` will be thrown:
```cpp
int x = 0xAB'CD'EF'00;
BufferStream stream{reinterpret_cast<std::byte*>(&x), sizeof(int)};
stream.set_big_endian(true);
stream.read<int>(); // 0x00'EF'CD'AB

struct Vector {
	std::int64_t x;
	std::int64_t y;
};

auto vec = stream.read<Vector>(); // INCORRECT - Will throw exception!

Vector vec{};
stream >> vec.x >> vec.y; // Correct
```

When writing to a std container, the stream will automatically resize the container by
powers of two when it needs more space. **Keep in mind if you are reading spans or views
over the data in the stream, they will be invalidated if the container is resized!**
Automatic resizing on writes that would otherwise exceed the capacity of the container
can be disabled by adding an argument to the constructor:
```cpp
std::vector<std::byte> buffer(32);
BufferStream stream{buffer, false};
```

Additionally, since the container size is adjusted by powers of two, do not treat the size
of the container as the size of the stream! When the program is finished writing to the stream,
resize the buffer to the stream's size before using it anywhere:
```cpp
std::vector<std::byte> buffer;
BufferStream stream{buffer};
...
// Keep in mind the stream size is measured in bytes
buffer.resize(stream.size());
...
// It may also be convenient to copy the data to a new container
std::vector<std::byte> newData = stream.seek(0).read_bytes(stream.size());
```

Files can be opened using the `FileStream` class. Keep in mind this was created for convenience,
and realistically the `BufferStream` class is better for many more use cases. See the comment at
the top of the header for a more complete list of missing features compared to `BufferStream`.
```cpp
FileStream stream{"path/to/file.bin"};
```
