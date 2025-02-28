#pragma once

// convenience wrapper
template<typename T>
class UniformBuffer
{
public:
	void init() {
		glCreateBuffers(1, &handle);
	}
	void free() {
		glDeleteBuffers(1, &handle);
	}
	void update(const T& t) {
		glNamedBufferData(handle, sizeof(T), &t, GL_DYNAMIC_DRAW);
	}
	void bind_to(int index) {
		glBindBufferBase(GL_UNIFORM_BUFFER, index, handle);
	}
	bufferhandle get_handle() const {
		return handle;
	}
private:
	bufferhandle handle = 0;
};
