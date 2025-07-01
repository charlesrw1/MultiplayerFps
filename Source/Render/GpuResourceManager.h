#pragma once

class IGpuResourceManager
{
public:
	virtual void upload_texture();
	virtual void buffer_sub_data();
	virtual void upload_model_buffer();
	virtual void free_texture();
};