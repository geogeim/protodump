// Copyright (c) 2015 by Marian Geoge, Licensed under the MIT license: http://www.opensource.org/licenses/mit-license.php

#ifndef BUFFER_H
#define BUFFER_H

#include <cstring>

struct buffer
{
	buffer(void* data, size_t size)
		: buffer(size)
	{
		copy(data, size);
	}

	buffer(size_t size)
	{
		data_ = operator new(size);
		size_ = size;
	}

	~buffer()
	{
		delete data_;
	}

	buffer(const buffer& other)
		: buffer(other.size_)
	{
		copy(other);
	}

	buffer(buffer&& other)
	{
		data_ = other.data_;
		size_ = other.size_;

		other.data_ = nullptr;
		other.size_ = 0;
	}


	buffer& operator=(const buffer& other)
	{
		delete data_;
		copy(other);
		return *this;
	}
	buffer& operator=(buffer&& other)
	{
		data_ = other.data_;
		size_ = other.size_;

		other.data_ = nullptr;
		other.size_ = 0;

		return *this;
	}

	void* data() const
	{
		return data_;
	}

	size_t size() const
	{
		return size_;
	}

	size_t copy(void* data, size_t size)
	{
		size_ = size_ < size ? size_ : size;
		std::memcpy(data_, data, size_);
		return size_;
	}

	void copy(const buffer& other)
	{
		copy(other.data_, other.size_);
	}

	void* reset(size_t size)
	{
		delete data_;
		*this = buffer(size);

		return data_;
	}

private:
	void* data_;
	size_t size_;
};

#endif // !BUFFER_H
