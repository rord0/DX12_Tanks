#ifndef SERIALIZE_H
#define SERIALIZE_H

#include "../core.h"

#define serializeint(stream, value)  if(!stream.serializeU32(value))  { return false; }
#define SerializeU8(stream, value)   if(!stream.serializeU8(value))   { return false; }
#define SerializeU16(stream, value)  if(!stream.serializeU16(value))  { return false; }
#define SerializeU32(stream, value)  if(!stream.serializeU32(value))  { return false; }
#define SerializeF32(stream, value)  if(!serializeF32(stream, value)) { return false; }
#define SerializeV2(stream, value)   if(!serializeV2(stream, value))  { return false; }
#define SerializeCStr(stream, value, bufferSize) if(!stream.serializeCStr(value, bufferSize)) { return false; }

template<typename Stream>
bool serializeF32(Stream & stream, f32 & value)
{
	if (stream.pos + sizeof(f32) > stream.size) { return false; }
	union FloatInt
	{
		f32 float_value;
		u32 int_value;
	};
	FloatInt temp;

	if (Stream::IsWriting)
	{
		temp.float_value = value;
	}
	bool result = stream.serializeU32(temp.int_value);
	if (Stream::IsReading)
	{
		value = temp.float_value;
	}
	return result;
}

template<typename Stream>
bool serializeV2(Stream & stream, vec2 & value)
{
	SerializeF32(stream, value.x);
	SerializeF32(stream, value.y);
	return true;
}

struct WriteStream
{
	u8 * buffer;
	size_t size;
	size_t pos;
    static constexpr bool IsReading = false;
    static constexpr bool IsWriting = true;

	WriteStream(u8 * buf, size_t bufSize) : buffer(buf), size(bufSize), pos(0) {}

	bool serializeU32(u32 value)
	{
		if (pos + sizeof(u32) > size) { return false; }
		*((u32*)(buffer + pos)) = value;
		pos += sizeof(u32);
		return true;
	}

	bool serializeU16(u16 value)
	{
		if (pos + sizeof(u16) > size) { return false; }
		*((u16*)(buffer + pos)) = value;
		pos += sizeof(u16);
		return true;
	}

	bool serializeU8(u8 value)
	{
		if (pos + 1 > size) { return false; }
		*(buffer + pos) = value;
		pos += 1;
		return true;
	}

	bool writeBytes(u8 * data, size_t count)
	{
		if ((pos + count) > size) { return false; }
		memcpy(buffer + pos, data, count);
		pos += count;
		return true;
	}

	bool serializeCStr(const char * str, size_t bufferSize)
	{
		size_t len = 0;
		while (len < bufferSize && str[len] != '\0') { len++; }

		if ((pos + len + sizeof(u16)) > size) { return false; }
		if (len > 65535) { return false; }
		serializeU16((u16)len);
		writeBytes((u8*)str, len);
		return true;
	}
};

struct ReadStream
{
	u8 * buffer;
	size_t size;
	size_t pos;
    static constexpr bool IsReading = true;
    static constexpr bool IsWriting = false;

	ReadStream(u8 * buf, size_t bufSize) : buffer(buf), size(bufSize), pos(0) {}

	bool serializeU32(u32 & value)
	{
		if (pos + sizeof(u32) > size) { return false; }
		value = *((u32*)(buffer + pos));
		pos += sizeof(u32);
		return true;
	}

	bool serializeU8(u8 & value)
	{
		if (pos + 1 > size) { return false; }
		value = *(buffer + pos);
		pos += 1;
		return true;
	}

	bool serializeU16(u16 & value)
	{
		if (pos + sizeof(u16) > size) { return false; }
		value = *((u16*)(buffer + pos));
		pos += sizeof(u16);
		return true;
	}

	bool readBytes(u8 * data, size_t count)
	{
		if ((pos + count) > size) { return false; }
		memcpy(data, buffer + pos, count);
		pos += count;
		return true;
	}

	bool serializeCStr(char * outStr, u16 bufferSize)
	{
		u16 len = 0;
		if (!serializeU16(len)) { return false; }
		if (len >= bufferSize)  { return false; }
		if (!readBytes((u8*)outStr, len)) { return false; }
		outStr[len] = '\0';
		return true;
	}
};

#endif // SERIALIZE_H
