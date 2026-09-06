# Build output only: embed the pinned data, including dictionary break rules.
# Splitting the initializer into lines also keeps MSVC's line parser bounded.
file(READ "${INPUT}" _data HEX)
file(WRITE "${OUTPUT}" "alignas(16) extern const unsigned char oneuiEmbeddedIcuData[] = {\n")
string(LENGTH "${_data}" _length)
math(EXPR _last "${_length} - 1")
foreach(_offset RANGE 0 ${_last} 2048)
    string(SUBSTRING "${_data}" ${_offset} 2048 _chunk)
    string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," _chunk "${_chunk}")
    file(APPEND "${OUTPUT}" "${_chunk}\n")
endforeach()
file(APPEND "${OUTPUT}" "};\n")
