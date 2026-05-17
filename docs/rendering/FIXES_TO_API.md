
# documents

docs/rendering/gfx_abstraction.md
C:\Users\charl\.claude\plans\i-want-to-put-idempotent-fairy.md

# instructions 

stop and fix. this is the SDL3 gpu api: https://github.com/libsdl-org/SDL/blob/main/include/SDL3/SDL_gpu.h

I am writing an abstraction layer over it. Which will then have an impl for opengl and sdl3 backend.

So IGraphicsDevice must abstract both neatly. Doesnt have to exactly conform. I want to keep my "dynamic-like" pipeline state abstraction. Where I fill out the struct with pipeline data so its sort of like a "pipeline object" not statemachine, then I "Bind" it. The Opengl backend will do what the code does now, set each state (with invalid bit etc). The SDL3 backend will ahve to do some pipeline caching underneath the abstraction.

some issues im noticing with IGraphicsDevice:
	-remove the set_clear_color. in SDL3gpu it goes on SDL_GPUColorTargetInfo. Mirror this with my abstraction, put it on color target info. Not statemachine "set_clear_color"
	
	```
	"	// Bind a sub-range of a raw GL buffer to an SSBO slot. Wraps
	// glBindBufferRange(GL_SHADER_STORAGE_BUFFER, …). The _raw suffix flags
	// that the buffer still lives as a raw `bufferhandle`; folds into
	// IGraphicsBuffer* in a later sub-phase.
	virtual void bind_storage_buffer_range_raw(int slot, uint32_t buffer_handle,
											   int offset, int size) = 0;

	// Bind a raw GL buffer to GL_DRAW_INDIRECT_BUFFER (handle 0 unbinds).
	// _raw escape for sites whose buffer is still a `bufferhandle`.
	virtual void bind_indirect_buffer_raw(uint32_t buffer_handle) = 0;

	// glNamedBufferData on a raw buffer handle. _raw escape for sites whose
	// buffer is still a `bufferhandle`; folds into IGraphicsBuffer::upload
	// (and Phase 2e's ring buffer) once those buffers migrate.
	virtual void upload_buffer_raw(uint32_t buffer_handle, int size,
								   const void* data) = 0;

	// glNamedBufferSubData on a raw buffer handle. _raw escape; see above.
	virtual void sub_upload_buffer_raw(uint32_t buffer_handle, int offset,
									   int size, const void* data) = 0;"
	```
	
	-all of this shouldn't be there. NO more raw buffer handles...they should be wrapped with IGraphicsBuffer*.
	
	```virtual void set_mip_range(IGraphicsTexture* tex, int base, int max) = 0;, virtual void download_texture_2d(IGraphicsTexture* tex, int mip,
								 void* dest, int dest_size_bytes) = 0;
	```
	
	-this should be a function on the IGraphicsTexture* object, not global device.
	
	- set_scissor can stay since that is in SDL3 gpu api.
	
